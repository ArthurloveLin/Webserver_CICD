#ifndef BLOG_HANDLER_H
#define BLOG_HANDLER_H

#include <string>
#include <map>
#include <vector>
#include <mysql/mysql.h>
#include "../CGImysql/sql_connection_pool.h"
#include "../log/log.h"
#include "template_engine.h"

using namespace std;

struct Article {
    int article_id;
    string title;
    string content;
    string summary;
    int author_id;
    int category_id;
    string status;
    int view_count;
    int like_count;
    int comment_count;
    string created_at;
    string updated_at;
    string category_name;
    vector<string> tags;
};

struct Comment {
    int comment_id;
    int article_id;
    string user_name;
    string email;
    string content;
    int parent_id;
    int like_count;
    string created_at;
    vector<Comment> replies;
};

struct Category {
    int category_id;
    string name;
    string description;
    int article_count;
};

struct Tag {
    int tag_id;
    string name;
    int article_count;
};

class BlogHandler {
public:
    BlogHandler();
    ~BlogHandler();
    
    // 初始化数据库连接
    void init(connection_pool* conn_pool);
    
    // 路由处理方法
    string handle_request(const string& method, const string& url, const string& post_data, const string& client_ip);
    
    // 页面渲染方法
    string render_blog_index(int page = 1);
    string render_article_detail(int article_id);
    string render_category_page(int category_id, int page = 1);
    string render_admin_dashboard();
    string render_admin_editor(int article_id = 0);
    
    // API接口方法
    string api_get_articles(int page = 1, int category_id = 0);
    string api_get_article(int article_id);
    string api_create_article(const string& post_data);
    string api_update_article(int article_id, const string& post_data);
    string api_delete_article(int article_id);
    string api_add_comment(const string& post_data);
    string api_toggle_like(const string& post_data);
    
    // 管理员验证
    bool is_admin_session(const string& session_id);
    string create_admin_session(const string& username);
    
private:
    connection_pool* m_conn_pool;
    TemplateEngine* template_engine;
    
    // 数据库操作方法
    vector<Article> get_articles_list(int page = 1, int limit = 10, int category_id = 0, const string& status = "published");
    Article get_article_by_id(int article_id);
    vector<Comment> get_article_comments(int article_id);
    vector<Category> get_categories();
    Category get_category_by_id(int category_id);
    int get_category_article_count(int category_id);
    vector<Tag> get_tags();
    bool increment_view_count(int article_id);
    
    // 辅助方法
    string parse_url_param(const string& url, const string& param);
    string url_decode(const string& str);
    string html_escape(const string& str);
    string json_escape(const string& str);
    map<string, string> parse_post_data(const string& data);
    
    // 响应构建方法
    string build_json_response(const string& json_data, int status_code = 200);
    string build_html_response(const string& html_content, int status_code = 200);
    string build_error_response(int status_code, const string& message);
    
    // 路由解析
    bool is_blog_route(const string& url);
    string extract_route_param(const string& url, const string& pattern);
    
    // Session管理（简单实现）
    map<string, string> admin_sessions;
    string generate_session_id();
};

#endif