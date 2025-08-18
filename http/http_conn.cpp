#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string, string> users; // 保持原有的用户名密码映射
map<string, string> user_roles; // 新增：用户名到角色的映射
BlogHandler* http_conn::blog_handler = nullptr;

// Session管理静态成员定义
unordered_map<string, UserSession> http_conn::sessions;
locker http_conn::session_lock;

void http_conn::initmysql_result(connection_pool *connPool)
{
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd，role数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd,role FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名、密码和角色，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]); // username
        string temp2(row[1]); // passwd
        string temp3(row[2]); // role
        users[temp1] = temp2;
        user_roles[temp1] = temp3;
    }
}

void http_conn::init_blog_handler(connection_pool *connPool)
{
    if (blog_handler == nullptr)
    {
        blog_handler = new BlogHandler();
        blog_handler->init(connPool);
        // 设置session验证函数
        BlogHandler::set_session_functions(validate_session, get_session_username, get_session_role);
        printf("Blog handler initialized\n");
    }
}

//对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//从内核时间表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

//关闭连接，关闭一个连接，客户总量减一
void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

//初始化连接,外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                     int close_log, string user, string passwd, string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_count++;

    //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}

//初始化新接受的连接
//check_state默认为分析请求行状态
void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_cookie = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')
        {
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

    //LT读取数据
    if (0 == m_TRIGMode)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        m_read_idx += bytes_read;

        if (bytes_read <= 0)
        {
            return false;
        }

        return true;
    }
    //ET读数据
    else
    {
        while (true)
        {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if (bytes_read == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            else if (bytes_read == 0)
            {
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
}

//解析http请求行，获得请求方法，目标url及http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    m_url = strpbrk(text, " \t");
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else if (strcasecmp(method, "PUT") == 0)
    {
        m_method = PUT;
        cgi = 1;
    }
    else if (strcasecmp(method, "DELETE") == 0)
    {
        m_method = DELETE;
        cgi = 1;
    }
    else
        return BAD_REQUEST;
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    //当url为/时，显示判断界面
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//解析http请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else if (strncasecmp(text, "Cookie:", 7) == 0)
    {
        text += 7;
        text += strspn(text, " \t");
        m_cookie = text;
    }
    else
    {
        LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}

//判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    //printf("m_url:%s\n", m_url);
    const char *p = strrchr(m_url, '/');

    // 处理博客路由
    if (blog_handler && strncmp(m_url, "/blog", 5) == 0)
    {
        string method_str;
        switch (m_method)
        {
        case GET:
            method_str = "GET";
            break;
        case POST:
            method_str = "POST";
            break;
        case PUT:
            method_str = "PUT";
            break;
        case DELETE:
            method_str = "DELETE";
            break;
        default:
            method_str = "GET";
            break;
        }
        
        string url_str(m_url);
        string post_data_str = m_string ? string(m_string) : "";
        string client_ip = inet_ntoa(m_address.sin_addr);
        
        string cookie_str = m_cookie ? string(m_cookie) : "";
        string blog_response = blog_handler->handle_request(method_str, url_str, post_data_str, client_ip, cookie_str);
        
        if (!blog_response.empty())
        {
            // 将博客响应写入临时文件并使用mmap
            char temp_file[] = "/tmp/blog_response_XXXXXX";
            int temp_fd = mkstemp(temp_file);
            if (temp_fd != -1)
            {
                ssize_t written = ::write(temp_fd, blog_response.c_str(), blog_response.length());
                close(temp_fd);
                
                if (written > 0 && stat(temp_file, &m_file_stat) >= 0)
                {
                    int fd = open(temp_file, O_RDONLY);
                    if (fd >= 0)
                    {
                        m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
                        close(fd);
                        // 注意：不立即删除临时文件，让unmap时处理
                        return FILE_REQUEST;
                    }
                }
                unlink(temp_file);
            }
            return INTERNAL_ERROR;
        }
        return NO_RESOURCE;
    }

    // 处理静态文件请求
    if (strncmp(m_url, "/static", 7) == 0) {
        // 构建静态文件路径
        string static_path = string(doc_root) + string(m_url);
        strncpy(m_real_file, static_path.c_str(), FILENAME_LEN - 1);
        
        // 检查文件是否存在
        if (stat(m_real_file, &m_file_stat) < 0) {
            return NO_RESOURCE;
        }
        
        if (!(m_file_stat.st_mode & S_IROTH)) {
            return FORBIDDEN_REQUEST;
        }
        
        if (S_ISDIR(m_file_stat.st_mode)) {
            return BAD_REQUEST;
        }
        
        int fd = open(m_real_file, O_RDONLY);
        m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        return FILE_REQUEST;
    }

    //处理cgi
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        if (*(p + 1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据，使用加密密码
            string salt = generate_salt();
            string hashed_password = hash_password(password, salt);
            
            char *sql_insert = (char *)malloc(sizeof(char) * 300);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, hashed_password.c_str());
            strcat(sql_insert, "')");

            if (users.find(name) == users.end())
            {
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, hashed_password));
                user_roles[name] = "guest"; // 新注册用户默认为guest
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && verify_password(password, users[name]))
            {
                // 登录成功，存储用户信息并返回特殊状态码
                login_username = string(name);
                login_role = user_roles[name];
                strcpy(m_url, "/welcome.html");
                // 不在这里直接设置m_url，而是返回特殊状态码让process_write处理
            }
            else
                strcpy(m_url, "/logError.html");
        }
    }

    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        // 原fans.html已改为弹出窗口，重定向到welcome页面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/welcome.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    
    // 检查是否是登录成功情况
    if (!login_username.empty() && strcmp(m_url, "/welcome.html") == 0) {
        return LOGIN_SUCCESS;
    }
    
    return FILE_REQUEST;
}
void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
bool http_conn::write()
{
    int temp = 0;

    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}
bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);

    LOG_INFO("request:%s", m_write_buf);

    return true;
}
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() &&
           add_blank_line();
}
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}
bool http_conn::add_content_type()
{
    // 根据文件扩展名设置正确的Content-Type
    const char* ext = strrchr(m_real_file, '.');
    const char* content_type = "text/html";
    
    if (ext) {
        if (strcmp(ext, ".pdf") == 0) {
            content_type = "application/pdf";
        }
        else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
            content_type = "image/jpeg";
        }
        else if (strcmp(ext, ".png") == 0) {
            content_type = "image/png";
        }
        else if (strcmp(ext, ".gif") == 0) {
            content_type = "image/gif";
        }
        else if (strcmp(ext, ".css") == 0) {
            content_type = "text/css; charset=utf-8";
        }
        else if (strcmp(ext, ".js") == 0) {
            content_type = "application/javascript; charset=utf-8";
        }
        else if (strcmp(ext, ".mp4") == 0) {
            content_type = "video/mp4";
        }
    }
    
    return add_response("Content-Type:%s\r\n", content_type);
}
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool http_conn::add_cookie(const string& name, const string& value, int max_age)
{
    return add_response("Set-Cookie: %s=%s; Max-Age=%d; Path=/\r\n", 
                       name.c_str(), value.c_str(), max_age);
}
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_content_type();
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    case LOGIN_SUCCESS:
    {
        // 创建session并设置cookie
        string session_id = create_session(login_username, login_role);
        
        add_status_line(200, ok_200_title);
        add_content_type();
        add_cookie("session_id", session_id, SESSION_TIMEOUT);
        
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            
            // 清理登录信息
            login_username.clear();
            login_role.clear();
            
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string)) {
                login_username.clear();
                login_role.clear();
                return false;
            }
            login_username.clear();
            login_role.clear();
        }
        break;
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}
void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}

