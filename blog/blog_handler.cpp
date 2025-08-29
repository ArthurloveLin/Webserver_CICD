#include "blog_handler.h"
#include <sstream>
#include <algorithm>
#include <ctime>
#include <random>

// 静态函数指针定义
bool (*BlogHandler::validate_session_func)(const string& session_id) = nullptr;
string (*BlogHandler::get_username_func)(const string& session_id) = nullptr;
string (*BlogHandler::get_role_func)(const string& session_id) = nullptr;

BlogHandler::BlogHandler() : m_conn_pool(nullptr) {
    markdown_parser = new MarkdownParser();
    image_uploader = new ImageUploader();
}

BlogHandler::~BlogHandler() {
    delete markdown_parser;
    delete image_uploader;
}

void BlogHandler::init(connection_pool* conn_pool) {
    m_conn_pool = conn_pool;
}

string BlogHandler::handle_request(const string& method, const string& url, const string& post_data, const string& client_ip, const string& cookie_header) {
    if (!is_blog_route(url)) {
        return "";
    }
    
    // 处理方法覆盖（_method字段）
    string actual_method = method;
    if (method == "POST" && !post_data.empty()) {
        map<string, string> form_data = parse_post_data(post_data);
        if (!form_data["_method"].empty()) {
            actual_method = form_data["_method"];
        }
    }
    
    
    try {
        // 路由分发
        if (url == "/blog/" || url == "/blog") {
            return render_blog_index();
        }
        else if (url.find("/blog/article/") == 0) {
            string article_id_str = extract_route_param(url, "/blog/article/");
            int article_id = atoi(article_id_str.c_str());
            if (article_id > 0) {
                return render_article_detail(article_id);
            }
        }
        else if (url.find("/blog/category/") == 0) {
            string category_id_str = extract_route_param(url, "/blog/category/");
            int category_id = atoi(category_id_str.c_str());
            if (category_id > 0) {
                // 解析page参数
                int page = 1;
                string page_param = parse_url_param(url, "page");
                if (!page_param.empty()) {
                    page = max(1, atoi(page_param.c_str()));
                }
                return render_category_page(category_id, page);
            }
        }
        else if (url == "/blog/admin/" || url == "/blog/admin") {
            // 检查管理员权限
            if (!check_user_permission(cookie_header, "admin")) {
                return build_error_response(403, "需要管理员权限访问");
            }
            return render_admin_dashboard();
        }
        else if (url == "/blog/admin/new") {
            // 检查管理员权限
            if (!check_user_permission(cookie_header, "admin")) {
                return build_error_response(403, "需要管理员权限访问");
            }
            return render_admin_editor();
        }
        else if (url.find("/blog/admin/edit/") == 0) {
            // 检查管理员权限
            if (!check_user_permission(cookie_header, "admin")) {
                return build_error_response(403, "需要管理员权限访问");
            }
            string article_id_str = extract_route_param(url, "/blog/admin/edit/");
            int article_id = atoi(article_id_str.c_str());
            if (article_id > 0) {
                return render_admin_editor(article_id);
            }
        }
        // API路由
        else if (url == "/blog/api/articles") {
            printf("Debug: 处理 /blog/api/articles 路由，方法: %s\n", actual_method.c_str());
            if (actual_method == "GET") {
                return api_get_articles();
            } else if (actual_method == "POST") {
                // 创建文章需要管理员权限
                printf("Debug: 检查用户权限，cookie: %s\n", cookie_header.c_str());
                bool has_permission = check_user_permission(cookie_header, "admin");
                printf("Debug: 权限检查结果: %s\n", has_permission ? "true" : "false");
                if (!has_permission) {
                    return build_json_response("{\"success\":false,\"message\":\"需要管理员权限\"}", 403);
                }
                printf("Debug: 开始调用 api_create_article\n");
                return api_create_article(post_data);
            }
        }
        else if (url.find("/blog/api/articles/") == 0) {
            string article_id_str = extract_route_param(url, "/blog/api/articles/");
            int article_id = atoi(article_id_str.c_str());
            if (article_id > 0) {
                if (actual_method == "GET") {
                    return api_get_article(article_id);
                } else if (actual_method == "PUT") {
                    // 更新文章需要管理员权限
                    printf("Debug: 更新文章权限检查，cookie: %s\n", cookie_header.c_str());
                    bool has_permission = check_user_permission(cookie_header, "admin");
                    printf("Debug: 更新权限检查结果: %s\n", has_permission ? "true" : "false");
                    if (!has_permission) {
                        return build_json_response("{\"success\":false,\"message\":\"需要管理员权限\"}", 403);
                    }
                    return api_update_article(article_id, post_data);
                } else if (actual_method == "DELETE") {
                    // 删除文章需要管理员权限
                    if (!check_user_permission(cookie_header, "admin")) {
                        return build_json_response("{\"success\":false,\"message\":\"需要管理员权限\"}", 403);
                    }
                    return api_delete_article(article_id);
                }
            }
        }
        else if (url == "/blog/api/comments" && actual_method == "POST") {
            return api_add_comment(post_data);
        }
        else if (url == "/blog/api/like" && actual_method == "POST") {
            return api_toggle_like(post_data);
        }
        else if (url == "/blog/api/upload/image" && actual_method == "POST") {
            // 图片上传需要管理员权限
            if (!check_user_permission(cookie_header, "admin")) {
                return build_json_response("{\"success\":false,\"message\":\"需要管理员权限\"}", 403);
            }
            return api_upload_image("multipart/form-data", post_data);
        }
        
        return build_error_response(404, "Page not found");
    }
    catch (const exception& e) {
        return build_error_response(500, "Internal server error");
    }
}

string BlogHandler::render_blog_index(int page) {
    vector<Article> articles = get_articles_list(page);
    vector<Category> categories = get_categories();
    
    // 直接生成HTML，统一现代化风格
    stringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html lang=\"zh-CN\">\n";
    html << "<head>\n";
    html << "<meta charset=\"UTF-8\">\n";
    html << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    html << "<title>TinyWebServer博客</title>\n";
    html << "<link href=\"https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap\" rel=\"stylesheet\">\n";
    html << "<style>\n";
    html << "* { margin: 0; padding: 0; box-sizing: border-box; }\n";
    html << "body { font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 20px; }\n";
    html << ".blog-container { max-width: 1200px; margin: 0 auto; }\n";
    html << ".header { background: rgba(255, 255, 255, 0.95); backdrop-filter: blur(10px); border-radius: 20px; box-shadow: 0 20px 40px rgba(0, 0, 0, 0.1); border: 1px solid rgba(255, 255, 255, 0.2); padding: 40px; text-align: center; margin-bottom: 30px; }\n";
    html << ".header h1 { font-size: 32px; font-weight: 700; color: #2d3748; margin-bottom: 12px; letter-spacing: -0.5px; }\n";
    html << ".header p { color: #718096; font-size: 16px; font-weight: 400; margin-bottom: 20px; }\n";
    html << ".nav { display: flex; gap: 15px; justify-content: center; }\n";
    html << ".btn { display: inline-flex; align-items: center; padding: 12px 24px; border-radius: 25px; text-decoration: none; font-weight: 600; font-size: 14px; transition: all 0.3s ease; border: none; cursor: pointer; }\n";
    html << ".btn { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; }\n";
    html << ".btn:hover { transform: translateY(-2px); box-shadow: 0 10px 25px rgba(102, 126, 234, 0.3); }\n";
    html << ".btn-secondary { background: linear-gradient(135deg, #4299e1, #3182ce); }\n";
    html << ".main-content { display: grid; grid-template-columns: 2fr 1fr; gap: 30px; }\n";
    html << ".content { background: rgba(255, 255, 255, 0.95); backdrop-filter: blur(10px); border-radius: 20px; box-shadow: 0 20px 40px rgba(0, 0, 0, 0.1); border: 1px solid rgba(255, 255, 255, 0.2); padding: 30px; }\n";
    html << ".sidebar { background: rgba(255, 255, 255, 0.95); backdrop-filter: blur(10px); border-radius: 20px; box-shadow: 0 20px 40px rgba(0, 0, 0, 0.1); border: 1px solid rgba(255, 255, 255, 0.2); padding: 30px; height: fit-content; }\n";
    html << ".article { margin-bottom: 30px; padding-bottom: 30px; border-bottom: 1px solid #e2e8f0; }\n";
    html << ".article:last-child { border-bottom: none; }\n";
    html << ".article-title { font-size: 24px; font-weight: 600; color: #2d3748; margin-bottom: 15px; }\n";
    html << ".article-title a { color: #2d3748; text-decoration: none; transition: color 0.3s ease; }\n";
    html << ".article-title a:hover { color: #667eea; }\n";
    html << ".article-meta { display: flex; gap: 20px; font-size: 14px; color: #718096; margin-bottom: 15px; flex-wrap: wrap; }\n";
    html << ".article-summary { color: #4a5568; line-height: 1.6; margin-bottom: 15px; }\n";
    html << ".article-actions { margin-top: 15px; }\n";
    html << ".widget { margin-bottom: 30px; }\n";
    html << ".widget h3 { font-size: 18px; font-weight: 600; color: #2d3748; margin-bottom: 15px; }\n";
    html << ".category-list { list-style: none; }\n";
    html << ".category-list li { margin-bottom: 8px; }\n";
    html << ".category-list a { color: #4a5568; text-decoration: none; display: flex; justify-content: space-between; padding: 8px 12px; border-radius: 8px; transition: all 0.3s ease; }\n";
    html << ".category-list a:hover { background: #f7fafc; color: #667eea; }\n";
    html << ".count { background: #e2e8f0; color: #4a5568; padding: 2px 8px; border-radius: 12px; font-size: 12px; }\n";
    html << ".no-content { text-align: center; padding: 60px 20px; color: #718096; }\n";
    html << ".no-content h3 { font-size: 20px; margin-bottom: 10px; color: #4a5568; }\n";
    html << "@media (max-width: 768px) { .main-content { grid-template-columns: 1fr; } .header { padding: 30px 20px; } .header h1 { font-size: 24px; } .content, .sidebar { padding: 20px; } }\n";
    html << "</style>\n";
    html << "</head>\n";
    html << "<body>\n";
    html << "<div class=\"blog-container\">\n";
    
    html << "<div class=\"header\">\n";
    html << "<h1>TinyWebServer博客</h1>\n";
    html << "<p>基于C++的高性能Web服务器博客系统</p>\n";
    html << "<div class=\"nav\">\n";
    html << "<a href=\"/welcome.html\" class=\"btn\">首页</a>\n";
    html << "<a href=\"/blog/admin/\" class=\"btn btn-secondary\">管理后台</a>\n";
    html << "</div>\n";
    html << "</div>\n";
    
    html << "<div class=\"main-content\">\n";
    html << "<div class=\"content\">\n";
    
    if (articles.empty()) {
        html << "<div class=\"no-content\">\n";
        html << "<h3>暂无文章</h3>\n";
        html << "<p>还没有发布任何文章，<a href=\"/blog/admin/new\">立即创建</a>第一篇文章吧！</p>\n";
        html << "</div>\n";
    } else {
        for (const auto& article : articles) {
            html << "<article class=\"article\">\n";
            html << "<h2 class=\"article-title\">\n";
            html << "<a href=\"/blog/article/" << article.article_id << "\">" 
                 << html_escape(article.title) << "</a>\n";
            html << "</h2>\n";
            html << "<div class=\"article-meta\">\n";
            html << "<span>📅 发布时间: " << article.created_at << "</span>\n";
            html << "<span>📂 分类: " << html_escape(article.category_name) << "</span>\n";
            html << "<span>👁️ 浏览: " << article.view_count << "</span>\n";
            html << "<span>💬 评论: " << article.comment_count << "</span>\n";
            html << "</div>\n";
            html << "<div class=\"article-summary\">" << html_escape(article.summary) << "</div>\n";
            html << "<div class=\"article-actions\">\n";
            html << "<a href=\"/blog/article/" << article.article_id << "\" class=\"btn\">阅读全文</a>\n";
            html << "</div>\n";
            html << "</article>\n";
        }
    }
    
    html << "</div>\n"; // content
    
    html << "<aside class=\"sidebar\">\n";
    html << "<div class=\"widget\">\n";
    html << "<h3>文章分类</h3>\n";
    html << "<ul class=\"category-list\">\n";
    for (const auto& category : categories) {
        html << "<li>\n";
        html << "<a href=\"/blog/category/" << category.category_id << "\">\n";
        html << html_escape(category.name) << " <span class=\"count\">" << category.article_count << "</span>\n";
        html << "</a>\n";
        html << "</li>\n";
    }
    html << "</ul>\n";
    html << "</div>\n";
    
    html << "</aside>\n";
    
    html << "</div>\n"; // main-content
    html << "</div>\n"; // blog-container
    
    html << "</body>\n";
    html << "</html>\n";
    
    return build_html_response(html.str());
}

