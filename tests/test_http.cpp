#include <gtest/gtest.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../http/http_conn.h"

class HttpConnTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化必要的测试数据
    }

    void TearDown() override {
        // 清理测试数据
    }
};

// 测试HTTP连接的基本初始化
TEST_F(HttpConnTest, BasicInitialization) {
    http_conn conn;
    
    // 创建一个简单的socket地址结构
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    
    // 测试地址获取功能（不需要实际socket）
    // 这里只是测试类的基本结构
    EXPECT_NO_THROW({
        // 基本的构造函数测试
        http_conn test_conn;
    });
}

// 测试密码加密功能
TEST_F(HttpConnTest, PasswordHashing) {
    // 测试生成盐值
    string salt = http_conn::generate_salt(16);
    EXPECT_EQ(salt.length(), 16);
    
    // 测试同一个盐值生成的结果应该一致
    string salt2 = http_conn::generate_salt(16);
    EXPECT_NE(salt, salt2); // 不同的盐值应该不同
    
    // 测试密码哈希
    string password = "testpassword123";
    string test_salt = "abcdef1234567890";
    string stored_hash = http_conn::hash_password(password, test_salt);
    string stored_hash2 = http_conn::hash_password(password, test_salt);
    
    // 相同密码和盐值应该产生相同的哈希
    EXPECT_EQ(stored_hash, stored_hash2);
    EXPECT_GT(stored_hash.length(), 0);
    // 验证格式是 salt$hash
    EXPECT_NE(stored_hash.find('$'), string::npos);
    
    // 测试密码验证
    EXPECT_TRUE(http_conn::verify_password(password, stored_hash));
    EXPECT_FALSE(http_conn::verify_password("wrongpassword", stored_hash));
    
    // 测试从哈希中提取盐值
    string extracted_salt = http_conn::extract_salt_from_hash(stored_hash);
    EXPECT_EQ(extracted_salt, test_salt);
}

// 测试Session管理功能
TEST_F(HttpConnTest, SessionManagement) {
    // 创建session
    string session_id = http_conn::create_session("testuser", "admin");
    EXPECT_GT(session_id.length(), 0);
    
    // 验证session
    EXPECT_TRUE(http_conn::validate_session(session_id));
    EXPECT_FALSE(http_conn::validate_session("invalid_session"));
    
    // 获取session信息
    UserSession* session = http_conn::get_session(session_id);
    EXPECT_NE(session, nullptr);
    if (session) {
        EXPECT_EQ(session->username, "testuser");
        EXPECT_EQ(session->role, "admin");
        EXPECT_FALSE(session->is_expired());
    }
    
    // 获取session用户名和角色
    EXPECT_EQ(http_conn::get_session_username(session_id), "testuser");
    EXPECT_EQ(http_conn::get_session_role(session_id), "admin");
    
    // 销毁session
    http_conn::destroy_session(session_id);
    EXPECT_FALSE(http_conn::validate_session(session_id));
}

// 测试UserSession结构
TEST_F(HttpConnTest, UserSessionStruct) {
    UserSession session("testuser", "user");
    
    EXPECT_EQ(session.username, "testuser");
    EXPECT_EQ(session.role, "user");
    EXPECT_FALSE(session.is_expired());
    
    // 测试访问时间更新
    time_t original_time = session.last_access;
    sleep(1);
    session.update_access_time();
    EXPECT_GT(session.last_access, original_time);
}

// 测试Cookie解析功能
TEST_F(HttpConnTest, CookieSessionExtraction) {
    // 测试从cookie头中提取session
    string cookie_header = "session_id=abc123def456; other_cookie=value";
    string session_id = http_conn::get_session_from_cookie(cookie_header);
    EXPECT_EQ(session_id, "abc123def456");
    
    // 测试没有session的cookie
    string cookie_no_session = "other_cookie=value; another=test";
    string empty_session = http_conn::get_session_from_cookie(cookie_no_session);
    EXPECT_EQ(empty_session, "");
    
    // 测试空cookie头
    string empty_cookie = "";
    string empty_result = http_conn::get_session_from_cookie(empty_cookie);
    EXPECT_EQ(empty_result, "");
}