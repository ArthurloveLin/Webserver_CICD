#include <gtest/gtest.h>
#include "../http/http_conn.h"
#include "../CGImysql/sql_connection_pool.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cstring>
#include <vector>

using namespace std;

// 测试访问器类，作为http_conn的友元来访问私有成员
class HttpConnTestAccessor {
public:
    // 访问私有成员的静态方法
    static int& get_sockfd(http_conn& conn) { return conn.m_sockfd; }
    static sockaddr_in& get_address(http_conn& conn) { return conn.m_address; }
    static http_conn::CHECK_STATE& get_check_state(http_conn& conn) { return conn.m_check_state; }
    static http_conn::METHOD& get_method(http_conn& conn) { return conn.m_method; }
    static bool& get_linger(http_conn& conn) { return conn.m_linger; }
    static long& get_read_idx(http_conn& conn) { return conn.m_read_idx; }
    static int& get_write_idx(http_conn& conn) { return conn.m_write_idx; }
    static long& get_checked_idx(http_conn& conn) { return conn.m_checked_idx; }
    static long& get_content_length(http_conn& conn) { return conn.m_content_length; }
    static char* get_read_buf(http_conn& conn) { return conn.m_read_buf; }
    static char* get_write_buf(http_conn& conn) { return conn.m_write_buf; }
    static char*& get_url(http_conn& conn) { return conn.m_url; }
    static char*& get_version(http_conn& conn) { return conn.m_version; }
    static char*& get_host(http_conn& conn) { return conn.m_host; }
    static int& get_cgi(http_conn& conn) { return conn.cgi; }
    
    // 访问私有方法的静态方法
    static http_conn::HTTP_CODE call_parse_request_line(http_conn& conn, char* text) {
        return conn.parse_request_line(text);
    }
    static http_conn::HTTP_CODE call_parse_headers(http_conn& conn, char* text) {
        return conn.parse_headers(text);
    }
    static http_conn::LINE_STATUS call_parse_line(http_conn& conn) {
        return conn.parse_line();
    }
    static bool call_add_status_line(http_conn& conn, int status, const char* title) {
        return conn.add_status_line(status, title);
    }
    static bool call_add_headers(http_conn& conn, int content_length) {
        return conn.add_headers(content_length);
    }
    static bool call_add_content(http_conn& conn, const char* content) {
        return conn.add_content(content);
    }
    static bool call_add_cookie(http_conn& conn, const string& name, const string& value, int max_age) {
        return conn.add_cookie(name, value, max_age);
    }
};

class HttpConnTest : public ::testing::Test {
protected:
    void SetUp() override {
        http_conn::m_epollfd = epoll_create(5);
        http_conn::m_user_count = 0;
        
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sin_family = AF_INET;
        client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        client_addr.sin_port = htons(8080);
        
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GT(sockfd, 0);
    }
    
    void TearDown() override {
        if (sockfd > 0) {
            close(sockfd);
        }
        if (http_conn::m_epollfd > 0) {
            close(http_conn::m_epollfd);
        }
    }
    
    http_conn conn;
    int sockfd;
    sockaddr_in client_addr;
};

TEST_F(HttpConnTest, InitializationTest) {
    string root = "/var/www/html";
    string user = "testuser";
    string passwd = "testpass";
    string sqlname = "testdb";
    
    conn.init(sockfd, client_addr, const_cast<char*>(root.c_str()), 
             0, 0, user, passwd, sqlname);
    
    EXPECT_EQ(HttpConnTestAccessor::get_sockfd(conn), sockfd);
    EXPECT_EQ(memcmp(&HttpConnTestAccessor::get_address(conn), &client_addr, sizeof(sockaddr_in)), 0);
    EXPECT_EQ(HttpConnTestAccessor::get_check_state(conn), http_conn::CHECK_STATE_REQUESTLINE);
    EXPECT_EQ(HttpConnTestAccessor::get_method(conn), http_conn::GET);
    EXPECT_FALSE(HttpConnTestAccessor::get_linger(conn));
    EXPECT_EQ(HttpConnTestAccessor::get_read_idx(conn), 0);
    EXPECT_EQ(HttpConnTestAccessor::get_write_idx(conn), 0);
}

TEST_F(HttpConnTest, ParseRequestLineValidGetRequest) {
    conn.init(sockfd, client_addr, const_cast<char*>("/var/www/html"), 0, 0, "user", "pass", "db");
    
    char request_line[] = "GET /index.html HTTP/1.1";
    http_conn::HTTP_CODE result = HttpConnTestAccessor::call_parse_request_line(conn, request_line);
    
    EXPECT_EQ(result, http_conn::NO_REQUEST);
    EXPECT_EQ(HttpConnTestAccessor::get_method(conn), http_conn::GET);
    EXPECT_STREQ(HttpConnTestAccessor::get_url(conn), "/index.html");
    EXPECT_STREQ(HttpConnTestAccessor::get_version(conn), "HTTP/1.1");
}