string BlogHandler::render_article_detail(int article_id) {
    Article article = get_article_by_id(article_id);
    if (article.article_id == 0) {
        return build_error_response(404, "Article not found");
    }
    
    // 增加浏览计数
    increment_view_count(article_id);
    
    vector<Comment> comments = get_article_comments(article_id);
    
    stringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html lang=\"zh-CN\">\n<head>\n";
    html << "<meta charset=\"UTF-8\">\n";
    html << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    html << "<title>" << html_escape(article.title) << " - TinyWebServer博客</title>\n";
    html << "<link href=\"https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap\" rel=\"stylesheet\">\n";
    html << "<style>\n";
    html << "* { margin: 0; padding: 0; box-sizing: border-box; }\n";
    html << "body { font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 20px; }\n";
    html << ".container { max-width: 900px; margin: 0 auto; background: rgba(255, 255, 255, 0.95); backdrop-filter: blur(10px); border-radius: 20px; box-shadow: 0 20px 40px rgba(0, 0, 0, 0.1); border: 1px solid rgba(255, 255, 255, 0.2); padding: 40px; }\n";
    html << ".article-title { font-size: 28px; font-weight: 700; color: #2d3748; margin-bottom: 20px; line-height: 1.2; }\n";
    html << ".article-meta { color: #718096; margin-bottom: 30px; padding-bottom: 20px; border-bottom: 1px solid #e2e8f0; font-size: 14px; }\n";
    html << ".article-content { line-height: 1.8; color: #4a5568; margin-bottom: 40px; white-space: pre-wrap; font-size: 16px; }\n";
    html << ".comments { margin-top: 40px; }\n";
    html << ".comments h3 { font-size: 20px; font-weight: 600; color: #2d3748; margin-bottom: 20px; }\n";
    html << ".comment { background: rgba(248, 250, 252, 0.8); padding: 20px; margin: 20px 0; border-radius: 12px; border-left: 4px solid #667eea; backdrop-filter: blur(5px); }\n";
    html << ".comment-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }\n";
    html << ".comment-author { font-weight: 600; color: #2d3748; }\n";
    html << ".comment-time { color: #718096; font-size: 12px; }\n";
    html << ".comment-content { margin: 12px 0; line-height: 1.6; color: #4a5568; }\n";
    html << ".comment-likes { color: #667eea; font-size: 12px; margin-top: 8px; }\n";
    html << ".no-comments { text-align: center; color: #718096; padding: 40px; background: rgba(248, 250, 252, 0.5); border-radius: 12px; margin: 20px 0; }\n";
    html << ".back-link { display: inline-flex; align-items: center; margin-bottom: 20px; color: #667eea; text-decoration: none; font-weight: 500; padding: 8px 16px; border-radius: 8px; transition: all 0.3s ease; }\n";
    html << ".back-link:hover { background: rgba(102, 126, 234, 0.1); transform: translateX(-5px); }\n";
    html << ".comment-form { background: rgba(248, 250, 252, 0.6); padding: 30px; border-radius: 16px; margin-top: 30px; backdrop-filter: blur(5px); }\n";
    html << ".comment-form h3 { font-size: 18px; font-weight: 600; color: #2d3748; margin-bottom: 20px; }\n";
    html << ".form-group { margin-bottom: 20px; }\n";
    html << ".form-group label { display: block; margin-bottom: 8px; font-weight: 500; color: #4a5568; }\n";
    html << ".form-group input, .form-group textarea { width: 100%; padding: 12px 16px; border: 2px solid #e2e8f0; border-radius: 8px; font-size: 14px; transition: all 0.3s ease; font-family: inherit; }\n";
    html << ".form-group input:focus, .form-group textarea:focus { outline: none; border-color: #667eea; box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1); }\n";
    html << ".btn { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 12px 24px; border: none; border-radius: 25px; cursor: pointer; font-weight: 600; font-size: 14px; transition: all 0.3s ease; }\n";
    html << ".btn:hover { transform: translateY(-2px); box-shadow: 0 10px 25px rgba(102, 126, 234, 0.3); }\n";
    html << "@media (max-width: 768px) { .container { padding: 30px 20px; } .article-title { font-size: 24px; } }\n";
    html << "</style>\n";
    html << "</head>\n<body>\n";
    
    html << "<div class=\"container\">\n";
    html << "<a href=\"/welcome.html\" class=\"back-link\">← 返回首页</a>\n";
    html << "<h1 class=\"article-title\">" << html_escape(article.title) << "</h1>\n";
    html << "<div class=\"article-meta\">\n";
    html << "发布时间: " << article.created_at << " | ";
    html << "分类: " << html_escape(article.category_name) << " | ";
    html << "浏览: " << article.view_count << " | ";
    html << "评论: " << article.comment_count << "\n";
    html << "</div>\n";
    html << "<div class=\"article-content\">" << render_content(article.content, article.content_type) << "</div>\n";
    
    // 评论区
    html << "<div class=\"comments\">\n";
    html << "<h3>评论 (" << comments.size() << ")</h3>\n";
    
    if (comments.empty()) {
        html << "<div class=\"no-comments\">\n";
        html << "<p>还没有评论，快来抢沙发吧！</p>\n";
        html << "</div>\n";
    } else {
        for (const auto& comment : comments) {
            html << "<div class=\"comment\">\n";
            html << "<div class=\"comment-header\">\n";
            html << "<span class=\"comment-author\">👤 " << html_escape(comment.user_name) << "</span>\n";
            html << "<span class=\"comment-time\">🕐 " << comment.created_at << "</span>\n";
            html << "</div>\n";
            html << "<div class=\"comment-content\">" << html_escape(comment.content) << "</div>\n";
            if (comment.like_count > 0) {
                html << "<div class=\"comment-likes\">👍 " << comment.like_count << "</div>\n";
            }
            html << "</div>\n";
        }
    }
    html << "</div>\n";
    
    // 评论表单
    html << "<div class=\"comment-form\">\n";
    html << "<h3>发表评论</h3>\n";
    html << "<form id=\"comment-form\" method=\"post\" action=\"/blog/api/comments\">\n";
    html << "<input type=\"hidden\" name=\"article_id\" value=\"" << article_id << "\">\n";
    html << "<div class=\"form-group\">\n";
    html << "<label>姓名:</label>\n";
    html << "<input type=\"text\" name=\"user_name\" required>\n";
    html << "</div>\n";
    html << "<div class=\"form-group\">\n";
    html << "<label>邮箱:</label>\n";
    html << "<input type=\"email\" name=\"email\">\n";
    html << "</div>\n";
    html << "<div class=\"form-group\">\n";
    html << "<label>评论内容:</label>\n";
    html << "<textarea name=\"content\" rows=\"4\" required></textarea>\n";
    html << "</div>\n";
    html << "<button type=\"submit\" class=\"btn\">提交评论</button>\n";
    html << "</form>\n";
    html << "</div>\n";
    
    // JavaScript for comment form
    html << "<script>\n";
    html << "document.getElementById('comment-form').addEventListener('submit', function(e) {\n";
    html << "    e.preventDefault();\n";
    html << "    \n";
    html << "    const formData = new FormData(this);\n";
    html << "    const submitBtn = this.querySelector('button[type=\"submit\"]');\n";
    html << "    const originalText = submitBtn.textContent;\n";
    html << "    \n";
    html << "    submitBtn.textContent = '提交中...';\n";
    html << "    submitBtn.disabled = true;\n";
    html << "    \n";
    html << "    const params = new URLSearchParams();\n";
    html << "    for (const [key, value] of formData.entries()) {\n";
    html << "        params.append(key, value);\n";
    html << "    }\n";
    html << "    \n";
    html << "    fetch('/blog/api/comments', {\n";
    html << "        method: 'POST',\n";
    html << "        headers: {\n";
    html << "            'Content-Type': 'application/x-www-form-urlencoded'\n";
    html << "        },\n";
    html << "        body: params.toString()\n";
    html << "    })\n";
    html << "    .then(response => response.text().then(text => {\n";
    html << "        console.log('Comment response:', text);\n";
    html << "        try {\n";
    html << "            return JSON.parse(text);\n";
    html << "        } catch (e) {\n";
    html << "            throw new Error('Invalid JSON: ' + text);\n";
    html << "        }\n";
    html << "    }))\n";
    html << "    .then(data => {\n";
    html << "        if (data.success) {\n";
    html << "            alert('评论发布成功！');\n";
    html << "            // 清空表单\n";
    html << "            this.querySelector('[name=\"user_name\"]').value = '';\n";
    html << "            this.querySelector('[name=\"email\"]').value = '';\n";
    html << "            this.querySelector('[name=\"content\"]').value = '';\n";
    html << "        } else {\n";
    html << "            alert('评论发布失败：' + (data.message || '未知错误'));\n";
    html << "        }\n";
    html << "    })\n";
    html << "    .catch(error => {\n";
    html << "        console.error('Comment error:', error);\n";
    html << "        alert('评论发布失败：' + error.message);\n";
    html << "    })\n";
    html << "    .finally(() => {\n";
    html << "        submitBtn.textContent = originalText;\n";
    html << "        submitBtn.disabled = false;\n";
    html << "    });\n";
    html << "});\n";
    html << "</script>\n";
    
    html << "</div>\n</body>\n</html>";
    
    return build_html_response(html.str());
}