// Session管理功能实现
string http_conn::create_session(const string& username, const string& role) {
    // 生成session ID
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<> dis(0, 15);
    
    string session_id = "";
    for (int i = 0; i < 32; i++) {
        int hex_val = dis(gen);
        if (hex_val < 10) {
            session_id += char('0' + hex_val);
        } else {
            session_id += char('a' + hex_val - 10);
        }
    }
    
    // 存储session
    session_lock.lock();
    sessions[session_id] = UserSession(username, role);
    session_lock.unlock();
    
    return session_id;
}

bool http_conn::validate_session(const string& session_id) {
    if (session_id.empty()) return false;
    
    session_lock.lock();
    auto it = sessions.find(session_id);
    if (it != sessions.end()) {
        // 检查是否过期
        time_t now = time(nullptr);
        if (now - it->second.last_access > SESSION_TIMEOUT) {
            sessions.erase(it);
            session_lock.unlock();
            return false;
        }
        // 更新最后访问时间
        it->second.last_access = now;
        session_lock.unlock();
        return true;
    }
    session_lock.unlock();
    return false;
}

UserSession* http_conn::get_session(const string& session_id) {
    if (!validate_session(session_id)) return nullptr;
    
    session_lock.lock();
    auto it = sessions.find(session_id);
    UserSession* session = (it != sessions.end()) ? &it->second : nullptr;
    session_lock.unlock();
    
    return session;
}

void http_conn::destroy_session(const string& session_id) {
    session_lock.lock();
    sessions.erase(session_id);
    session_lock.unlock();
}

void http_conn::cleanup_expired_sessions() {
    time_t now = time(nullptr);
    session_lock.lock();
    
    auto it = sessions.begin();
    while (it != sessions.end()) {
        if (now - it->second.last_access > SESSION_TIMEOUT) {
            it = sessions.erase(it);
        } else {
            ++it;
        }
    }
    
    session_lock.unlock();
}

string http_conn::get_session_from_cookie(const string& cookie_header) {
    if (cookie_header.empty()) return "";
    
    // 查找 session_id=xxx 格式
    size_t pos = cookie_header.find("session_id=");
    if (pos == string::npos) return "";
    
    pos += 11; // strlen("session_id=")
    size_t end_pos = cookie_header.find(";", pos);
    if (end_pos == string::npos) {
        end_pos = cookie_header.length();
    }
    
    return cookie_header.substr(pos, end_pos - pos);
}

string http_conn::get_session_username(const string& session_id) {
    UserSession* session = get_session(session_id);
    return session ? session->username : "";
}

string http_conn::get_session_role(const string& session_id) {
    UserSession* session = get_session(session_id);
    return session ? session->role : "guest";
}

// 密码加密功能实现
string http_conn::generate_salt(int length) {
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<> dis(0, sizeof(charset) - 2);
    
    string salt;
    for (int i = 0; i < length; i++) {
        salt += charset[dis(gen)];
    }
    return salt;
}

string http_conn::hash_password(const string& password, const string& salt) {
    string salted_password = salt + password;
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)salted_password.c_str(), salted_password.length(), hash);
    
    stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    
    // 返回格式：salt$hash
    return salt + "$" + ss.str();
}

bool http_conn::verify_password(const string& password, const string& stored_hash) {
    // 如果stored_hash不包含$，说明是旧的明文密码，直接比较
    size_t dollar_pos = stored_hash.find('$');
    if (dollar_pos == string::npos) {
        // 明文比较（兼容旧密码）
        return password == stored_hash;
    }
    
    // 提取salt
    string salt = stored_hash.substr(0, dollar_pos);
    
    // 计算password的hash
    string computed_hash = hash_password(password, salt);
    
    return computed_hash == stored_hash;
}

string http_conn::extract_salt_from_hash(const string& stored_hash) {
    size_t dollar_pos = stored_hash.find('$');
    if (dollar_pos == string::npos) {
        return ""; // 明文密码没有salt
    }
    return stored_hash.substr(0, dollar_pos);
}
