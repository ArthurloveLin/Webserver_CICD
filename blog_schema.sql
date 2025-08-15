-- TinyWebServer博客系统数据库表结构
-- 使用现有的webserver数据库
USE webserver;

-- 扩展现有用户表结构（假设已存在user表）
-- 如果user表不存在，则创建基础用户表
CREATE TABLE IF NOT EXISTS user (
    user_id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    passwd VARCHAR(50) NOT NULL,
    email VARCHAR(100),
    role ENUM('admin', 'guest') DEFAULT 'guest',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP NULL,
    INDEX idx_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 分类表
CREATE TABLE categories (
    category_id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100) NOT NULL UNIQUE,
    description TEXT,
    article_count INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 标签表
CREATE TABLE tags (
    tag_id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(50) NOT NULL UNIQUE,
    article_count INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 文章表
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

-- 文章标签关系表
CREATE TABLE article_tags (
    article_id INT,
    tag_id INT,
    PRIMARY KEY (article_id, tag_id),
    FOREIGN KEY (article_id) REFERENCES articles(article_id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(tag_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 评论表
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

-- 点赞记录表
CREATE TABLE likes (
    like_id INT AUTO_INCREMENT PRIMARY KEY,
    user_identifier VARCHAR(100) NOT NULL, -- IP地址或用户ID
    target_type ENUM('article', 'comment') NOT NULL,
    target_id INT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY unique_like (user_identifier, target_type, target_id),
    INDEX idx_target (target_type, target_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 管理员配置表
CREATE TABLE admin_settings (
    setting_id INT AUTO_INCREMENT PRIMARY KEY,
    setting_key VARCHAR(100) NOT NULL UNIQUE,
    setting_value TEXT,
    description VARCHAR(255),
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 插入默认数据
-- 创建管理员账户（密码需要加密处理）
INSERT INTO user (username, passwd, email, role) VALUES 
('admin', 'admin123', 'admin@example.com', 'admin')
ON DUPLICATE KEY UPDATE role = 'admin';

-- 创建默认分类
INSERT INTO categories (name, description) VALUES 
('技术', '技术相关文章'),
('生活', '生活随笔'),
('学习', '学习笔记')
ON DUPLICATE KEY UPDATE name = VALUES(name);

-- 创建默认标签
INSERT INTO tags (name) VALUES 
('C++'), ('Web开发'), ('数据库'), ('Linux'), ('算法')
ON DUPLICATE KEY UPDATE name = VALUES(name);

-- 插入默认管理设置
INSERT INTO admin_settings (setting_key, setting_value, description) VALUES 
('site_title', 'TinyWebServer博客', '网站标题'),
('site_description', '基于TinyWebServer的个人博客系统', '网站描述'),
('articles_per_page', '10', '每页文章数量'),
('allow_comments', '1', '是否允许评论'),
('comment_need_approval', '0', '评论是否需要审核')
ON DUPLICATE KEY UPDATE setting_value = VALUES(setting_value);

-- 创建触发器，更新文章相关计数
DELIMITER $$

-- 更新分类文章数量
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
    -- 如果分类改变
    IF OLD.category_id != NEW.category_id OR (OLD.category_id IS NULL AND NEW.category_id IS NOT NULL) OR (OLD.category_id IS NOT NULL AND NEW.category_id IS NULL) THEN
        -- 旧分类计数减1
        IF OLD.category_id IS NOT NULL AND OLD.status = 'published' THEN
            UPDATE categories SET article_count = article_count - 1 WHERE category_id = OLD.category_id;
        END IF;
        -- 新分类计数加1
        IF NEW.category_id IS NOT NULL AND NEW.status = 'published' THEN
            UPDATE categories SET article_count = article_count + 1 WHERE category_id = NEW.category_id;
        END IF;
    -- 如果只是状态改变
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

-- 更新文章评论数量
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