string BlogHandler::render_admin_dashboard() {
    vector<Article> articles = get_articles_list(1, 50, 0, ""); // 获取所有状态的文章
    
    // 统计数据
    int total_articles = 0;
    int published_articles = 0;
    int draft_articles = 0;
    
    for (const auto& article : articles) {
        total_articles++;
        if (article.status == "published") {
            published_articles++;
        } else if (article.status == "draft") {
            draft_articles++;
        }
    }
    
    // 直接生成HTML，统一现代化风格
    stringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html lang=\"zh-CN\">\n";
    html << "<head>\n";
    html << "<meta charset=\"UTF-8\">\n";
    html << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    html << "<title>管理后台 - TinyWebServer博客</title>\n";
    html << "<link href=\"https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap\" rel=\"stylesheet\">\n";
    html << "<style>\n";
    html << "* { margin: 0; padding: 0; box-sizing: border-box; }\n";
    html << "body { font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 20px; }\n";
    html << ".container { max-width: 1200px; margin: 0 auto; }\n";
    html << ".admin-header { background: rgba(255, 255, 255, 0.95); backdrop-filter: blur(10px); border-radius: 20px; box-shadow: 0 20px 40px rgba(0, 0, 0, 0.1); border: 1px solid rgba(255, 255, 255, 0.2); padding: 30px; margin-bottom: 30px; text-align: center; }\n";
    html << ".admin-header h1 { font-size: 28px; font-weight: 700; color: #2d3748; margin-bottom: 15px; }\n";
    html << ".admin-nav { display: flex; gap: 15px; justify-content: center; margin-top: 20px; }\n";
    html << ".admin-nav a { color: #667eea; text-decoration: none; padding: 8px 16px; border-radius: 8px; transition: all 0.3s ease; font-weight: 500; }\n";
    html << ".admin-nav a:hover { background: rgba(102, 126, 234, 0.1); }\n";
    html << ".stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin-bottom: 30px; }\n";
    html << ".stat-card { background: rgba(255, 255, 255, 0.95); backdrop-filter: blur(10px); border-radius: 16px; padding: 25px; text-align: center; box-shadow: 0 10px 30px rgba(0, 0, 0, 0.1); border: 1px solid rgba(255, 255, 255, 0.2); }\n";
    html << ".stat-card h3 { font-size: 32px; font-weight: 700; margin-bottom: 8px; }\n";
    html << ".stat-card p { color: #718096; font-size: 14px; font-weight: 500; }\n";
    html << ".stat-card.primary h3 { color: #667eea; }\n";
    html << ".stat-card.success h3 { color: #48bb78; }\n";
    html << ".stat-card.warning h3 { color: #ed8936; }\n";
    html << ".stat-card.danger h3 { color: #f56565; }\n";
    html << ".admin-panel { background: rgba(255, 255, 255, 0.95); backdrop-filter: blur(10px); border-radius: 20px; box-shadow: 0 20px 40px rgba(0, 0, 0, 0.1); border: 1px solid rgba(255, 255, 255, 0.2); padding: 30px; }\n";
    html << ".panel-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 25px; }\n";
    html << ".panel-header h2 { font-size: 20px; font-weight: 600; color: #2d3748; }\n";
    html << ".btn { display: inline-flex; align-items: center; padding: 10px 20px; border-radius: 25px; text-decoration: none; font-weight: 600; font-size: 14px; transition: all 0.3s ease; border: none; cursor: pointer; }\n";
    html << ".btn-success { background: linear-gradient(135deg, #48bb78, #38a169); color: white; }\n";
    html << ".btn-success:hover { transform: translateY(-2px); box-shadow: 0 10px 25px rgba(72, 187, 120, 0.3); }\n";
    html << ".btn-sm { padding: 6px 12px; font-size: 12px; }\n";
    html << ".btn-secondary { background: linear-gradient(135deg, #4299e1, #3182ce); color: white; }\n";
    html << ".btn-danger { background: linear-gradient(135deg, #f56565, #e53e3e); color: white; }\n";
    html << ".table { width: 100%; border-collapse: collapse; }\n";
    html << ".table th, .table td { padding: 12px; text-align: left; border-bottom: 1px solid #e2e8f0; }\n";
    html << ".table th { font-weight: 600; color: #4a5568; background: rgba(248, 250, 252, 0.5); }\n";
    html << ".table td { color: #2d3748; }\n";
    html << ".status { padding: 4px 12px; border-radius: 12px; font-size: 12px; font-weight: 600; }\n";
    html << ".status-published { background: #c6f6d5; color: #22543d; }\n";
    html << ".status-draft { background: #fed7cc; color: #c05621; }\n";
    html << ".actions { display: flex; gap: 8px; }\n";
    html << ".no-content { text-align: center; padding: 60px 20px; color: #718096; }\n";
    html << ".no-content h3 { font-size: 18px; margin-bottom: 10px; color: #4a5568; }\n";
    html << "@media (max-width: 768px) { .stats-grid { grid-template-columns: 1fr; } .admin-header { padding: 25px 20px; } .admin-panel { padding: 25px 20px; } .panel-header { flex-direction: column; gap: 15px; text-align: center; } }\n";
    html << "</style>\n";
    html << "</head>\n";
    html << "<body>\n";
    
    html << "<div class=\"container\">\n";
    html << "<div class=\"admin-header\">\n";
    html << "<h1>管理后台</h1>\n";
    html << "<nav class=\"admin-nav\">\n";
    html << "<a href=\"/blog/admin/\">仪表盘</a>\n";
    html << "<a href=\"/blog/admin/new\">新建文章</a>\n";
    html << "<a href=\"/blog/\">返回前台</a>\n";
    html << "</nav>\n";
    html << "</div>\n";
    
    // 统计卡片
    html << "<div class=\"stats-grid\">\n";
    html << "<div class=\"stat-card primary\">\n";
    html << "<h3>" << total_articles << "</h3>\n";
    html << "<p>文章总数</p>\n";
    html << "</div>\n";
    html << "<div class=\"stat-card success\">\n";
    html << "<h3>" << published_articles << "</h3>\n";
    html << "<p>已发布</p>\n";
    html << "</div>\n";
    html << "<div class=\"stat-card warning\">\n";
    html << "<h3>" << draft_articles << "</h3>\n";
    html << "<p>草稿</p>\n";
    html << "</div>\n";
    html << "<div class=\"stat-card danger\">\n";
    html << "<h3>0</h3>\n";
    html << "<p>评论总数</p>\n";
    html << "</div>\n";
    html << "</div>\n";
    
    // 文章管理面板
    html << "<div class=\"admin-panel\">\n";
    html << "<div class=\"panel-header\">\n";
    html << "<h2>文章管理</h2>\n";
    html << "<a href=\"/blog/admin/new\" class=\"btn btn-success\">新建文章</a>\n";
    html << "</div>\n";
    html << "<div class=\"panel-content\">\n";
    
    if (articles.empty()) {
        html << "<div class=\"no-content\">\n";
        html << "<h3>暂无文章</h3>\n";
        html << "<p><a href=\"/blog/admin/new\" class=\"btn btn-success\">创建第一篇文章</a></p>\n";
        html << "</div>\n";
    } else {
        html << "<table class=\"table\">\n";
        html << "<thead>\n";
        html << "<tr><th>ID</th><th>标题</th><th>状态</th><th>分类</th><th>创建时间</th><th>浏览量</th><th>操作</th></tr>\n";
        html << "</thead>\n";
        html << "<tbody>\n";
        
        for (const auto& article : articles) {
            html << "<tr>\n";
            html << "<td>" << article.article_id << "</td>\n";
            html << "<td>" << html_escape(article.title) << "</td>\n";
            html << "<td><span class=\"status status-" << article.status << "\">" << article.status << "</span></td>\n";
            html << "<td>" << html_escape(article.category_name) << "</td>\n";
            html << "<td>" << article.created_at << "</td>\n";
            html << "<td>" << article.view_count << "</td>\n";
            html << "<td class=\"actions\">\n";
            html << "<a href=\"/blog/admin/edit/" << article.article_id << "\" class=\"btn btn-sm\">编辑</a> \n";
            html << "<a href=\"/blog/article/" << article.article_id << "\" class=\"btn btn-sm btn-secondary\">查看</a> \n";
            html << "<button class=\"btn btn-sm btn-danger\" onclick=\"deleteArticle(" << article.article_id << ")\">删除</button>\n";
            html << "</td>\n";
            html << "</tr>\n";
        }
        
        html << "</tbody>\n";
        html << "</table>\n";
    }
    
    html << "</div>\n"; // panel-content
    html << "</div>\n"; // admin-panel
    html << "</div>\n"; // container
    
    // JavaScript
    html << "<script>\n";
    html << "function deleteArticle(articleId) {\n";
    html << "    if (confirm('确定要删除这篇文章吗？此操作不可恢复。')) {\n";
    html << "        fetch('/blog/api/articles/' + articleId, {\n";
    html << "            method: 'DELETE'\n";
    html << "        })\n";
    html << "        .then(response => response.json())\n";
    html << "        .then(data => {\n";
    html << "            if (data.success) {\n";
    html << "                location.reload();\n";
    html << "            } else {\n";
    html << "                alert('删除失败：' + (data.message || '未知错误'));\n";
    html << "            }\n";
    html << "        })\n";
    html << "        .catch(error => {\n";
    html << "            alert('删除失败：网络错误');\n";
    html << "        });\n";
    html << "    }\n";
    html << "}\n";
    html << "</script>\n";
    
    html << "</body>\n";
    html << "</html>\n";
    
    return build_html_response(html.str());
}

// 数据库操作方法的简单实现
vector<Article> BlogHandler::get_articles_list(int page, int limit, int category_id, const string& status) {
    vector<Article> articles;
    if (!m_conn_pool) return articles;
    
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, m_conn_pool);
    
    stringstream query;
    query << "SELECT a.article_id, a.title, a.summary, a.author_id, a.category_id, a.status, ";
    query << "a.view_count, a.like_count, a.comment_count, a.created_at, a.updated_at, ";
    query << "COALESCE(c.name, '未分类') as category_name ";
    query << "FROM articles a LEFT JOIN categories c ON a.category_id = c.category_id ";
    
    if (!status.empty()) {
        query << "WHERE a.status = '" << status << "' ";
        if (category_id > 0) {
            query << "AND a.category_id = " << category_id << " ";
        }
    } else if (category_id > 0) {
        query << "WHERE a.category_id = " << category_id << " ";
    }
    
    query << "ORDER BY a.created_at DESC ";
    query << "LIMIT " << ((page - 1) * limit) << ", " << limit;
    
    if (mysql_query(mysql, query.str().c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(mysql);
        if (result) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                Article article;
                article.article_id = atoi(row[0]);
                article.title = row[1] ? row[1] : "";
                article.summary = row[2] ? row[2] : "";
                article.author_id = atoi(row[3] ? row[3] : "0");
                article.category_id = atoi(row[4] ? row[4] : "0");
                article.status = row[5] ? row[5] : "";
                article.view_count = atoi(row[6] ? row[6] : "0");
                article.like_count = atoi(row[7] ? row[7] : "0");
                article.comment_count = atoi(row[8] ? row[8] : "0");
                article.created_at = row[9] ? row[9] : "";
                article.updated_at = row[10] ? row[10] : "";
                article.category_name = row[11] ? row[11] : "";
                articles.push_back(article);
            }
            mysql_free_result(result);
        }
    }
    
    return articles;
}

