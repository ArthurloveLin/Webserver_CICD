-- =================================================================
-- 博客系统数据库初始化脚本
-- 功能：为博客系统创建所需的表结构和初始数据到 `webserver` 数据库
-- 作者：Gemini
-- 版本：1.1
-- 日期：2025-08-17
-- =================================================================

-- 1. 创建并使用目标数据库
-- -----------------------------------------------------------------
CREATE DATABASE IF NOT EXISTS `webserver` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE `webserver`;

-- 2. 创建表结构
-- -----------------------------------------------------------------

-- 用户表 (user)
DROP TABLE IF EXISTS `user`;
CREATE TABLE `user` (
  `user_id` INT AUTO_INCREMENT PRIMARY KEY,
  `username` VARCHAR(50) NOT NULL UNIQUE,
  `passwd` VARCHAR(255) NOT NULL, -- 密码字段长度增加，为加密做准备
  `email` VARCHAR(100) UNIQUE,
  `role` ENUM('admin', 'guest') DEFAULT 'guest',
  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  `last_login` TIMESTAMP NULL,
  INDEX `idx_username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 分类表 (categories)
DROP TABLE IF EXISTS `categories`;
CREATE TABLE `categories` (
  `category_id` INT AUTO_INCREMENT PRIMARY KEY,
  `name` VARCHAR(100) NOT NULL UNIQUE,
  `description` TEXT,
  `article_count` INT DEFAULT 0,
  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX `idx_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 标签表 (tags)
DROP TABLE IF EXISTS `tags`;
CREATE TABLE `tags` (
  `tag_id` INT AUTO_INCREMENT PRIMARY KEY,
  `name` VARCHAR(50) NOT NULL UNIQUE,
  `article_count` INT DEFAULT 0,
  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX `idx_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 文章表 (articles)
DROP TABLE IF EXISTS `articles`;
CREATE TABLE `articles` (
  `article_id` INT AUTO_INCREMENT PRIMARY KEY,
  `title` VARCHAR(255) NOT NULL,
  `content` LONGTEXT NOT NULL,
  `summary` TEXT,
  `author_id` INT,
  `category_id` INT,
  `status` ENUM('draft', 'published') DEFAULT 'draft',
  `view_count` INT DEFAULT 0,
  `like_count` INT DEFAULT 0,
  `comment_count` INT DEFAULT 0,
  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `published_at` TIMESTAMP NULL,
  FOREIGN KEY (`author_id`) REFERENCES `user`(`user_id`) ON DELETE SET NULL,
  FOREIGN KEY (`category_id`) REFERENCES `categories`(`category_id`) ON DELETE SET NULL,
  INDEX `idx_status_published` (`status`, `published_at`),
  INDEX `idx_category` (`category_id`),
  INDEX `idx_author` (`author_id`),
  INDEX `idx_created` (`created_at`),
  FULLTEXT `idx_title_content` (`title`, `content`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 文章与标签关系表 (article_tags)
DROP TABLE IF EXISTS `article_tags`;
CREATE TABLE `article_tags` (
  `article_id` INT,
  `tag_id` INT,
  PRIMARY KEY (`article_id`, `tag_id`),
  FOREIGN KEY (`article_id`) REFERENCES `articles`(`article_id`) ON DELETE CASCADE,
  FOREIGN KEY (`tag_id`) REFERENCES `tags`(`tag_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 评论表 (comments)
DROP TABLE IF EXISTS `comments`;
CREATE TABLE `comments` (
  `comment_id` INT AUTO_INCREMENT PRIMARY KEY,
  `article_id` INT NOT NULL,
  `user_name` VARCHAR(50) NOT NULL,
  `email` VARCHAR(100),
  `content` TEXT NOT NULL,
  `parent_id` INT DEFAULT NULL,
  `like_count` INT DEFAULT 0,
  `status` ENUM('pending', 'approved', 'rejected') DEFAULT 'approved',
  `ip_address` VARCHAR(45),
  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (`article_id`) REFERENCES `articles`(`article_id`) ON DELETE CASCADE,
  FOREIGN KEY (`parent_id`) REFERENCES `comments`(`comment_id`) ON DELETE CASCADE,
  INDEX `idx_article_status` (`article_id`, `status`),
  INDEX `idx_parent` (`parent_id`),
  INDEX `idx_created` (`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 点赞记录表 (likes)
DROP TABLE IF EXISTS `likes`;
CREATE TABLE `likes` (
  `like_id` INT AUTO_INCREMENT PRIMARY KEY,
  `user_identifier` VARCHAR(100) NOT NULL, -- 可以是用户ID或访客的唯一标识
  `target_type` ENUM('article', 'comment') NOT NULL,
  `target_id` INT NOT NULL,
  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY `unique_like` (`user_identifier`, `target_type`, `target_id`),
  INDEX `idx_target` (`target_type`, `target_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 管理员配置表 (admin_settings)
DROP TABLE IF EXISTS `admin_settings`;
CREATE TABLE `admin_settings` (
  `setting_id` INT AUTO_INCREMENT PRIMARY KEY,
  `setting_key` VARCHAR(100) NOT NULL UNIQUE,
  `setting_value` TEXT,
  `description` VARCHAR(255),
  `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- 3. 插入初始化数据
-- -----------------------------------------------------------------

-- 创建默认管理员用户 (密码建议使用哈希存储，这里仅为示例)
INSERT INTO `user` (`username`, `passwd`, `email`, `role`) VALUES
('admin', 'admin123', 'admin@example.com', 'admin');

-- 创建默认分类
INSERT INTO `categories` (`name`, `description`) VALUES
('技术', '技术相关文章'),
('生活', '生活随笔'),
('学习', '学习笔记'),
('C++', 'C++编程相关'),
('Web开发', 'Web开发技术');

-- 创建默认标签
INSERT INTO `tags` (`name`) VALUES
('C++'), ('Web开发'), ('数据库'), ('Linux'), ('算法'), ('TinyWebServer'), ('博客系统');

-- 插入测试文章
INSERT INTO `articles` (`title`, `content`, `summary`, `author_id`, `category_id`, `status`, `created_at`, `published_at`) VALUES
('TinyWebServer博客系统开发日志',
'# TinyWebServer博客系统开发日志...', -- 内容省略以保持脚本简洁
'记录TinyWebServer博客系统的开发过程和技术要点',
1, 1, 'published', NOW(), NOW()),

('C++Web服务器开发入门',
'# C++Web服务器开发入门...', -- 内容省略
'介绍C++Web服务器开发的基础知识和TinyWebServer的特色',
1, 4, 'published', DATE_SUB(NOW(), INTERVAL 1 DAY), DATE_SUB(NOW(), INTERVAL 1 DAY)),

('博客系统数据库设计',
'# 博客系统数据库设计...', -- 内容省略
'详细介绍博客系统的数据库表结构设计和优化策略',
1, 3, 'draft', DATE_SUB(NOW(), INTERVAL 2 DAY), NULL);

-- 插入文章与标签的关联关系
INSERT INTO `article_tags` (`article_id`, `tag_id`) VALUES
(1, 6), (1, 7), (1, 2),
(2, 1), (2, 2), (2, 6),
(3, 3), (3, 7);

-- 插入默认管理设置
INSERT INTO `admin_settings` (`setting_key`, `setting_value`, `description`) VALUES
('site_title', 'TinyWebServer博客', '网站标题'),
('site_description', '基于TinyWebServer的个人博客系统', '网站描述'),
('articles_per_page', '10', '每页文章数量'),
('allow_comments', '1', '是否允许评论'),
('comment_need_approval', '0', '评论是否需要审核');


-- 4. 更新初始计数
-- -----------------------------------------------------------------
UPDATE `categories` c SET `article_count` = (
    SELECT COUNT(*) FROM `articles` a
    WHERE a.category_id = c.category_id AND a.status = 'published'
);

UPDATE `tags` t SET `article_count` = (
    SELECT COUNT(*) FROM `article_tags` at
    JOIN `articles` a ON at.article_id = a.article_id
    WHERE at.tag_id = t.tag_id AND a.status = 'published'
);


-- 5. 创建触发器 (用于自动维护计数)
-- -----------------------------------------------------------------
DELIMITER $$

-- 文章插入后更新分类计数
CREATE TRIGGER `trg_update_category_count_after_insert`
AFTER INSERT ON `articles` FOR EACH ROW
BEGIN
    IF NEW.status = 'published' AND NEW.category_id IS NOT NULL THEN
        UPDATE `categories` SET `article_count` = `article_count` + 1 WHERE `category_id` = NEW.category_id;
    END IF;
END$$

-- 文章更新后更新分类计数
CREATE TRIGGER `trg_update_category_count_after_update`
AFTER UPDATE ON `articles` FOR EACH ROW
BEGIN
    -- 处理分类变更
    IF OLD.category_id != NEW.category_id OR (OLD.category_id IS NULL AND NEW.category_id IS NOT NULL) OR (OLD.category_id IS NOT NULL AND NEW.category_id IS NULL) THEN
        IF OLD.category_id IS NOT NULL AND OLD.status = 'published' THEN
            UPDATE `categories` SET `article_count` = `article_count` - 1 WHERE `category_id` = OLD.category_id;
        END IF;
        IF NEW.category_id IS NOT NULL AND NEW.status = 'published' THEN
            UPDATE `categories` SET `article_count` = `article_count` + 1 WHERE `category_id` = NEW.category_id;
        END IF;
    -- 处理状态变更
    ELSEIF OLD.status != NEW.status AND NEW.category_id IS NOT NULL THEN
        IF OLD.status != 'published' AND NEW.status = 'published' THEN
            UPDATE `categories` SET `article_count` = `article_count` + 1 WHERE `category_id` = NEW.category_id;
        ELSEIF OLD.status = 'published' AND NEW.status != 'published' THEN
            UPDATE `categories` SET `article_count` = `article_count` - 1 WHERE `category_id` = NEW.category_id;
        END IF;
    END IF;
END$$

-- 文章删除后更新分类计数
CREATE TRIGGER `trg_update_category_count_after_delete`
AFTER DELETE ON `articles` FOR EACH ROW
BEGIN
    IF OLD.status = 'published' AND OLD.category_id IS NOT NULL THEN
        UPDATE `categories` SET `article_count` = `article_count` - 1 WHERE `category_id` = OLD.category_id;
    END IF;
END$$

-- 评论插入后更新文章评论数
CREATE TRIGGER `trg_update_article_comment_count_after_insert`
AFTER INSERT ON `comments` FOR EACH ROW
BEGIN
    UPDATE `articles` SET `comment_count` = `comment_count` + 1 WHERE `article_id` = NEW.article_id;
END$$

-- 评论删除后更新文章评论数
CREATE TRIGGER `trg_update_article_comment_count_after_delete`
AFTER DELETE ON `comments` FOR EACH ROW
BEGIN
    UPDATE `articles` SET `comment_count` = `comment_count` - 1 WHERE `article_id` = OLD.article_id;
END$$

DELIMITER ;


-- 6. 显示结果，确认安装成功
-- -----------------------------------------------------------------
SELECT '=== 数据库 webserver 初始化完成 ===' AS `status`;

SELECT '=== 用户表示例数据 ===' AS `info`;
SELECT `user_id`, `username`, `role` FROM `user`;

SELECT '=== 分类表示例数据 ===' AS `info`;
SELECT * FROM `categories`;

SELECT '=== 标签表示例数据 ===' AS `info`;
SELECT * FROM `tags`;

SELECT '=== 文章表示例数据 ===' AS `info`;
SELECT `article_id`, `title`, `status`, `category_id`, `created_at` FROM `articles`;
