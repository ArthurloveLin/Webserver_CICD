-- 博客系统数据迁移脚本（最终修复版）
-- 适配现有webserver数据库结构

-- 使用webserver数据库
USE webserver;

-- 1. 备份现有user表数据
CREATE TEMPORARY TABLE user_backup AS SELECT * FROM user;

-- 2. 重建user表结构
DROP TABLE user;
CREATE TABLE user (
    user_id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    passwd VARCHAR(50) NOT NULL,
    email VARCHAR(100),
    role ENUM('admin', 'guest') DEFAULT 'guest',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP NULL,
    INDEX idx_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 3. 恢复user表数据
INSERT INTO user (username, passwd, email, role) 
SELECT username, passwd, 
       CASE WHEN username = 'admin' THEN 'admin@example.com' ELSE NULL END,
       CASE WHEN username = 'admin' THEN 'admin' ELSE 'guest' END
FROM user_backup;

-- 如果没有admin用户，创建一个
INSERT IGNORE INTO user (username, passwd, email, role) VALUES 
('admin', 'admin123', 'admin@example.com', 'admin');

-- 4. 删除可能存在的博客相关表
DROP TABLE IF EXISTS article_tags;
DROP TABLE IF EXISTS comments;
DROP TABLE IF EXISTS likes;
DROP TABLE IF EXISTS articles;
DROP TABLE IF EXISTS admin_settings;

-- 5. 重建categories表
DROP TABLE IF EXISTS categories;
CREATE TABLE categories (
    category_id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100) NOT NULL UNIQUE,
    description TEXT,
    article_count INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 6. 重建tags表
DROP TABLE IF EXISTS tags;
CREATE TABLE tags (
    tag_id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(50) NOT NULL UNIQUE,
    article_count INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 7. 创建文章表
CREATE TABLE articles (
    article_id INT AUTO_INCREMENT PRIMARY KEY,
    title VARCHAR(255) NOT NULL,
    content LONGTEXT NOT NULL,
    summary TEXT,
    author_id INT,
    category_id INT,
    status ENUM('draft', 'published') DEFAULT 'draft',
    view_count INT DEFAULT 0,
    like_count INT DEFAULT 0,
    comment_count INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    published_at TIMESTAMP NULL,
    FOREIGN KEY (author_id) REFERENCES user(user_id) ON DELETE SET NULL,
    FOREIGN KEY (category_id) REFERENCES categories(category_id) ON DELETE SET NULL,
    INDEX idx_status_published (status, published_at),
    INDEX idx_category (category_id),
    INDEX idx_author (author_id),
    INDEX idx_created (created_at),
    FULLTEXT idx_title_content (title, content)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 8. 创建文章标签关系表
CREATE TABLE article_tags (
    article_id INT,
    tag_id INT,
    PRIMARY KEY (article_id, tag_id),
    FOREIGN KEY (article_id) REFERENCES articles(article_id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(tag_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 9. 创建评论表
CREATE TABLE comments (
    comment_id INT AUTO_INCREMENT PRIMARY KEY,
    article_id INT NOT NULL,
    user_name VARCHAR(50) NOT NULL,
    email VARCHAR(100),
    content TEXT NOT NULL,
    parent_id INT DEFAULT NULL,
    like_count INT DEFAULT 0,
    status ENUM('pending', 'approved', 'rejected') DEFAULT 'approved',
    ip_address VARCHAR(45),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (article_id) REFERENCES articles(article_id) ON DELETE CASCADE,
    FOREIGN KEY (parent_id) REFERENCES comments(comment_id) ON DELETE CASCADE,
    INDEX idx_article_status (article_id, status),
    INDEX idx_parent (parent_id),
    INDEX idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 10. 创建点赞记录表
CREATE TABLE likes (
    like_id INT AUTO_INCREMENT PRIMARY KEY,
    user_identifier VARCHAR(100) NOT NULL,
    target_type ENUM('article', 'comment') NOT NULL,
    target_id INT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY unique_like (user_identifier, target_type, target_id),
    INDEX idx_target (target_type, target_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 11. 创建管理员配置表
CREATE TABLE admin_settings (
    setting_id INT AUTO_INCREMENT PRIMARY KEY,
    setting_key VARCHAR(100) NOT NULL UNIQUE,
    setting_value TEXT,
    description VARCHAR(255),
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 12. 插入默认数据
-- 创建默认分类
INSERT INTO categories (name, description) VALUES 
('技术', '技术相关文章'),
('生活', '生活随笔'),
('学习', '学习笔记'),
('C++', 'C++编程相关'),
('Web开发', 'Web开发技术');

-- 创建默认标签
INSERT INTO tags (name) VALUES 
('C++'), ('Web开发'), ('数据库'), ('Linux'), ('算法'), ('TinyWebServer'), ('博客系统');

-- 插入测试文章
INSERT INTO articles (title, content, summary, author_id, category_id, status, created_at, published_at) VALUES 
('TinyWebServer博客系统开发日志', 
'# TinyWebServer博客系统开发日志

今天完成了TinyWebServer博客系统的核心功能开发，包括：

## 已实现功能
1. 文章管理系统
2. 分类和标签管理
3. 用户权限控制
4. 模板引擎
5. 静态资源服务

## 技术特点
- 基于C++的高性能Web服务器
- 支持并发处理
- MySQL数据库存储
- UTF-8中文支持

## 下一步计划
- 完善评论系统
- 添加搜索功能
- 优化性能

这是一个很好的学习项目，展示了Web开发的完整流程。',
'记录TinyWebServer博客系统的开发过程和技术要点', 
1, 1, 'published', NOW(), NOW()),

('C++Web服务器开发入门', 
'# C++Web服务器开发入门

## 什么是Web服务器？
Web服务器是处理HTTP请求并返回响应的程序。

## 核心组件
1. Socket编程
2. HTTP协议解析
3. 多线程/异步处理
4. 数据库连接

## TinyWebServer特色
- 线程池模式
- Epoll事件驱动
- 连接池管理
- 日志系统

这个项目很适合学习Web服务器的底层原理。',
'介绍C++Web服务器开发的基础知识和TinyWebServer的特色', 
1, 4, 'published', DATE_SUB(NOW(), INTERVAL 1 DAY), DATE_SUB(NOW(), INTERVAL 1 DAY)),

('博客系统数据库设计', 
'# 博客系统数据库设计

## 核心表结构

### 用户表 (user)
- 用户认证
- 权限管理

### 文章表 (articles)  
- 文章内容
- 状态管理
- 分类关联

### 分类表 (categories)
- 文章分类
- 层级结构

### 评论表 (comments)
- 评论内容
- 回复关系

## 设计原则
1. 规范化设计
2. 索引优化
3. 外键约束
4. 触发器自动更新

良好的数据库设计是系统性能的基础。',
'详细介绍博客系统的数据库表结构设计和优化策略', 
1, 3, 'draft', DATE_SUB(NOW(), INTERVAL 2 DAY), NULL);

-- 插入文章标签关系
INSERT INTO article_tags (article_id, tag_id) VALUES 
(1, 6), (1, 7), (1, 2),
(2, 1), (2, 2), (2, 6),
(3, 3), (3, 7);

-- 插入默认管理设置
INSERT INTO admin_settings (setting_key, setting_value, description) VALUES 
('site_title', 'TinyWebServer博客', '网站标题'),
('site_description', '基于TinyWebServer的个人博客系统', '网站描述'),
('articles_per_page', '10', '每页文章数量'),
('allow_comments', '1', '是否允许评论'),
('comment_need_approval', '0', '评论是否需要审核');

-- 13. 更新计数
UPDATE categories c SET article_count = (
    SELECT COUNT(*) FROM articles a 
    WHERE a.category_id = c.category_id AND a.status = 'published'
);

UPDATE tags t SET article_count = (
    SELECT COUNT(*) FROM article_tags at 
    JOIN articles a ON at.article_id = a.article_id 
    WHERE at.tag_id = t.tag_id AND a.status = 'published'
);

-- 14. 创建触发器
DELIMITER $$

CREATE TRIGGER update_category_count_after_insert 
AFTER INSERT ON articles FOR EACH ROW
BEGIN
    IF NEW.status = 'published' AND NEW.category_id IS NOT NULL THEN
        UPDATE categories SET article_count = article_count + 1 WHERE category_id = NEW.category_id;
    END IF;
END$$

CREATE TRIGGER update_category_count_after_update 
AFTER UPDATE ON articles FOR EACH ROW
BEGIN
    IF OLD.category_id != NEW.category_id OR (OLD.category_id IS NULL AND NEW.category_id IS NOT NULL) OR (OLD.category_id IS NOT NULL AND NEW.category_id IS NULL) THEN
        IF OLD.category_id IS NOT NULL AND OLD.status = 'published' THEN
            UPDATE categories SET article_count = article_count - 1 WHERE category_id = OLD.category_id;
        END IF;
        IF NEW.category_id IS NOT NULL AND NEW.status = 'published' THEN
            UPDATE categories SET article_count = article_count + 1 WHERE category_id = NEW.category_id;
        END IF;
    ELSEIF OLD.status != NEW.status AND NEW.category_id IS NOT NULL THEN
        IF OLD.status != 'published' AND NEW.status = 'published' THEN
            UPDATE categories SET article_count = article_count + 1 WHERE category_id = NEW.category_id;
        ELSEIF OLD.status = 'published' AND NEW.status != 'published' THEN
            UPDATE categories SET article_count = article_count - 1 WHERE category_id = NEW.category_id;
        END IF;
    END IF;
END$$

CREATE TRIGGER update_category_count_after_delete 
AFTER DELETE ON articles FOR EACH ROW
BEGIN
    IF OLD.status = 'published' AND OLD.category_id IS NOT NULL THEN
        UPDATE categories SET article_count = article_count - 1 WHERE category_id = OLD.category_id;
    END IF;
END$$

CREATE TRIGGER update_article_comment_count_after_insert 
AFTER INSERT ON comments FOR EACH ROW
BEGIN
    UPDATE articles SET comment_count = comment_count + 1 WHERE article_id = NEW.article_id;
END$$

CREATE TRIGGER update_article_comment_count_after_delete 
AFTER DELETE ON comments FOR EACH ROW
BEGIN
    UPDATE articles SET comment_count = comment_count - 1 WHERE article_id = OLD.article_id;
END$$

DELIMITER ;

-- 15. 删除旧数据库（如果存在）
DROP DATABASE IF EXISTS tinywebserver_blog;

-- 16. 显示结果
SELECT '=== 迁移完成 ===' AS info;

SELECT '=== 用户表结构 ===' AS info;
DESCRIBE user;

SELECT '=== 分类数据 ===' AS info;
SELECT * FROM categories;

SELECT '=== 文章数据 ===' AS info;  
SELECT article_id, title, status, category_id, created_at FROM articles;

SELECT '=== 用户数据 ===' AS info;
SELECT user_id, username, role FROM user;