Article BlogHandler::get_article_by_id(int article_id) {
    Article article = {0}; // 初始化结构体
    if (!m_conn_pool) return article;
    
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, m_conn_pool);
    
    stringstream query;
    query << "SELECT a.article_id, a.title, a.content, a.content_type, a.summary, a.author_id, a.category_id, ";
    query << "a.status, a.view_count, a.like_count, a.comment_count, a.created_at, a.updated_at, ";
    query << "COALESCE(c.name, '未分类') as category_name ";
    query << "FROM articles a LEFT JOIN categories c ON a.category_id = c.category_id ";
    query << "WHERE a.article_id = " << article_id;
    
    if (mysql_query(mysql, query.str().c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(mysql);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row) {
                article.article_id = atoi(row[0]);
                article.title = row[1] ? row[1] : "";
                article.content = row[2] ? row[2] : "";
                article.content_type = row[3] ? row[3] : "html"; // 新增字段
                article.summary = row[4] ? row[4] : "";
                article.author_id = atoi(row[5] ? row[5] : "0");
                article.category_id = atoi(row[6] ? row[6] : "0");
                article.status = row[7] ? row[7] : "";
                article.view_count = atoi(row[8] ? row[8] : "0");
                article.like_count = atoi(row[9] ? row[9] : "0");
                article.comment_count = atoi(row[10] ? row[10] : "0");
                article.created_at = row[11] ? row[11] : "";
                article.updated_at = row[12] ? row[12] : "";
                article.category_name = row[13] ? row[13] : "";
            }
            mysql_free_result(result);
        }
    }
    
    return article;
}

vector<Comment> BlogHandler::get_article_comments(int article_id) {
    vector<Comment> comments;
    if (!m_conn_pool) return comments;
    
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, m_conn_pool);
    
    stringstream query;
    query << "SELECT comment_id, article_id, user_name, email, content, parent_id, ";
    query << "like_count, created_at FROM comments WHERE article_id = " << article_id;
    query << " ORDER BY created_at ASC";
    
    if (mysql_query(mysql, query.str().c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(mysql);
        if (result) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                Comment comment;
                comment.comment_id = atoi(row[0]);
                comment.article_id = atoi(row[1]);
                comment.user_name = row[2] ? row[2] : "";
                comment.email = row[3] ? row[3] : "";
                comment.content = row[4] ? row[4] : "";
                comment.parent_id = atoi(row[5] ? row[5] : "0");
                comment.like_count = atoi(row[6] ? row[6] : "0");
                comment.created_at = row[7] ? row[7] : "";
                comments.push_back(comment);
            }
            mysql_free_result(result);
        }
    }
    
    return comments;
}

vector<Category> BlogHandler::get_categories() {
    vector<Category> categories;
    if (!m_conn_pool) {
        return categories;
    }
    
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, m_conn_pool);
    
    if (!mysql) {
        return categories;
    }
    
    string query = "SELECT category_id, name, description, article_count FROM categories ORDER BY name";
    
    if (mysql_query(mysql, query.c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(mysql);
        if (result) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                Category category;
                category.category_id = atoi(row[0]);
                category.name = row[1] ? row[1] : "";
                category.description = row[2] ? row[2] : "";
                category.article_count = atoi(row[3] ? row[3] : "0");
                categories.push_back(category);
            }
            mysql_free_result(result);
        } else {
        }
    } else {
    }
    
    return categories;
}

Category BlogHandler::get_category_by_id(int category_id) {
    Category category = {0}; // 初始化结构体
    if (!m_conn_pool) return category;
    
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, m_conn_pool);
    
    stringstream query;
    query << "SELECT category_id, name, description, article_count FROM categories WHERE category_id = " << category_id;
    
    if (mysql_query(mysql, query.str().c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(mysql);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row) {
                category.category_id = atoi(row[0]);
                category.name = row[1] ? row[1] : "";
                category.description = row[2] ? row[2] : "";
                category.article_count = atoi(row[3] ? row[3] : "0");
            }
            mysql_free_result(result);
        }
    }
    
    return category;
}

int BlogHandler::get_category_article_count(int category_id) {
    if (!m_conn_pool) return 0;
    
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, m_conn_pool);
    
    stringstream query;
    query << "SELECT COUNT(*) FROM articles WHERE category_id = " << category_id << " AND status = 'published'";
    
    if (mysql_query(mysql, query.str().c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(mysql);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row && row[0]) {
                int count = atoi(row[0]);
                mysql_free_result(result);
                return count;
            }
            mysql_free_result(result);
        }
    }
    
    return 0;
}


bool BlogHandler::increment_view_count(int article_id) {
    if (!m_conn_pool) return false;
    
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, m_conn_pool);
    
    stringstream query;
    query << "UPDATE articles SET view_count = view_count + 1 WHERE article_id = " << article_id;
    
    return mysql_query(mysql, query.str().c_str()) == 0;
}

// 辅助方法实现
string BlogHandler::html_escape(const string& str) {
    string result = str;
    size_t pos = 0;
    while ((pos = result.find("&", pos)) != string::npos) {
        result.replace(pos, 1, "&amp;");
        pos += 5;
    }
    pos = 0;
    while ((pos = result.find("<", pos)) != string::npos) {
        result.replace(pos, 1, "&lt;");
        pos += 4;
    }
    pos = 0;
    while ((pos = result.find(">", pos)) != string::npos) {
        result.replace(pos, 1, "&gt;");
        pos += 4;
    }
    pos = 0;
    while ((pos = result.find("\"", pos)) != string::npos) {
        result.replace(pos, 1, "&quot;");
        pos += 6;
    }
    return result;
}

string BlogHandler::build_html_response(const string& html_content, int status_code) {
    // 只返回HTML内容，HTTP头部由主服务器处理
    return html_content;
}

string BlogHandler::build_error_response(int status_code, const string& message) {
    stringstream html;
    html << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>错误</title></head>";
    html << "<body><h1>错误 " << status_code << "</h1><p>" << html_escape(message) << "</p>";
    html << "<a href=\"/welcome.html\">返回首页</a></body></html>";
    return build_html_response(html.str(), status_code);
}

bool BlogHandler::is_blog_route(const string& url) {
    return url.find("/blog") == 0;
}

string BlogHandler::extract_route_param(const string& url, const string& pattern) {
    if (url.find(pattern) == 0) {
        string param = url.substr(pattern.length());
        size_t pos = param.find('/');
        if (pos != string::npos) {
            param = param.substr(0, pos);
        }
        return param;
    }
    return "";
}

