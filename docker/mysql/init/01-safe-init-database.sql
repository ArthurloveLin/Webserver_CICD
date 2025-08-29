-- =================================================================
-- TinyWebServer数据库安全初始化脚本 (Docker版本)
-- 特点：只创建不存在的表，不会删除现有数据
-- =================================================================

-- 使用默认创建的webserver数据库
USE webserver;

-- 用户表 (user) - 仅在不存在时创建
CREATE TABLE IF NOT EXISTS `user` (
  `user_id` INT AUTO_INCREMENT PRIMARY KEY,
  `username` VARCHAR(50) NOT NULL UNIQUE,
  `passwd` VARCHAR(255) NOT NULL,
  `email` VARCHAR(100) UNIQUE,
  `role` ENUM('admin', 'guest') DEFAULT 'guest',
  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  `last_login` TIMESTAMP NULL,
  INDEX `idx_username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 分类表 (categories) - 仅在不存在时创建
CREATE TABLE IF NOT EXISTS `categories` (
  `category_id` INT AUTO_INCREMENT PRIMARY KEY,
  `name` VARCHAR(100) NOT NULL UNIQUE,
  `description` TEXT,
  `article_count` INT DEFAULT 0,
  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX `idx_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 标签表 (tags) - 仅在不存在时创建
CREATE TABLE IF NOT EXISTS `tags` (
  `tag_id` INT AUTO_INCREMENT PRIMARY KEY,
  `name` VARCHAR(50) NOT NULL UNIQUE,
  `article_count` INT DEFAULT 0,
  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX `idx_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 文章表 (articles) - 仅在不存在时创建
CREATE TABLE IF NOT EXISTS `articles` (
  `article_id` INT AUTO_INCREMENT PRIMARY KEY,
  `title` VARCHAR(200) NOT NULL,
  `content` LONGTEXT NOT NULL,
  `summary` TEXT,
  `author_id` INT NOT NULL,
  `category_id` INT,
  `status` ENUM('draft', 'published', 'archived') DEFAULT 'draft',
  `view_count` INT DEFAULT 0,
  `like_count` INT DEFAULT 0,
  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `published_at` TIMESTAMP NULL,
  INDEX `idx_author` (`author_id`),
  INDEX `idx_category` (`category_id`),
  INDEX `idx_status` (`status`),
  INDEX `idx_published_at` (`published_at`),
  FOREIGN KEY (`author_id`) REFERENCES `user`(`user_id`) ON DELETE CASCADE,
  FOREIGN KEY (`category_id`) REFERENCES `categories`(`category_id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 文章标签关联表 (article_tags) - 仅在不存在时创建
CREATE TABLE IF NOT EXISTS `article_tags` (
  `article_id` INT,
  `tag_id` INT,
  PRIMARY KEY (`article_id`, `tag_id`),
  FOREIGN KEY (`article_id`) REFERENCES `articles`(`article_id`) ON DELETE CASCADE,
  FOREIGN KEY (`tag_id`) REFERENCES `tags`(`tag_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 安全的初始数据插入：使用INSERT IGNORE避免重复插入
-- 创建管理员用户（仅在不存在时插入）
INSERT IGNORE INTO `user` (username, passwd, role, email) VALUES 
('admin', 'TcH1pYCcNVq9t8Cm$8fa35266b14a1b49c4ad5bb7e6e867c6a75f5b4b1c7e2a88e9d45e06f8b12345', 'admin', 'admin@example.com'),
('guest', 'guest', 'guest', 'guest@example.com');

-- 创建默认分类（仅在不存在时插入）
INSERT IGNORE INTO `categories` (name, description) VALUES 
('技术分享', '技术相关的文章和教程'),
('生活随笔', '日常生活感悟和随笔'),
('项目展示', '个人项目和作品展示');

-- 创建默认标签（仅在不存在时插入）
INSERT IGNORE INTO `tags` (name) VALUES 
('C++'), 
('Web开发'), 
('数据库'), 
('Linux'), 
('Docker'),
('教程');

-- 安全插入示例文章（检查是否已存在）
INSERT INTO `articles` (title, content, summary, author_id, category_id, status, published_at) 
SELECT * FROM (SELECT 
    '欢迎使用TinyWebServer博客系统' as title,
    '# 欢迎使用TinyWebServer博客系统\n\n这是一个基于C++开发的轻量级Web服务器，支持博客功能。\n\n## 特性\n\n- 高性能C++后端\n- MySQL数据库支持\n- Markdown编辑器\n- 用户权限管理\n- Docker容器化部署\n\n开始您的博客之旅吧！' as content,
    '欢迎使用TinyWebServer博客系统，这是一个高性能的C++博客平台。' as summary,
    1 as author_id, 
    1 as category_id, 
    'published' as status, 
    NOW() as published_at
) AS tmp
WHERE NOT EXISTS (
    SELECT 1 FROM `articles` WHERE title = '欢迎使用TinyWebServer博客系统'
) LIMIT 1;

-- 安全插入文章标签关联（避免重复）
INSERT IGNORE INTO `article_tags` (article_id, tag_id) 
SELECT a.article_id, t.tag_id 
FROM `articles` a, `tags` t 
WHERE a.title = '欢迎使用TinyWebServer博客系统' 
AND t.name IN ('C++', 'Web开发', 'Docker');

-- 创建用于测试的视图（如果不存在）
CREATE OR REPLACE VIEW `published_articles_view` AS 
SELECT 
    a.article_id,
    a.title,
    a.summary,
    a.view_count,
    a.like_count,
    a.published_at,
    u.username as author_name,
    c.name as category_name
FROM `articles` a
LEFT JOIN `user` u ON a.author_id = u.user_id
LEFT JOIN `categories` c ON a.category_id = c.category_id
WHERE a.status = 'published'
ORDER BY a.published_at DESC;

-- 输出初始化完成信息
SELECT 'Database safely initialized! Existing data preserved.' as status;
SELECT COUNT(*) as user_count FROM user;
SELECT COUNT(*) as article_count FROM articles;