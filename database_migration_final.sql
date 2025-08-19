-- TinyWebServer博客系统数据库完整迁移脚本
-- 版本: 最终版 (包含Markdown支持)
-- 日期: 2025-08-19

USE webserver;

-- 开始事务
START TRANSACTION;

-- 1. 检查并创建用户表
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

-- 2. 检查并创建分类表
CREATE TABLE IF NOT EXISTS categories (
    category_id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100) NOT NULL UNIQUE,
    description TEXT,
    article_count INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 3. 检查并创建标签表
CREATE TABLE IF NOT EXISTS tags (
    tag_id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(50) NOT NULL UNIQUE,
    article_count INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 4. 检查并创建文章表（包含content_type字段）
CREATE TABLE IF NOT EXISTS articles (
    article_id INT AUTO_INCREMENT PRIMARY KEY,
    title VARCHAR(255) NOT NULL,
    content LONGTEXT NOT NULL,
    content_type ENUM('html', 'markdown') DEFAULT 'html',
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
    INDEX idx_status_published (status, published_at),
    INDEX idx_category (category_id),
    INDEX idx_author (author_id),
    INDEX idx_created (created_at),
    FULLTEXT idx_title_content (title, content)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 5. 添加content_type字段（如果文章表已存在但没有此字段）
-- 检查字段是否存在
SET @column_exists = 0;
SELECT COUNT(*) INTO @column_exists
FROM INFORMATION_SCHEMA.COLUMNS 
WHERE TABLE_SCHEMA = 'webserver' 
  AND TABLE_NAME = 'articles' 
  AND COLUMN_NAME = 'content_type';

-- 如果字段不存在则添加
SET @sql = IF(@column_exists = 0, 
    'ALTER TABLE articles ADD COLUMN content_type ENUM(''html'', ''markdown'') DEFAULT ''html'' AFTER content',
    'SELECT "content_type column already exists" as message');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- 6. 检查并创建文章标签关系表
CREATE TABLE IF NOT EXISTS article_tags (
    article_id INT,
    tag_id INT,
    PRIMARY KEY (article_id, tag_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 7. 检查并创建评论表
CREATE TABLE IF NOT EXISTS comments (
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
    INDEX idx_article_status (article_id, status),
    INDEX idx_parent (parent_id),
    INDEX idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 8. 检查并创建点赞记录表
CREATE TABLE IF NOT EXISTS likes (
    like_id INT AUTO_INCREMENT PRIMARY KEY,
    user_identifier VARCHAR(100) NOT NULL,
    target_type ENUM('article', 'comment') NOT NULL,
    target_id INT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY unique_like (user_identifier, target_type, target_id),
    INDEX idx_target (target_type, target_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 9. 检查并创建管理员配置表
CREATE TABLE IF NOT EXISTS admin_settings (
    setting_id INT AUTO_INCREMENT PRIMARY KEY,
    setting_key VARCHAR(100) NOT NULL UNIQUE,
    setting_value TEXT,
    description VARCHAR(255),
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 10. 添加外键约束（如果不存在）
-- 检查外键是否存在的函数较复杂，这里使用忽略错误的方式
-- 文章表外键
SET @sql = 'ALTER TABLE articles ADD CONSTRAINT fk_articles_author FOREIGN KEY (author_id) REFERENCES user(user_id) ON DELETE SET NULL';
SET @ignore_error = 1;
-- 由于MySQL不支持IF NOT EXISTS for constraints，我们使用程序来处理

-- 文章标签关系表外键
SET @sql = 'ALTER TABLE article_tags ADD CONSTRAINT fk_article_tags_article FOREIGN KEY (article_id) REFERENCES articles(article_id) ON DELETE CASCADE';
-- 同样的问题

-- 评论表外键
SET @sql = 'ALTER TABLE comments ADD CONSTRAINT fk_comments_article FOREIGN KEY (article_id) REFERENCES articles(article_id) ON DELETE CASCADE';

-- 11. 插入默认数据
-- 创建管理员账户
INSERT IGNORE INTO user (username, passwd, email, role) VALUES 
('admin', 'admin123', 'admin@example.com', 'admin');

-- 创建默认分类
INSERT IGNORE INTO categories (name, description) VALUES 
('技术', '技术相关文章'),
('生活', '生活随笔'),
('学习', '学习笔记'),
('随笔', '个人随笔');

-- 创建默认标签
INSERT IGNORE INTO tags (name) VALUES 
('C++'), ('Web开发'), ('数据库'), ('Linux'), ('算法'), ('Markdown'), ('前端'), ('后端');

-- 插入默认管理设置
INSERT IGNORE INTO admin_settings (setting_key, setting_value, description) VALUES 
('site_title', 'TinyWebServer博客', '网站标题'),
('site_description', '基于TinyWebServer的个人博客系统', '网站描述'),
('articles_per_page', '10', '每页文章数量'),
('allow_comments', '1', '是否允许评论'),
('comment_need_approval', '0', '评论是否需要审核'),
('default_content_type', 'markdown', '默认内容类型'),
('enable_markdown', '1', '是否启用Markdown支持'),
('max_upload_size', '5242880', '最大上传文件大小(字节)');

-- 12. 为现有文章设置默认content_type
UPDATE articles SET content_type = 'html' WHERE content_type IS NULL;

-- 13. 创建触发器（删除已存在的触发器后重新创建）
-- 删除可能存在的触发器
DROP TRIGGER IF EXISTS update_category_count_after_insert;
DROP TRIGGER IF EXISTS update_category_count_after_update;
DROP TRIGGER IF EXISTS update_category_count_after_delete;
DROP TRIGGER IF EXISTS update_article_comment_count_after_insert;
DROP TRIGGER IF EXISTS update_article_comment_count_after_delete;

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
    IF NEW.status = 'approved' THEN
        UPDATE articles SET comment_count = comment_count + 1 WHERE article_id = NEW.article_id;
    END IF;
END$$

CREATE TRIGGER update_article_comment_count_after_delete 
AFTER DELETE ON comments FOR EACH ROW
BEGIN
    IF OLD.status = 'approved' THEN
        UPDATE articles SET comment_count = comment_count - 1 WHERE article_id = OLD.article_id;
    END IF;
END$$

DELIMITER ;

-- 14. 创建上传目录的示例记录（如果需要）
-- 这部分在应用启动时由代码处理

-- 15. 更新现有数据的统计信息
-- 重新计算分类文章数量
UPDATE categories c SET article_count = (
    SELECT COUNT(*) FROM articles a 
    WHERE a.category_id = c.category_id AND a.status = 'published'
);

-- 重新计算文章评论数量
UPDATE articles a SET comment_count = (
    SELECT COUNT(*) FROM comments c 
    WHERE c.article_id = a.article_id AND c.status = 'approved'
);

-- 提交事务
COMMIT;

-- 显示迁移完成信息
SELECT 'Database migration completed successfully!' as message;
SELECT 'Tables created/updated:' as info;
SELECT TABLE_NAME, TABLE_ROWS, CREATE_TIME 
FROM INFORMATION_SCHEMA.TABLES 
WHERE TABLE_SCHEMA = 'webserver' 
AND TABLE_NAME IN ('user', 'articles', 'categories', 'tags', 'comments', 'likes', 'admin_settings', 'article_tags')
ORDER BY TABLE_NAME;

-- 显示文章表结构确认content_type字段
SELECT COLUMN_NAME, DATA_TYPE, IS_NULLABLE, COLUMN_DEFAULT 
FROM INFORMATION_SCHEMA.COLUMNS 
WHERE TABLE_SCHEMA = 'webserver' 
AND TABLE_NAME = 'articles' 
ORDER BY ORDINAL_POSITION;