// 占位符实现，用于编译通过
string BlogHandler::render_category_page(int category_id, int page) {
    // 获取分类信息
    Category category = get_category_by_id(category_id);
    if (category.category_id == 0) {
        return build_error_response(404, "分类不存在");
    }
    
    // 获取该分类下的文章
    vector<Article> articles = get_articles_list(page, 10, category_id, "published");
    vector<Category> all_categories = get_categories();
    
    // 计算分页信息
    int total_articles = get_category_article_count(category_id);
    int total_pages = (total_articles + 9) / 10; // 每页10篇文章
    
    stringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html lang=\"zh-CN\">\n";
    html << "<head>\n";
    html << "<meta charset=\"UTF-8\">\n";
    html << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    html << "<title>" << html_escape(category.name) << " - TinyWebServer博客</title>\n";
    html << "<link rel=\"stylesheet\" href=\"/static/css/main.css\">\n";
    html << "<style>\n";
    html << ".category-header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 60px 0; text-align: center; margin-bottom: 40px; }\n";
    html << ".category-title { font-size: 2.5em; margin-bottom: 10px; font-weight: 300; }\n";
    html << ".category-description { font-size: 1.2em; opacity: 0.9; max-width: 600px; margin: 0 auto; }\n";
    html << ".category-stats { background: rgba(255,255,255,0.1); display: inline-block; padding: 10px 20px; border-radius: 20px; margin-top: 20px; }\n";
    html << ".breadcrumb { margin-bottom: 30px; color: #666; }\n";
    html << ".breadcrumb a { color: #3498db; text-decoration: none; }\n";
    html << ".breadcrumb a:hover { text-decoration: underline; }\n";
    html << ".pagination { text-align: center; margin: 40px 0; }\n";
    html << ".pagination a, .pagination span { display: inline-block; padding: 10px 15px; margin: 0 5px; border: 1px solid #ddd; border-radius: 5px; text-decoration: none; color: #333; }\n";
    html << ".pagination a:hover { background: #3498db; color: white; border-color: #3498db; }\n";
    html << ".pagination .current { background: #3498db; color: white; border-color: #3498db; }\n";
    html << ".category-sidebar { background: #f8f9fa; padding: 20px; border-radius: 8px; }\n";
    html << ".sidebar-title { color: #2c3e50; margin-bottom: 15px; font-size: 1.1em; font-weight: bold; }\n";
    html << "</style>\n";
    html << "</head>\n";
    html << "<body>\n";
    
    // 分类头部
    html << "<div class=\"category-header\">\n";
    html << "<div class=\"container\">\n";
    html << "<h1 class=\"category-title\">" << html_escape(category.name) << "</h1>\n";
    if (!category.description.empty()) {
        html << "<p class=\"category-description\">" << html_escape(category.description) << "</p>\n";
    }
    html << "<div class=\"category-stats\">\n";
    html << "📚 共 " << total_articles << " 篇文章\n";
    html << "</div>\n";
    html << "</div>\n";
    html << "</div>\n";
    
    html << "<div class=\"container\">\n";
    
    // 面包屑导航
    html << "<div class=\"breadcrumb\">\n";
    html << "<a href=\"/welcome.html\">首页</a> > 分类 > " << html_escape(category.name) << "\n";
    html << "</div>\n";
    
    html << "<div class=\"main-content\">\n";
    html << "<div class=\"content\">\n";
    
    // 文章列表
    if (articles.empty()) {
        html << "<div class=\"no-content\">\n";
        html << "<h3>暂无文章</h3>\n";
        html << "<p>该分类下还没有已发布的文章。</p>\n";
        html << "<a href=\"/welcome.html\" class=\"btn\">返回首页</a>\n";
        html << "</div>\n";
    } else {
        for (const auto& article : articles) {
            html << "<article class=\"article\">\n";
            html << "<h2 class=\"article-title\">\n";
            html << "<a href=\"/blog/article/" << article.article_id << "\">" << html_escape(article.title) << "</a>\n";
            html << "</h2>\n";
            html << "<div class=\"article-meta\">\n";
            html << "<span>📅 " << article.created_at << "</span>\n";
            html << "<span>👁️ " << article.view_count << "</span>\n";
            html << "<span>💬 " << article.comment_count << "</span>\n";
            html << "</div>\n";
            if (!article.summary.empty()) {
                html << "<div class=\"article-summary\">\n";
                html << html_escape(article.summary) << "\n";
                html << "</div>\n";
            }
            html << "<div class=\"article-actions\">\n";
            html << "<a href=\"/blog/article/" << article.article_id << "\" class=\"btn\">阅读全文</a>\n";
            html << "</div>\n";
            html << "</article>\n";
        }
        
        // 分页导航
        if (total_pages > 1) {
            html << "<div class=\"pagination\">\n";
            
            // 上一页
            if (page > 1) {
                html << "<a href=\"/blog/category/" << category_id << "?page=" << (page - 1) << "\">« 上一页</a>\n";
            }
            
            // 页码
            for (int i = 1; i <= total_pages; i++) {
                if (i == page) {
                    html << "<span class=\"current\">" << i << "</span>\n";
                } else {
                    html << "<a href=\"/blog/category/" << category_id << "?page=" << i << "\">" << i << "</a>\n";
                }
            }
            
            // 下一页
            if (page < total_pages) {
                html << "<a href=\"/blog/category/" << category_id << "?page=" << (page + 1) << "\">下一页 »</a>\n";
            }
            
            html << "</div>\n";
        }
    }
    
    html << "</div>\n"; // content
    
    // 侧边栏
    html << "<aside class=\"sidebar\">\n";
    html << "<div class=\"category-sidebar\">\n";
    html << "<h3 class=\"sidebar-title\">所有分类</h3>\n";
    html << "<ul class=\"category-list\">\n";
    for (const auto& cat : all_categories) {
        html << "<li";
        if (cat.category_id == category_id) {
            html << " class=\"active\"";
        }
        html << ">\n";
        html << "<a href=\"/blog/category/" << cat.category_id << "\">\n";
        html << html_escape(cat.name) << " <span class=\"count\">" << cat.article_count << "</span>\n";
        html << "</a>\n";
        html << "</li>\n";
    }
    html << "</ul>\n";
    html << "</div>\n";
    
    html << "</aside>\n";
    
    html << "</div>\n"; // main-content
    html << "</div>\n"; // container
    
    html << "</body>\n";
    html << "</html>\n";
    
    return build_html_response(html.str());
}
string BlogHandler::render_admin_editor(int article_id) {
    Article article;
    bool is_edit = false;
    
    if (article_id > 0) {
        article = get_article_by_id(article_id);
        if (article.article_id == 0) {
            return build_error_response(404, "Article not found");
        }
        is_edit = true;
    }
    
    vector<Category> categories = get_categories();
    
    // 直接生成HTML
    stringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html lang=\"zh-CN\">\n";
    html << "<head>\n";
    html << "<meta charset=\"UTF-8\">\n";
    html << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    html << "<title>" << (is_edit ? "编辑文章" : "新建文章") << " - TinyWebServer博客</title>\n";
    html << "<link href=\"https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap\" rel=\"stylesheet\">\n";
    html << "<style>\n";
    
    // 统一的样式，基于welcome.html的风格
    html << "* {\n";
    html << "    margin: 0;\n";
    html << "    padding: 0;\n";
    html << "    box-sizing: border-box;\n";
    html << "}\n\n";
    
    html << "body {\n";
    html << "    font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\n";
    html << "    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\n";
    html << "    min-height: 100vh;\n";
    html << "    padding: 20px;\n";
    html << "}\n\n";
    
    html << ".editor-container {\n";
    html << "    background: rgba(255, 255, 255, 0.95);\n";
    html << "    backdrop-filter: blur(10px);\n";
    html << "    border-radius: 20px;\n";
    html << "    box-shadow: 0 20px 40px rgba(0, 0, 0, 0.1);\n";
    html << "    border: 1px solid rgba(255, 255, 255, 0.2);\n";
    html << "    max-width: 1200px;\n";
    html << "    margin: 0 auto;\n";
    html << "    overflow: hidden;\n";
    html << "    animation: slideUp 0.6s ease-out;\n";
    html << "}\n\n";
    
    html << ".editor-header {\n";
    html << "    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\n";
    html << "    color: white;\n";
    html << "    padding: 40px;\n";
    html << "    text-align: center;\n";
    html << "}\n\n";
    
    html << ".editor-title {\n";
    html << "    font-size: 32px;\n";
    html << "    font-weight: 700;\n";
    html << "    margin-bottom: 12px;\n";
    html << "    letter-spacing: -0.5px;\n";
    html << "}\n\n";
    
    html << ".editor-subtitle {\n";
    html << "    font-size: 16px;\n";
    html << "    font-weight: 400;\n";
    html << "    opacity: 0.9;\n";
    html << "    margin-bottom: 20px;\n";
    html << "}\n\n";
    
    html << ".admin-nav {\n";
    html << "    display: flex;\n";
    html << "    justify-content: center;\n";
    html << "    gap: 15px;\n";
    html << "    flex-wrap: wrap;\n";
    html << "}\n\n";
    
    html << ".admin-nav a {\n";
    html << "    background: rgba(255, 255, 255, 0.2);\n";
    html << "    color: white;\n";
    html << "    text-decoration: none;\n";
    html << "    padding: 8px 16px;\n";
    html << "    border-radius: 20px;\n";
    html << "    font-size: 14px;\n";
    html << "    font-weight: 500;\n";
    html << "    transition: all 0.3s ease;\n";
    html << "    border: 1px solid rgba(255, 255, 255, 0.3);\n";
    html << "}\n\n";
    
    html << ".admin-nav a:hover {\n";
    html << "    background: rgba(255, 255, 255, 0.3);\n";
    html << "    transform: translateY(-2px);\n";
    html << "}\n\n";
    
    html << ".editor-content {\n";
    html << "    padding: 40px;\n";
    html << "}\n\n";
    
    html << ".editor-form {\n";
    html << "    display: grid;\n";
    html << "    grid-template-columns: 2fr 1fr;\n";
    html << "    gap: 40px;\n";
    html << "}\n\n";
    
    html << ".form-group {\n";
    html << "    margin-bottom: 25px;\n";
    html << "}\n\n";
    
    html << ".form-group label {\n";
    html << "    display: block;\n";
    html << "    margin-bottom: 8px;\n";
    html << "    font-weight: 600;\n";
    html << "    color: #2d3748;\n";
    html << "    font-size: 14px;\n";
    html << "}\n\n";
    
    html << ".form-group input,\n";
    html << ".form-group textarea,\n";
    html << ".form-group select {\n";
    html << "    width: 100%;\n";
    html << "    padding: 12px 16px;\n";
    html << "    border: 2px solid #e2e8f0;\n";
    html << "    border-radius: 12px;\n";
    html << "    font-size: 14px;\n";
    html << "    font-family: inherit;\n";
    html << "    transition: all 0.3s ease;\n";
    html << "    background: white;\n";
    html << "}\n\n";
    
    html << ".form-group input:focus,\n";
    html << ".form-group textarea:focus,\n";
    html << ".form-group select:focus {\n";
    html << "    outline: none;\n";
    html << "    border-color: #667eea;\n";
    html << "    box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);\n";
    html << "}\n\n";
    
    html << ".content-editor {\n";
    html << "    min-height: 400px;\n";
    html << "    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;\n";
    html << "    line-height: 1.6;\n";
    html << "}\n\n";
    
    html << ".editor-sidebar {\n";
    html << "    background: #f8f9fa;\n";
    html << "    border-radius: 12px;\n";
    html << "    padding: 25px;\n";
    html << "    height: fit-content;\n";
    html << "}\n\n";
    
    html << ".editor-sidebar h3 {\n";
    html << "    font-size: 18px;\n";
    html << "    font-weight: 600;\n";
    html << "    color: #2d3748;\n";
    html << "    margin-bottom: 20px;\n";
    html << "    border-bottom: 2px solid #667eea;\n";
    html << "    padding-bottom: 8px;\n";
    html << "}\n\n";
    
    html << ".btn {\n";
    html << "    display: inline-block;\n";
    html << "    padding: 12px 24px;\n";
    html << "    border: none;\n";
    html << "    border-radius: 12px;\n";
    html << "    font-size: 14px;\n";
    html << "    font-weight: 600;\n";
    html << "    cursor: pointer;\n";
    html << "    text-decoration: none;\n";
    html << "    transition: all 0.3s ease;\n";
    html << "    margin-right: 10px;\n";
    html << "    margin-bottom: 10px;\n";
    html << "}\n\n";
    
    html << ".btn-success {\n";
    html << "    background: linear-gradient(135deg, #48bb78, #38a169);\n";
    html << "    color: white;\n";
    html << "}\n\n";
    
    html << ".btn-success:hover {\n";
    html << "    transform: translateY(-2px);\n";
    html << "    box-shadow: 0 8px 25px rgba(72, 187, 120, 0.3);\n";
    html << "}\n\n";
    
    html << ".btn-secondary {\n";
    html << "    background: #e2e8f0;\n";
    html << "    color: #4a5568;\n";
    html << "}\n\n";
    
    html << ".btn-secondary:hover {\n";
    html << "    background: #cbd5e0;\n";
    html << "    transform: translateY(-2px);\n";
    html << "}\n\n";
    
    html << "@keyframes slideUp {\n";
    html << "    from {\n";
    html << "        opacity: 0;\n";
    html << "        transform: translateY(30px);\n";
    html << "    }\n";
    html << "    to {\n";
    html << "        opacity: 1;\n";
    html << "        transform: translateY(0);\n";
    html << "    }\n";
    html << "}\n\n";
    
    html << "@media (max-width: 768px) {\n";
    html << "    .editor-form {\n";
    html << "        grid-template-columns: 1fr;\n";
    html << "    }\n";
    html << "    \n";
    html << "    .editor-content {\n";
    html << "        padding: 20px;\n";
    html << "    }\n";
    html << "    \n";
    html << "    .editor-header {\n";
    html << "        padding: 30px 20px;\n";
    html << "    }\n";
    html << "    \n";
    html << "    .editor-title {\n";
    html << "        font-size: 24px;\n";
    html << "    }\n";
    html << "}\n";
    
    // Markdown编辑器样式
    html << ".markdown-toolbar {\n";
    html << "    background: #f8f9fa;\n";
    html << "    border: 1px solid #e2e8f0;\n";
    html << "    border-radius: 8px 8px 0 0;\n";
    html << "    padding: 8px;\n";
    html << "    display: flex;\n";
    html << "    gap: 4px;\n";
    html << "    flex-wrap: wrap;\n";
    html << "}\n\n";
    
    html << ".markdown-toolbar button {\n";
    html << "    background: white;\n";
    html << "    border: 1px solid #d1d5db;\n";
    html << "    border-radius: 6px;\n";
    html << "    padding: 6px 10px;\n";
    html << "    font-size: 12px;\n";
    html << "    cursor: pointer;\n";
    html << "    transition: all 0.2s ease;\n";
    html << "}\n\n";
    
    html << ".markdown-toolbar button:hover {\n";
    html << "    background: #667eea;\n";
    html << "    color: white;\n";
    html << "    border-color: #667eea;\n";
    html << "}\n\n";
    
    html << ".editor-wrapper {\n";
    html << "    display: flex;\n";
    html << "    gap: 10px;\n";
    html << "    border: 2px solid #e2e8f0;\n";
    html << "    border-radius: 0 0 12px 12px;\n";
    html << "    overflow: hidden;\n";
    html << "}\n\n";
    
    html << ".editor-wrapper.preview-mode .content-editor {\n";
    html << "    width: 50%;\n";
    html << "}\n\n";
    
    html << ".editor-wrapper.preview-mode .preview-pane {\n";
    html << "    width: 50%;\n";
    html << "    display: block !important;\n";
    html << "}\n\n";
    
    html << ".content-editor {\n";
    html << "    width: 100%;\n";
    html << "    border: none;\n";
    html << "    resize: vertical;\n";
    html << "    border-radius: 0;\n";
    html << "}\n\n";
    
    html << ".preview-pane {\n";
    html << "    background: white;\n";
    html << "    padding: 16px;\n";
    html << "    border-left: 1px solid #e2e8f0;\n";
    html << "    overflow-y: auto;\n";
    html << "    max-height: 500px;\n";
    html << "}\n\n";
    
    html << ".image-upload-area {\n";
    html << "    background: #f0f7ff;\n";
    html << "    border: 2px dashed #667eea;\n";
    html << "    border-radius: 8px;\n";
    html << "    padding: 20px;\n";
    html << "    text-align: center;\n";
    html << "    margin: 10px 0;\n";
    html << "}\n\n";
    
    html << ".upload-progress {\n";
    html << "    margin-top: 10px;\n";
    html << "    font-size: 14px;\n";
    html << "    color: #667eea;\n";
    html << "}\n\n";
    
    html << ".preview-pane h1, .preview-pane h2, .preview-pane h3 {\n";
    html << "    color: #2d3748;\n";
    html << "    margin: 16px 0 8px 0;\n";
    html << "}\n\n";
    
    html << ".preview-pane p {\n";
    html << "    margin: 8px 0;\n";
    html << "    line-height: 1.6;\n";
    html << "}\n\n";
    
    html << ".preview-pane code {\n";
    html << "    background: #f1f5f9;\n";
    html << "    padding: 2px 4px;\n";
    html << "    border-radius: 4px;\n";
    html << "    font-family: 'Consolas', monospace;\n";
    html << "}\n\n";
    
    html << ".preview-pane pre {\n";
    html << "    background: #1e293b;\n";
    html << "    color: #e2e8f0;\n";
    html << "    padding: 12px;\n";
    html << "    border-radius: 8px;\n";
    html << "    overflow-x: auto;\n";
    html << "}\n\n";
    
    html << ".preview-pane img {\n";
    html << "    max-width: 100%;\n";
    html << "    height: auto;\n";
    html << "    border-radius: 8px;\n";
    html << "}\n\n";
    
    html << "</style>\n";
    html << "</head>\n";
    html << "<body>\n";
    
    html << "<div class=\"editor-container\">\n";
    html << "<div class=\"editor-header\">\n";
    html << "<h1 class=\"editor-title\">" << (is_edit ? "✏️ 编辑文章" : "📝 新建文章") << "</h1>\n";
    html << "<p class=\"editor-subtitle\">使用 Markdown 语法编写优质内容</p>\n";
    html << "<nav class=\"admin-nav\">\n";
    html << "<a href=\"/blog/admin/\">📊 仪表盘</a>\n";
    html << "<a href=\"/blog/admin/new\">➕ 新建文章</a>\n";
    html << "<a href=\"/blog/\">🏠 返回前台</a>\n";
    html << "</nav>\n";
    html << "</div>\n";
    
    html << "<div class=\"editor-content\">\n";
    html << "<form id=\"article-form\" method=\"post\" action=\"" 
         << (is_edit ? "/blog/api/articles/" + to_string(article_id) : "/blog/api/articles") << "\">\n";
    
    if (is_edit) {
        html << "<input type=\"hidden\" name=\"_method\" value=\"PUT\">\n";
    }
    
    html << "<div class=\"editor-form\">\n";
    html << "<div class=\"editor-main\">\n";
    
    // 标题
    html << "<div class=\"form-group\">\n";
    html << "<label for=\"title\">📝 文章标题 *</label>\n";
    html << "<input type=\"text\" id=\"title\" name=\"title\" required placeholder=\"请输入一个吸引人的标题\" ";
    if (is_edit) {
        html << "value=\"" << html_escape(article.title) << "\" ";
    }
    html << ">\n";
    html << "</div>\n";
    
    // 摘要
    html << "<div class=\"form-group\">\n";
    html << "<label for=\"summary\">📄 文章摘要</label>\n";
    html << "<textarea id=\"summary\" name=\"summary\" rows=\"3\" placeholder=\"简要描述文章内容，帮助读者快速了解（可选）\">";
    if (is_edit) {
        html << html_escape(article.summary);
    }
    html << "</textarea>\n";
    html << "</div>\n";
    
    // 内容类型选择器
    html << "<div class=\"form-group\">\n";
    html << "<label for=\"content_type\">📝 内容格式</label>\n";
    html << "<select id=\"content_type\" name=\"content_type\" onchange=\"toggleEditorMode()\">\n";
    html << "<option value=\"markdown\"" << (is_edit && article.content_type == "markdown" ? " selected" : (!is_edit ? " selected" : "")) << ">🎯 Markdown</option>\n";
    html << "<option value=\"html\"" << (is_edit && article.content_type == "html" ? " selected" : "") << ">🌐 HTML</option>\n";
    html << "</select>\n";
    html << "</div>\n";
    
    // Markdown工具栏
    html << "<div id=\"markdown-toolbar\" class=\"markdown-toolbar\">\n";
    html << "<button type=\"button\" onclick=\"insertMarkdown('**', '**')\" title=\"加粗\">🔹 B</button>\n";
    html << "<button type=\"button\" onclick=\"insertMarkdown('*', '*')\" title=\"斜体\">🔸 I</button>\n";
    html << "<button type=\"button\" onclick=\"insertMarkdown('`', '`')\" title=\"行内代码\">💻 Code</button>\n";
    html << "<button type=\"button\" onclick=\"insertMarkdown('\\n```\\n', '\\n```\\n')\" title=\"代码块\">📋 Block</button>\n";
    html << "<button type=\"button\" onclick=\"insertMarkdown('[', '](url)')\" title=\"链接\">🔗 Link</button>\n";
    html << "<button type=\"button\" id=\"image-btn\" onclick=\"showImageUpload()\" title=\"插入图片\">📷 Image</button>\n";
    html << "<button type=\"button\" onclick=\"insertMarkdown('\\n# ', '')\" title=\"标题\">📝 H1</button>\n";
    html << "<button type=\"button\" onclick=\"insertMarkdown('\\n- ', '')\" title=\"列表\">📋 List</button>\n";
    html << "<button type=\"button\" onclick=\"togglePreview()\" title=\"预览\">👁️ Preview</button>\n";
    html << "</div>\n";
    
    // 图片上传区域
    html << "<div id=\"image-upload-area\" class=\"image-upload-area\" style=\"display: none;\">\n";
    html << "<input type=\"file\" id=\"image-file\" accept=\"image/*\" onchange=\"uploadImage()\">\n";
    html << "<div id=\"upload-progress\" class=\"upload-progress\"></div>\n";
    html << "</div>\n";
    
    // 编辑器容器
    html << "<div class=\"editor-container-inner\">\n";
    html << "<div class=\"form-group editor-group\">\n";
    html << "<label for=\"content\">✍️ 文章内容 *</label>\n";
    html << "<div class=\"editor-wrapper\">\n";
    html << "<textarea id=\"content\" name=\"content\" class=\"content-editor\" rows=\"20\" required placeholder=\"在这里开始编写您的精彩内容...\\n\\n支持 Markdown 语法：\\n- # 标题\\n- **粗体** *斜体*\\n- `代码`\\n- [链接](url)\\n- ![图片](url)\">";
    if (is_edit) {
        html << html_escape(article.content);
    }
    html << "</textarea>\n";
    html << "<div id=\"preview-pane\" class=\"preview-pane\" style=\"display: none;\"></div>\n";
    html << "</div>\n";
    html << "</div>\n";
    html << "</div>\n";
    
    html << "</div>\n"; // editor-main
    
    html << "<div class=\"editor-sidebar\">\n";
    
    // 发布设置
    html << "<h3>⚙️ 发布设置</h3>\n";
    
    // 状态
    html << "<div class=\"form-group\">\n";
    html << "<label for=\"status\">📊 发布状态</label>\n";
    html << "<select id=\"status\" name=\"status\">\n";
    html << "<option value=\"draft\"" << (is_edit && article.status == "draft" ? " selected" : "") << ">📄 草稿</option>\n";
    html << "<option value=\"published\"" << (is_edit && article.status == "published" ? " selected" : "") << ">🌟 已发布</option>\n";
    html << "</select>\n";
    html << "</div>\n";
    
    // 分类
    html << "<div class=\"form-group\">\n";
    html << "<label for=\"category_id\">📂 文章分类</label>\n";
    html << "<select id=\"category_id\" name=\"category_id\">\n";
    html << "<option value=\"\">🔍 选择分类</option>\n";
    for (const auto& category : categories) {
        html << "<option value=\"" << category.category_id << "\"";
        if (is_edit && article.category_id == category.category_id) {
            html << " selected";
        }
        html << ">" << html_escape(category.name) << "</option>\n";
    }
    html << "</select>\n";
    html << "</div>\n";
    
    // 标签
    html << "<div class=\"form-group\">\n";
    html << "<label for=\"tags\">🏷️ 文章标签</label>\n";
    html << "<input type=\"text\" id=\"tags\" name=\"tags\" placeholder=\"技术, 前端, React（用逗号分隔）\">\n";
    html << "</div>\n";
    
    // 操作按钮
    html << "<div class=\"form-group\">\n";
    html << "<button type=\"submit\" class=\"btn btn-success\">💾 保存文章</button>\n";
    if (is_edit) {
        html << "<a href=\"/blog/article/" << article_id << "\" class=\"btn btn-secondary\">👀 预览</a>\n";
    }
    html << "<a href=\"/blog/admin/\" class=\"btn btn-secondary\">❌ 取消</a>\n";
    html << "</div>\n";
    
    html << "</div>\n"; // editor-sidebar
    html << "</div>\n"; // editor-form
    html << "</form>\n";
    html << "</div>\n"; // editor-content
    html << "</div>\n"; // editor-container
    
    // JavaScript
    html << "<script>\n";
    html << "document.getElementById('article-form').addEventListener('submit', function(e) {\n";
    html << "    e.preventDefault();\n";
    html << "    \n";
    html << "    const formData = new FormData(this);\n";
    html << "    const method = formData.get('_method') || 'POST';\n";
    html << "    const url = this.action;\n";
    html << "    \n";
    html << "    // 显示保存状态\n";
    html << "    const submitBtn = this.querySelector('button[type=\"submit\"]');\n";
    html << "    const originalText = submitBtn.textContent;\n";
    html << "    submitBtn.textContent = '保存中...';\n";
    html << "    submitBtn.disabled = true;\n";
    html << "    \n";
    html << "    // 将FormData转换为URLSearchParams以便正确发送\n";
    html << "    const params = new URLSearchParams();\n";
    html << "    for (const [key, value] of formData.entries()) {\n";
    html << "        if (key !== '_method') {\n";
    html << "            params.append(key, value);\n";
    html << "        }\n";
    html << "    }\n";
    html << "    \n";
    html << "    fetch(url, {\n";
    html << "        method: method,\n";
    html << "        headers: {\n";
    html << "            'Content-Type': 'application/x-www-form-urlencoded'\n";
    html << "        },\n";
    html << "        body: params.toString()\n";
    html << "    })\n";
    html << "    .then(response => {\n";
    html << "        console.log('Response status:', response.status);\n";
    html << "        console.log('Response headers:', response.headers);\n";
    html << "        return response.text().then(text => {\n";
    html << "            console.log('Response text:', text);\n";
    html << "            try {\n";
    html << "                return JSON.parse(text);\n";
    html << "            } catch (e) {\n";
    html << "                console.error('JSON parse error:', e);\n";
    html << "                throw new Error('Invalid JSON response: ' + text);\n";
    html << "            }\n";
    html << "        });\n";
    html << "    })\n";
    html << "    .then(data => {\n";
    html << "        console.log('Parsed data:', data);\n";
    html << "        if (data.success) {\n";
    html << "            alert('文章保存成功！');\n";
    html << "            if (data.article_id) {\n";
    html << "                window.location.href = '/blog/admin/edit/' + data.article_id;\n";
    html << "            } else {\n";
    html << "                window.location.href = '/blog/admin/';\n";
    html << "            }\n";
    html << "        } else {\n";
    html << "            alert('保存失败：' + (data.message || '未知错误'));\n";
    html << "        }\n";
    html << "    })\n";
    html << "    .catch(error => {\n";
    html << "        console.error('Request error:', error);\n";
    html << "        alert('保存失败：' + error.message);\n";
    html << "    })\n";
    html << "    .finally(() => {\n";
    html << "        submitBtn.textContent = originalText;\n";
    html << "        submitBtn.disabled = false;\n";
    html << "    });\n";
    html << "});\n";
    html << "\n";
    html << "// Markdown编辑器功能\n";
    html << "function toggleEditorMode() {\n";
    html << "    const contentType = document.getElementById('content_type').value;\n";
    html << "    const markdownToolbar = document.getElementById('markdown-toolbar');\n";
    html << "    const previewPane = document.getElementById('preview-pane');\n";
    html << "    const editorWrapper = document.querySelector('.editor-wrapper');\n";
    html << "    \n";
    html << "    if (contentType === 'markdown') {\n";
    html << "        markdownToolbar.style.display = 'flex';\n";
    html << "    } else {\n";
    html << "        markdownToolbar.style.display = 'none';\n";
    html << "        previewPane.style.display = 'none';\n";
    html << "        editorWrapper.classList.remove('preview-mode');\n";
    html << "    }\n";
    html << "}\n";
    html << "\n";
    html << "function insertMarkdown(before, after) {\n";
    html << "    const textarea = document.getElementById('content');\n";
    html << "    const start = textarea.selectionStart;\n";
    html << "    const end = textarea.selectionEnd;\n";
    html << "    const selectedText = textarea.value.substring(start, end);\n";
    html << "    const newText = before + selectedText + after;\n";
    html << "    \n";
    html << "    textarea.value = textarea.value.substring(0, start) + newText + textarea.value.substring(end);\n";
    html << "    textarea.focus();\n";
    html << "    textarea.setSelectionRange(start + before.length, start + before.length + selectedText.length);\n";
    html << "    \n";
    html << "    updatePreview();\n";
    html << "}\n";
    html << "\n";
    html << "function showImageUpload() {\n";
    html << "    const uploadArea = document.getElementById('image-upload-area');\n";
    html << "    uploadArea.style.display = uploadArea.style.display === 'none' ? 'block' : 'none';\n";
    html << "}\n";
    html << "\n";
    html << "function uploadImage() {\n";
    html << "    const fileInput = document.getElementById('image-file');\n";
    html << "    const file = fileInput.files[0];\n";
    html << "    const progress = document.getElementById('upload-progress');\n";
    html << "    \n";
    html << "    if (!file) return;\n";
    html << "    \n";
    html << "    if (!file.type.startsWith('image/')) {\n";
    html << "        progress.textContent = '❌ 请选择图片文件';\n";
    html << "        return;\n";
    html << "    }\n";
    html << "    \n";
    html << "    progress.textContent = '📤 上传中...';\n";
    html << "    \n";
    html << "    const formData = new FormData();\n";
    html << "    formData.append('file', file);\n";
    html << "    \n";
    html << "    fetch('/blog/api/upload/image', {\n";
    html << "        method: 'POST',\n";
    html << "        body: formData\n";
    html << "    })\n";
    html << "    .then(response => response.json())\n";
    html << "    .then(data => {\n";
    html << "        if (data.success) {\n";
    html << "            const imageMarkdown = `![${file.name}](${data.url})`;\n";
    html << "            insertMarkdown('', imageMarkdown);\n";
    html << "            progress.textContent = '✅ 上传成功!';\n";
    html << "            fileInput.value = '';\n";
    html << "            setTimeout(() => {\n";
    html << "                document.getElementById('image-upload-area').style.display = 'none';\n";
    html << "                progress.textContent = '';\n";
    html << "            }, 2000);\n";
    html << "        } else {\n";
    html << "            progress.textContent = '❌ ' + data.message;\n";
    html << "        }\n";
    html << "    })\n";
    html << "    .catch(error => {\n";
    html << "        progress.textContent = '❌ 上传失败: ' + error.message;\n";
    html << "    });\n";
    html << "}\n";
    html << "\n";
    html << "let previewMode = false;\n";
    html << "function togglePreview() {\n";
    html << "    const contentType = document.getElementById('content_type').value;\n";
    html << "    if (contentType !== 'markdown') return;\n";
    html << "    \n";
    html << "    previewMode = !previewMode;\n";
    html << "    const editorWrapper = document.querySelector('.editor-wrapper');\n";
    html << "    const previewPane = document.getElementById('preview-pane');\n";
    html << "    \n";
    html << "    if (previewMode) {\n";
    html << "        editorWrapper.classList.add('preview-mode');\n";
    html << "        previewPane.style.display = 'block';\n";
    html << "        updatePreview();\n";
    html << "    } else {\n";
    html << "        editorWrapper.classList.remove('preview-mode');\n";
    html << "        previewPane.style.display = 'none';\n";
    html << "    }\n";
    html << "}\n";
    html << "\n";
    html << "function updatePreview() {\n";
    html << "    if (!previewMode) return;\n";
    html << "    \n";
    html << "    const content = document.getElementById('content').value;\n";
    html << "    const previewPane = document.getElementById('preview-pane');\n";
    html << "    \n";
    html << "    // 简单的Markdown到HTML转换（客户端预览）\n";
    html << "    let html = content\n";
    html << "        .replace(/^### (.*$)/gim, '<h3>$1</h3>')\n";
    html << "        .replace(/^## (.*$)/gim, '<h2>$1</h2>')\n";
    html << "        .replace(/^# (.*$)/gim, '<h1>$1</h1>')\n";
    html << "        .replace(/\\*\\*(.*?)\\*\\*/g, '<strong>$1</strong>')\n";
    html << "        .replace(/\\*(.*?)\\*/g, '<em>$1</em>')\n";
    html << "        .replace(/`(.*?)`/g, '<code>$1</code>')\n";
    html << "        .replace(/\\[([^\\]]+)\\]\\(([^\\)]+)\\)/g, '<a href=\"$2\" target=\"_blank\">$1</a>')\n";
    html << "        .replace(/!\\[([^\\]]*)\\]\\(([^\\)]+)\\)/g, '<img src=\"$2\" alt=\"$1\" style=\"max-width:100%;height:auto;\">')\n";
    html << "        .replace(/\\n/g, '<br>');\n";
    html << "    \n";
    html << "    previewPane.innerHTML = html;\n";
    html << "}\n";
    html << "\n";
    html << "// 内容变化时自动更新预览\n";
    html << "document.getElementById('content').addEventListener('input', updatePreview);\n";
    html << "\n";
    html << "// 页面加载时初始化编辑器模式\n";
    html << "document.addEventListener('DOMContentLoaded', function() {\n";
    html << "    toggleEditorMode();\n";
    html << "});\n";
    html << "\n";
    html << "</script>\n";
    
    html << "</body>\n";
    html << "</html>\n";
    
    return build_html_response(html.str());
}
string BlogHandler::api_get_articles(int page, int category_id) { return ""; }
string BlogHandler::api_get_article(int article_id) { return ""; }
string BlogHandler::api_create_article(const string& post_data) {
    printf("Debug: api_create_article called\n");
    printf("Debug: post_data length: %zu\n", post_data.length());
    
    if (!m_conn_pool) {
        printf("Debug: 数据库连接池为空\n");
        return build_json_response("{\"success\":false,\"message\":\"数据库连接失败\"}", 500);
    }
    
    // 解析POST数据
    map<string, string> form_data = parse_post_data(post_data);
    printf("Debug: 解析到的字段数量: %zu\n", form_data.size());
    
    for (const auto& pair : form_data) {
        printf("Debug: 字段 %s = %s (长度: %zu)\n", pair.first.c_str(), 
               pair.second.substr(0, 100).c_str(), pair.second.length());
    }
    
    // 验证必需字段
    if (form_data["title"].empty() || form_data["content"].empty()) {
        printf("Debug: 必需字段为空 - title: %s, content: %s\n", 
               form_data["title"].empty() ? "empty" : "not empty",
               form_data["content"].empty() ? "empty" : "not empty");
        return build_json_response("{\"success\":false,\"message\":\"标题和内容不能为空\"}", 400);
    }
    
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, m_conn_pool);
    
    // 使用预处理语句防止SQL注入和处理长文本
    const char* sql = "INSERT INTO articles (title, content, content_type, summary, category_id, status, author_id, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, NOW())";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    
    if (!stmt) {
        printf("mysql_stmt_init() failed\n");
        return build_json_response("{\"success\":false,\"message\":\"数据库预处理失败\"}", 500);
    }
    
    if (mysql_stmt_prepare(stmt, sql, strlen(sql))) {
        printf("mysql_stmt_prepare() failed: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return build_json_response("{\"success\":false,\"message\":\"SQL预处理失败\"}", 500);
    }
    
    // 绑定参数
    MYSQL_BIND bind[7];
    memset(bind, 0, sizeof(bind));
    
    string title = form_data["title"];
    string content = form_data["content"];
    string content_type = form_data["content_type"].empty() ? "markdown" : form_data["content_type"];
    string summary = form_data["summary"];
    string status = form_data["status"].empty() ? "draft" : form_data["status"];
    int category_id = form_data["category_id"].empty() || form_data["category_id"] == "0" ? 0 : atoi(form_data["category_id"].c_str());
    int author_id = 1;
    
    unsigned long title_length = title.length();
    unsigned long content_length = content.length();
    unsigned long content_type_length = content_type.length();
    unsigned long summary_length = summary.length();
    unsigned long status_length = status.length();
    
    // title
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)title.c_str();
    bind[0].buffer_length = title_length;
    bind[0].length = &title_length;
    
    // content
    bind[1].buffer_type = MYSQL_TYPE_LONG_BLOB;
    bind[1].buffer = (char*)content.c_str();
    bind[1].buffer_length = content_length;
    bind[1].length = &content_length;
    
    // content_type
    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char*)content_type.c_str();
    bind[2].buffer_length = content_type_length;
    bind[2].length = &content_type_length;
    
    // summary
    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (char*)summary.c_str();
    bind[3].buffer_length = summary_length;
    bind[3].length = &summary_length;
    
    // category_id
    bind[4].buffer_type = MYSQL_TYPE_LONG;
    bind[4].buffer = &category_id;
    bool category_is_null = (category_id == 0);
    bind[4].is_null = category_is_null ? &category_is_null : 0;
    
    // status
    bind[5].buffer_type = MYSQL_TYPE_STRING;
    bind[5].buffer = (char*)status.c_str();
    bind[5].buffer_length = status_length;
    bind[5].length = &status_length;
    
    // author_id
    bind[6].buffer_type = MYSQL_TYPE_LONG;
    bind[6].buffer = &author_id;
    
    if (mysql_stmt_bind_param(stmt, bind)) {
        printf("mysql_stmt_bind_param() failed: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return build_json_response("{\"success\":false,\"message\":\"参数绑定失败\"}", 500);
    }
    
    if (mysql_stmt_execute(stmt)) {
        printf("mysql_stmt_execute() failed: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return build_json_response("{\"success\":false,\"message\":\"文章创建失败\"}", 500);
    }
    
    int article_id = mysql_stmt_insert_id(stmt);
    mysql_stmt_close(stmt);
    
    // 更新发布时间
    if (status == "published") {
        stringstream update_query;
        update_query << "UPDATE articles SET published_at = NOW() WHERE article_id = " << article_id;
        mysql_query(mysql, update_query.str().c_str());
    }
    
    stringstream response;
    response << "{\"success\":true,\"message\":\"文章创建成功\",\"article_id\":" << article_id << "}";
    return build_json_response(response.str());
}
string BlogHandler::api_update_article(int article_id, const string& post_data) {
    printf("Debug: api_update_article called, article_id: %d\n", article_id);
    printf("Debug: post_data: %s\n", post_data.c_str());
    
    if (!m_conn_pool) {
        printf("Debug: 数据库连接池为空\n");
        return build_json_response("{\"success\":false,\"message\":\"数据库连接失败\"}", 500);
    }
    
    // 解析POST数据
    map<string, string> form_data = parse_post_data(post_data);
    printf("Debug: 解析到的字段数量: %zu\n", form_data.size());
    
    // 验证必需字段
    if (form_data["title"].empty() || form_data["content"].empty()) {
        printf("Debug: 必需字段为空\n");
        return build_json_response("{\"success\":false,\"message\":\"标题和内容不能为空\"}", 400);
    }
    
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, m_conn_pool);
    
    // 暂时使用简单SQL
    stringstream query;
    query << "UPDATE articles SET ";
    query << "title = '" << html_escape(form_data["title"]) << "', ";
    query << "content = '" << html_escape(form_data["content"]) << "', ";
    query << "content_type = '" << (form_data["content_type"].empty() ? "markdown" : form_data["content_type"]) << "', ";
    query << "summary = '" << html_escape(form_data["summary"]) << "', ";
    
    if (!form_data["category_id"].empty() && form_data["category_id"] != "0") {
        query << "category_id = " << form_data["category_id"] << ", ";
    } else {
        query << "category_id = NULL, ";
    }
    
    string status = form_data["status"].empty() ? "draft" : form_data["status"];
    query << "status = '" << status << "', ";
    query << "updated_at = NOW()";
    
    if (status == "published") {
        query << ", published_at = COALESCE(published_at, NOW())";
    }
    
    query << " WHERE article_id = " << article_id;
    
    printf("Debug: 执行SQL\n");
    
    if (mysql_query(mysql, query.str().c_str()) == 0) {
        int affected_rows = mysql_affected_rows(mysql);
        printf("Debug: SQL执行成功，影响行数: %d\n", affected_rows);
        
        if (affected_rows > 0) {
            printf("Debug: 文章更新成功\n");
            return build_json_response("{\"success\":true,\"message\":\"文章更新成功\"}");
        } else {
            printf("Debug: 没有找到要更新的文章\n");
            return build_json_response("{\"success\":false,\"message\":\"文章不存在\"}", 404);
        }
    } else {
        printf("SQL Error: %s\n", mysql_error(mysql));
        return build_json_response("{\"success\":false,\"message\":\"数据库操作失败\"}", 500);
    }
}
string BlogHandler::api_delete_article(int article_id) {
    if (!m_conn_pool) {
        return build_json_response("{\"success\":false,\"message\":\"数据库连接失败\"}", 500);
    }
    
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, m_conn_pool);
    
    // 删除文章（级联删除评论和点赞记录）
    stringstream query;
    query << "DELETE FROM articles WHERE article_id = " << article_id;
    
    
    if (mysql_query(mysql, query.str().c_str()) == 0) {
        int affected_rows = mysql_affected_rows(mysql);
        
        if (affected_rows > 0) {
            return build_json_response("{\"success\":true,\"message\":\"文章删除成功\"}");
        } else {
            return build_json_response("{\"success\":false,\"message\":\"文章不存在\"}", 404);
        }
    } else {
        printf("SQL Error: %s\n", mysql_error(mysql));
        return build_json_response("{\"success\":false,\"message\":\"数据库操作失败\"}", 500);
    }
}
string BlogHandler::api_add_comment(const string& post_data) {
    if (!m_conn_pool) {
        return build_json_response("{\"success\":false,\"message\":\"数据库连接失败\"}", 500);
    }
    
    // 解析POST数据
    map<string, string> form_data = parse_post_data(post_data);
    
    // 验证必需字段
    if (form_data["article_id"].empty() || form_data["content"].empty() || form_data["user_name"].empty()) {
        return build_json_response("{\"success\":false,\"message\":\"文章ID、内容和用户名不能为空\"}", 400);
    }
    
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, m_conn_pool);
    
    // 验证文章是否存在
    int article_id = atoi(form_data["article_id"].c_str());
    stringstream check_query;
    check_query << "SELECT article_id FROM articles WHERE article_id = " << article_id;
    
    if (mysql_query(mysql, check_query.str().c_str()) != 0) {
        return build_json_response("{\"success\":false,\"message\":\"数据库查询失败\"}", 500);
    }
    
    MYSQL_RES* result = mysql_store_result(mysql);
    if (!result || mysql_num_rows(result) == 0) {
        if (result) mysql_free_result(result);
        return build_json_response("{\"success\":false,\"message\":\"文章不存在\"}", 404);
    }
    mysql_free_result(result);
    
    // 插入评论
    stringstream query;
    query << "INSERT INTO comments (article_id, user_name, email, content, ip_address, created_at) VALUES (";
    query << article_id << ", ";
    query << "'" << html_escape(form_data["user_name"]) << "', ";
    query << "'" << html_escape(form_data["email"]) << "', ";
    query << "'" << html_escape(form_data["content"]) << "', ";
    query << "'127.0.0.1', ";  // 简化处理，实际应该获取真实IP
    query << "NOW())";
    
    if (mysql_query(mysql, query.str().c_str()) == 0) {
        int comment_id = mysql_insert_id(mysql);
        stringstream response;
        response << "{\"success\":true,\"message\":\"评论发布成功\",\"comment_id\":" << comment_id << "}";
        return build_json_response(response.str());
    } else {
        printf("SQL Error: %s\n", mysql_error(mysql));
        return build_json_response("{\"success\":false,\"message\":\"评论发布失败\"}", 500);
    }
}
string BlogHandler::api_toggle_like(const string& post_data) { return ""; }
vector<Tag> BlogHandler::get_tags() { return vector<Tag>(); }
string BlogHandler::parse_url_param(const string& url, const string& param) {
    size_t question_pos = url.find('?');
    if (question_pos == string::npos) {
        return "";
    }
    
    string query_string = url.substr(question_pos + 1);
    string search_param = param + "=";
    
    size_t param_pos = query_string.find(search_param);
    if (param_pos == string::npos) {
        return "";
    }
    
    size_t value_start = param_pos + search_param.length();
    size_t value_end = query_string.find('&', value_start);
    
    if (value_end == string::npos) {
        return query_string.substr(value_start);
    } else {
        return query_string.substr(value_start, value_end - value_start);
    }
}
string BlogHandler::url_decode(const string& str) {
    string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            // 解码十六进制
            char hex[3] = {str[i+1], str[i+2], '\0'};
            char* endptr;
            unsigned char c = static_cast<unsigned char>(strtol(hex, &endptr, 16));
            if (*endptr == '\0') {
                result += c;
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    
    return result;
}
string BlogHandler::json_escape(const string& str) { return str; }
map<string, string> BlogHandler::parse_post_data(const string& data) {
    map<string, string> result;
    
    if (data.empty()) {
        return result;
    }
    
    // 分割参数 key1=value1&key2=value2
    size_t start = 0;
    size_t end = 0;
    
    while ((end = data.find('&', start)) != string::npos) {
        string pair = data.substr(start, end - start);
        size_t equal_pos = pair.find('=');
        if (equal_pos != string::npos) {
            string key = url_decode(pair.substr(0, equal_pos));
            string value = url_decode(pair.substr(equal_pos + 1));
            result[key] = value;
        }
        start = end + 1;
    }
    
    // 处理最后一个参数
    if (start < data.length()) {
        string pair = data.substr(start);
        size_t equal_pos = pair.find('=');
        if (equal_pos != string::npos) {
            string key = url_decode(pair.substr(0, equal_pos));
            string value = url_decode(pair.substr(equal_pos + 1));
            result[key] = value;
        }
    }
    
    return result;
}
string BlogHandler::build_json_response(const string& json_data, int status_code) {
    string result = json_data;
    return result;
}
string BlogHandler::generate_session_id() { return ""; }


// Session验证功能实现
void BlogHandler::set_session_functions(bool (*validate_func)(const string&), 
                                       string (*username_func)(const string&),
                                       string (*role_func)(const string&)) {
    validate_session_func = validate_func;
    get_username_func = username_func;
    get_role_func = role_func;
}

string BlogHandler::extract_session_id(const string& cookie_header) {
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

bool BlogHandler::is_logged_in(const string& cookie_header) {
    if (!validate_session_func) return false;
    
    string session_id = extract_session_id(cookie_header);
    return validate_session_func(session_id);
}

string BlogHandler::get_current_user(const string& cookie_header) {
    if (!get_username_func) return "";
    
    string session_id = extract_session_id(cookie_header);
    return get_username_func(session_id);
}

string BlogHandler::get_user_role(const string& cookie_header) {
    if (!get_role_func) return "guest";
    
    string session_id = extract_session_id(cookie_header);
    return get_role_func(session_id);
}

bool BlogHandler::is_admin_user(const string& cookie_header) {
    return get_user_role(cookie_header) == "admin";
}

bool BlogHandler::check_user_permission(const string& cookie_header, const string& required_role) {
    if (required_role == "guest") {
        return true; // guest权限不需要登录
    }
    
    if (!is_logged_in(cookie_header)) {
        return false;
    }
    
    string user_role = get_user_role(cookie_header);
    
    if (required_role == "admin") {
        return user_role == "admin";
    }
    
    return true; // 其他情况默认允许
}

// Markdown和图片处理方法
string BlogHandler::api_upload_image(const string& content_type, const string& post_data) {
    UploadResult result = image_uploader->uploadImage(content_type, post_data);
    
    stringstream json;
    json << "{"
         << "\"success\":" << (result.success ? "true" : "false") << ","
         << "\"message\":\"" << html_escape(result.message) << "\"";
    
    if (result.success) {
        json << ",\"url\":\"" << html_escape(result.file_url) << "\""
             << ",\"size\":" << result.file_size;
    }
    
    json << "}";
    
    return build_json_response(json.str());
}

string BlogHandler::render_content(const string& content, const string& content_type) {
    if (content_type == "markdown") {
        return markdown_parser->parse(content);
    } else {
        // 默认为HTML，直接返回
        return content;
    }
}