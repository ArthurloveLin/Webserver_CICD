#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"
#include "../blog/blog_handler.h"
#include <unordered_map>
#include <random>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>

// Session信息结构
struct UserSession {
    string username;
    string role;
    time_t created_at;
    time_t last_access;
    
    static const time_t SESSION_TIMEOUT = 7200; // 2小时超时
    
    UserSession() : created_at(0), last_access(0) {}
    UserSession(const string& user, const string& user_role) 
        : username(user), role(user_role) {
        created_at = time(nullptr);
        last_access = created_at;
    }
    
    bool is_expired() const {
        return (time(nullptr) - last_access) > SESSION_TIMEOUT;
    }
    
    void update_access_time() {
        last_access = time(nullptr);
    }
};

class http_conn
{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        LOGIN_SUCCESS,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
    void close_conn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    void initmysql_result(connection_pool *connPool);
    static void init_blog_handler(connection_pool *connPool);
    
    // Session管理功能
    static string create_session(const string& username, const string& role);
    static bool validate_session(const string& session_id);
    static UserSession* get_session(const string& session_id);
    static void destroy_session(const string& session_id);
    static void cleanup_expired_sessions();
    static string get_session_from_cookie(const string& cookie_header);
    static string get_session_username(const string& session_id);
    static string get_session_role(const string& session_id);
    
    // 密码加密功能
    static string generate_salt(int length = 16);
    static string hash_password(const string& password, const string& salt);
    static bool verify_password(const string& password, const string& stored_hash);
    static string extract_salt_from_hash(const string& stored_hash);
    
    int timer_flag;
    int improv;


private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_start_line; };
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
    bool add_cookie(const string& name, const string& value, int max_age = 3600);

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;
    int m_state;  //读为0, 写为1

private:
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    long m_read_idx;
    long m_checked_idx;
    int m_start_line;
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    CHECK_STATE m_check_state;
    METHOD m_method;
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    char *m_cookie;
    long m_content_length;
    bool m_linger;
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;        //是否启用的POST
    char *m_string; //存储请求头数据
    int bytes_to_send;
    int bytes_have_send;
    char *doc_root;

    map<string, string> m_users;
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
    
    // 登录成功时的用户信息
    string login_username;
    string login_role;
    
    static BlogHandler* blog_handler;
    
    // Session存储 - 静态成员
    static unordered_map<string, UserSession> sessions;
    static locker session_lock;
};

#endif
