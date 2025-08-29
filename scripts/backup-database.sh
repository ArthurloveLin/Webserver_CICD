#!/bin/bash

# =================================================================
# MySQL数据库备份脚本
# 功能：创建数据库备份，支持自动化备份策略
# =================================================================

set -e

# 配置变量
BACKUP_DIR="${BACKUP_DIR:-./backups}"
DATE=$(date +"%Y%m%d_%H%M%S")
CONTAINER_NAME="${CONTAINER_NAME:-tinywebserver-mysql}"
DB_NAME="${DB_NAME:-webserver}"
DB_USER="${DB_USER:-root}"
DB_PASSWORD="${DB_PASSWORD:-webserver123}"

# 创建备份目录
mkdir -p "$BACKUP_DIR"

echo "🔄 开始备份数据库..."
echo "📅 时间: $(date)"
echo "🗄️  容器: $CONTAINER_NAME"
echo "💾 数据库: $DB_NAME"

# 检查容器是否运行
if ! docker ps | grep -q "$CONTAINER_NAME"; then
    echo "❌ 错误: MySQL容器 '$CONTAINER_NAME' 未运行"
    exit 1
fi

# 执行备份
BACKUP_FILE="$BACKUP_DIR/webserver_backup_$DATE.sql"

echo "💾 创建备份文件: $BACKUP_FILE"

docker exec "$CONTAINER_NAME" mysqldump \
    -u "$DB_USER" \
    -p"$DB_PASSWORD" \
    --routines \
    --triggers \
    --single-transaction \
    --lock-tables=false \
    --add-drop-database \
    --databases "$DB_NAME" > "$BACKUP_FILE"

# 压缩备份文件
gzip "$BACKUP_FILE"
BACKUP_FILE_GZ="${BACKUP_FILE}.gz"

echo "🗜️  备份已压缩: $BACKUP_FILE_GZ"

# 获取文件大小
FILE_SIZE=$(du -h "$BACKUP_FILE_GZ" | cut -f1)
echo "📊 备份文件大小: $FILE_SIZE"

# 清理旧备份（保留最近7天的备份）
echo "🧹 清理旧备份文件..."
find "$BACKUP_DIR" -name "webserver_backup_*.sql.gz" -mtime +7 -delete

echo "✅ 备份完成!"
echo "📁 备份文件: $BACKUP_FILE_GZ"

# 验证备份
echo "🔍 验证备份完整性..."
if gzip -t "$BACKUP_FILE_GZ"; then
    echo "✅ 备份文件完整性验证通过"
else
    echo "❌ 警告: 备份文件可能损坏!"
    exit 1
fi