TEST_F(HttpConnTest, ParseRequestLineValidPostRequest) {
    conn.init(sockfd, client_addr, const_cast<char*>("/var/www/html"), 0, 0, "user", "pass", "db");
    
    char request_line[] = "POST /api/login HTTP/1.1";
    http_conn::HTTP_CODE result = HttpConnTestAccessor::call_parse_request_line(conn, request_line);
    
    EXPECT_EQ(result, http_conn::NO_REQUEST);
    EXPECT_EQ(HttpConnTestAccessor::get_method(conn), http_conn::POST);
    EXPECT_STREQ(HttpConnTestAccessor::get_url(conn), "/api/login");
    EXPECT_EQ(HttpConnTestAccessor::get_cgi(conn), 1);
}

TEST_F(HttpConnTest, ParseRequestLineInvalidMethod) {
    conn.init(sockfd, client_addr, const_cast<char*>("/var/www/html"), 0, 0, "user", "pass", "db");
    
    char request_line[] = "INVALID /index.html HTTP/1.1";
    http_conn::HTTP_CODE result = HttpConnTestAccessor::call_parse_request_line(conn, request_line);
    
    EXPECT_EQ(result, http_conn::BAD_REQUEST);
}

TEST_F(HttpConnTest, ParseRequestLineInvalidVersion) {
    conn.init(sockfd, client_addr, const_cast<char*>("/var/www/html"), 0, 0, "user", "pass", "db");
    
    char request_line[] = "GET /index.html HTTP/2.0";
    http_conn::HTTP_CODE result = HttpConnTestAccessor::call_parse_request_line(conn, request_line);
    
    EXPECT_EQ(result, http_conn::BAD_REQUEST);
}

TEST_F(HttpConnTest, ParseRequestLineMalformed) {
    conn.init(sockfd, client_addr, const_cast<char*>("/var/www/html"), 0, 0, "user", "pass", "db");
    
    char request_line[] = "GET";
    http_conn::HTTP_CODE result = HttpConnTestAccessor::call_parse_request_line(conn, request_line);
    
    EXPECT_EQ(result, http_conn::BAD_REQUEST);
}

TEST_F(HttpConnTest, ParseLineStatusTests) {
    conn.init(sockfd, client_addr, const_cast<char*>("/var/www/html"), 0, 0, "user", "pass", "db");
    
    strcpy(HttpConnTestAccessor::get_read_buf(conn), "GET /index.html HTTP/1.1\r\n");
    HttpConnTestAccessor::get_read_idx(conn) = strlen(HttpConnTestAccessor::get_read_buf(conn));
    HttpConnTestAccessor::get_checked_idx(conn) = 0;
    
    http_conn::LINE_STATUS status = HttpConnTestAccessor::call_parse_line(conn);
    EXPECT_EQ(status, http_conn::LINE_OK);
}

TEST_F(HttpConnTest, ParseLineIncompleteData) {
    conn.init(sockfd, client_addr, const_cast<char*>("/var/www/html"), 0, 0, "user", "pass", "db");
    
    strcpy(HttpConnTestAccessor::get_read_buf(conn), "GET /index.html");
    HttpConnTestAccessor::get_read_idx(conn) = strlen(HttpConnTestAccessor::get_read_buf(conn));
    HttpConnTestAccessor::get_checked_idx(conn) = 0;
    
    http_conn::LINE_STATUS status = HttpConnTestAccessor::call_parse_line(conn);
    EXPECT_EQ(status, http_conn::LINE_OPEN);
}

TEST_F(HttpConnTest, ParseLineBadFormat) {
    conn.init(sockfd, client_addr, const_cast<char*>("/var/www/html"), 0, 0, "user", "pass", "db");
    
    strcpy(HttpConnTestAccessor::get_read_buf(conn), "GET /index.html\n");
    HttpConnTestAccessor::get_read_idx(conn) = strlen(HttpConnTestAccessor::get_read_buf(conn));
    HttpConnTestAccessor::get_checked_idx(conn) = 0;
    
    http_conn::LINE_STATUS status = HttpConnTestAccessor::call_parse_line(conn);
    EXPECT_EQ(status, http_conn::LINE_BAD);
}

