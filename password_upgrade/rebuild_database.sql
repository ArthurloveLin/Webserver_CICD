-- 安全重建数据库脚本，处理外键约束
USE webserver;

-- 1. 临时禁用外键检查
SET FOREIGN_KEY_CHECKS = 0;

-- 2. 删除所有相关表（按依赖顺序）
DROP TABLE IF EXISTS `likes`;
DROP TABLE IF EXISTS `comments`;
DROP TABLE IF EXISTS `article_tags`;
DROP TABLE IF EXISTS `articles`;
DROP TABLE IF EXISTS `tags`;
DROP TABLE IF EXISTS `categories`;
DROP TABLE IF EXISTS `admin_settings`;
DROP TABLE IF EXISTS `user`;

-- 3. 重新启用外键检查
SET FOREIGN_KEY_CHECKS = 1;

-- 现在可以安全运行 vblog.sql 了
SELECT '数据库已清理，现在可以运行 vblog.sql' AS message;