TEST_F(HttpConnTest, ParseHeadersValidHeaders) {
    conn.init(sockfd, client_addr, const_cast<char*>("/var/www/html"), 0, 0, "user", "pass", "db");
    
    char header1[] = "Host: localhost:8080";
    http_conn::HTTP_CODE result1 = HttpConnTestAccessor::call_parse_headers(conn, header1);
    EXPECT_EQ(result1, http_conn::NO_REQUEST);
    EXPECT_STREQ(HttpConnTestAccessor::get_host(conn), "localhost:8080");
    
    char header2[] = "Content-Length: 100";
    http_conn::HTTP_CODE result2 = HttpConnTestAccessor::call_parse_headers(conn, header2);
    EXPECT_EQ(result2, http_conn::NO_REQUEST);
    EXPECT_EQ(HttpConnTestAccessor::get_content_length(conn), 100);
    
    char header3[] = "Connection: keep-alive";
    http_conn::HTTP_CODE result3 = HttpConnTestAccessor::call_parse_headers(conn, header3);
    EXPECT_EQ(result3, http_conn::NO_REQUEST);
    EXPECT_TRUE(HttpConnTestAccessor::get_linger(conn));
}

TEST_F(HttpConnTest, ParseHeadersEmptyLine) {
    conn.init(sockfd, client_addr, const_cast<char*>("/var/www/html"), 0, 0, "user", "pass", "db");
    
    char empty_line[] = "";
    http_conn::HTTP_CODE result = HttpConnTestAccessor::call_parse_headers(conn, empty_line);
    EXPECT_EQ(result, http_conn::GET_REQUEST);
}

TEST_F(HttpConnTest, CloseConnectionTest) {
    conn.init(sockfd, client_addr, const_cast<char*>("/var/www/html"), 0, 0, "user", "pass", "db");
    int initial_count = http_conn::m_user_count;
    
    conn.close_conn(true);
    
    EXPECT_EQ(http_conn::m_user_count, initial_count - 1);
    EXPECT_EQ(HttpConnTestAccessor::get_sockfd(conn), -1);
}

TEST_F(HttpConnTest, BufferOverflowProtection) {
    conn.init(sockfd, client_addr, const_cast<char*>("/var/www/html"), 0, 0, "user", "pass", "db");
    
    HttpConnTestAccessor::get_read_idx(conn) = http_conn::READ_BUFFER_SIZE;
    bool result = conn.read_once();
    
    EXPECT_FALSE(result);
}

// HttpResponseTest 移除，因为这些方法依赖日志系统初始化

class UserSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        username = "testuser";
        role = "admin";
    }
    
    string username;
    string role;
};

TEST_F(UserSessionTest, SessionCreationTest) {
    UserSession session(username, role);
    
    EXPECT_EQ(session.username, username);
    EXPECT_EQ(session.role, role);
    EXPECT_GT(session.created_at, 0);
    EXPECT_EQ(session.created_at, session.last_access);
    EXPECT_FALSE(session.is_expired());
}

TEST_F(UserSessionTest, SessionExpirationLogicTest) {
    UserSession session(username, role);
    
    session.last_access = time(nullptr) - UserSession::SESSION_TIMEOUT - 1;
    EXPECT_TRUE(session.is_expired());
    
    session.update_access_time();
    EXPECT_FALSE(session.is_expired());
}

TEST_F(UserSessionTest, DefaultConstructorTest) {
    UserSession session;
    
    EXPECT_TRUE(session.username.empty());
    EXPECT_TRUE(session.role.empty());
    EXPECT_EQ(session.created_at, 0);
    EXPECT_EQ(session.last_access, 0);
}

class HttpStaticMethodsTest : public ::testing::Test {
protected:
    void SetUp() override {
        http_conn::cleanup_expired_sessions();
    }
    
    void TearDown() override {
        http_conn::cleanup_expired_sessions();
    }
};

TEST_F(HttpStaticMethodsTest, SessionManagementTest) {
    string username = "testuser";
    string role = "admin";
    
    string session_id = http_conn::create_session(username, role);
    EXPECT_FALSE(session_id.empty());
    
    bool is_valid = http_conn::validate_session(session_id);
    EXPECT_TRUE(is_valid);
    
    UserSession* session = http_conn::get_session(session_id);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->username, username);
    EXPECT_EQ(session->role, role);
    
    string retrieved_username = http_conn::get_session_username(session_id);
    EXPECT_EQ(retrieved_username, username);
    
    string retrieved_role = http_conn::get_session_role(session_id);
    EXPECT_EQ(retrieved_role, role);
    
    http_conn::destroy_session(session_id);
    EXPECT_FALSE(http_conn::validate_session(session_id));
}

TEST_F(HttpStaticMethodsTest, SessionExpirationTest) {
    string username = "testuser";
    string role = "user";
    
    string session_id = http_conn::create_session(username, role);
    UserSession* session = http_conn::get_session(session_id);
    ASSERT_NE(session, nullptr);
    
    session->last_access = time(nullptr) - UserSession::SESSION_TIMEOUT - 1;
    EXPECT_TRUE(session->is_expired());
    
    http_conn::cleanup_expired_sessions();
    EXPECT_FALSE(http_conn::validate_session(session_id));
}

TEST_F(HttpStaticMethodsTest, GenerateSaltTest) {
    string salt1 = http_conn::generate_salt(16);
    string salt2 = http_conn::generate_salt(16);
    
    EXPECT_EQ(salt1.length(), 16);
    EXPECT_EQ(salt2.length(), 16);
    EXPECT_NE(salt1, salt2);
    
    string salt_custom = http_conn::generate_salt(32);
    EXPECT_EQ(salt_custom.length(), 32);
}

TEST_F(HttpStaticMethodsTest, PasswordHashingConsistencyTest) {
    string password = "mypassword123";
    string salt = "randomsalt123456";
    
    string hash1 = http_conn::hash_password(password, salt);
    string hash2 = http_conn::hash_password(password, salt);
    
    EXPECT_EQ(hash1, hash2);
    EXPECT_NE(hash1, password);
    EXPECT_GT(hash1.length(), 0);
}

TEST_F(HttpStaticMethodsTest, PasswordVerificationTest) {
    string password = "correctpassword";
    string wrong_password = "wrongpassword";
    string salt = http_conn::generate_salt();
    string hash = http_conn::hash_password(password, salt);
    
    EXPECT_TRUE(http_conn::verify_password(password, hash));
    EXPECT_FALSE(http_conn::verify_password(wrong_password, hash));
}

TEST_F(HttpStaticMethodsTest, SaltExtractionTest) {
    string password = "testpass";
    string original_salt = http_conn::generate_salt(20);
    string hash = http_conn::hash_password(password, original_salt);
    
    string extracted_salt = http_conn::extract_salt_from_hash(hash);
    EXPECT_EQ(extracted_salt, original_salt);
}

TEST_F(HttpStaticMethodsTest, CookieParsingTest) {
    string cookie_header = "session_id=abc123; other_cookie=value";
    string session_id = http_conn::get_session_from_cookie(cookie_header);
    EXPECT_EQ(session_id, "abc123");
    
    string empty_cookie = "";
    string empty_result = http_conn::get_session_from_cookie(empty_cookie);
    EXPECT_TRUE(empty_result.empty());
    
    string no_session_cookie = "other_cookie=value; another=test";
    string no_session_result = http_conn::get_session_from_cookie(no_session_cookie);
    EXPECT_TRUE(no_session_result.empty());
}

TEST_F(HttpStaticMethodsTest, MultipleConcurrentSessionsTest) {
    vector<string> session_ids;
    
    for (int i = 0; i < 5; i++) {
        string username = "user" + to_string(i);
        string role = "role" + to_string(i);
        string session_id = http_conn::create_session(username, role);
        session_ids.push_back(session_id);
        
        EXPECT_TRUE(http_conn::validate_session(session_id));
        EXPECT_EQ(http_conn::get_session_username(session_id), username);
        EXPECT_EQ(http_conn::get_session_role(session_id), role);
    }
    
    for (const string& session_id : session_ids) {
        http_conn::destroy_session(session_id);
        EXPECT_FALSE(http_conn::validate_session(session_id));
    }
}

class HttpConnectionLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        http_conn::m_epollfd = epoll_create(5);
        http_conn::m_user_count = 0;
        
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sin_family = AF_INET;
        client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        client_addr.sin_port = htons(8080);
        
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GT(sockfd, 0);
    }
    
    void TearDown() override {
        if (sockfd > 0) {
            close(sockfd);
        }
        if (http_conn::m_epollfd > 0) {
            close(http_conn::m_epollfd);
        }
    }
    
    http_conn conn;
    int sockfd;
    sockaddr_in client_addr;
};

TEST_F(HttpConnectionLifecycleTest, FullInitializationTest) {
    int initial_count = http_conn::m_user_count;
    
    conn.init(sockfd, client_addr, const_cast<char*>("/var/www/html"), 0, 0, "user", "pass", "db");
    
    EXPECT_EQ(http_conn::m_user_count, initial_count + 1);
    EXPECT_EQ(HttpConnTestAccessor::get_sockfd(conn), sockfd);
    EXPECT_EQ(conn.timer_flag, 0);
    EXPECT_EQ(conn.improv, 0);
}

TEST_F(HttpConnectionLifecycleTest, AddressGetterTest) {
    conn.init(sockfd, client_addr, const_cast<char*>("/var/www/html"), 0, 0, "user", "pass", "db");
    
    sockaddr_in* addr = conn.get_address();
    ASSERT_NE(addr, nullptr);
    EXPECT_EQ(addr->sin_family, AF_INET);
    EXPECT_EQ(addr->sin_port, htons(8080));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}