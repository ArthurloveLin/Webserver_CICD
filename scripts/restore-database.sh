#!/bin/bash

# =================================================================
# MySQL数据库恢复脚本
# 功能：从备份文件恢复数据库
# =================================================================

set -e

# 配置变量
CONTAINER_NAME="${CONTAINER_NAME:-tinywebserver-mysql}"
DB_USER="${DB_USER:-root}"
DB_PASSWORD="${DB_PASSWORD:-webserver123}"

# 显示用法
show_usage() {
    echo "用法: $0 <backup_file>"
    echo ""
    echo "示例:"
    echo "  $0 ./backups/webserver_backup_20231128_143022.sql.gz"
    echo "  $0 ./backups/webserver_backup_20231128_143022.sql"
    echo ""
    echo "环境变量:"
    echo "  CONTAINER_NAME: MySQL容器名称 (默认: tinywebserver-mysql)"
    echo "  DB_USER: 数据库用户 (默认: root)"
    echo "  DB_PASSWORD: 数据库密码 (默认: webserver123)"
    exit 1
}

# 检查参数
if [ $# -ne 1 ]; then
    show_usage
fi

BACKUP_FILE="$1"

# 检查备份文件是否存在
if [ ! -f "$BACKUP_FILE" ]; then
    echo "❌ 错误: 备份文件 '$BACKUP_FILE' 不存在"
    exit 1
fi

echo "🔄 开始恢复数据库..."
echo "📅 时间: $(date)"
echo "🗄️  容器: $CONTAINER_NAME"
echo "📁 备份文件: $BACKUP_FILE"

# 检查容器是否运行
if ! docker ps | grep -q "$CONTAINER_NAME"; then
    echo "❌ 错误: MySQL容器 '$CONTAINER_NAME' 未运行"
    exit 1
fi

# 确认操作
echo ""
echo "⚠️  警告: 此操作将恢复数据库，可能会覆盖现有数据!"
echo "请确认您已经备份了当前数据。"
echo ""
read -p "是否继续? (输入 'yes' 确认): " -r
if [[ ! $REPLY == "yes" ]]; then
    echo "❌ 操作已取消"
    exit 1
fi

# 创建当前数据的备份
echo "🔄 创建当前数据的安全备份..."
SAFETY_BACKUP_DIR="./backups/safety"
mkdir -p "$SAFETY_BACKUP_DIR"
SAFETY_BACKUP_FILE="$SAFETY_BACKUP_DIR/before_restore_$(date +"%Y%m%d_%H%M%S").sql"

docker exec "$CONTAINER_NAME" mysqldump \
    -u "$DB_USER" \
    -p"$DB_PASSWORD" \
    --routines \
    --triggers \
    --single-transaction \
    --lock-tables=false \
    --add-drop-database \
    --databases webserver > "$SAFETY_BACKUP_FILE"

gzip "$SAFETY_BACKUP_FILE"
echo "✅ 安全备份已创建: ${SAFETY_BACKUP_FILE}.gz"

# 准备恢复
echo "🔄 正在恢复数据库..."

# 检查文件是否是压缩的
if [[ "$BACKUP_FILE" == *.gz ]]; then
    echo "🗜️  解压缩备份文件..."
    zcat "$BACKUP_FILE" | docker exec -i "$CONTAINER_NAME" mysql -u "$DB_USER" -p"$DB_PASSWORD"
else
    echo "📄 导入SQL文件..."
    cat "$BACKUP_FILE" | docker exec -i "$CONTAINER_NAME" mysql -u "$DB_USER" -p"$DB_PASSWORD"
fi

echo "✅ 数据库恢复完成!"

# 验证恢复
echo "🔍 验证恢复结果..."
TABLES_COUNT=$(docker exec "$CONTAINER_NAME" mysql -u "$DB_USER" -p"$DB_PASSWORD" -e "USE webserver; SHOW TABLES;" | wc -l)
USER_COUNT=$(docker exec "$CONTAINER_NAME" mysql -u "$DB_USER" -p"$DB_PASSWORD" -e "USE webserver; SELECT COUNT(*) FROM user;" | tail -n1)

echo "📊 数据库表数量: $((TABLES_COUNT - 1))"
echo "👥 用户数量: $USER_COUNT"

echo ""
echo "✅ 数据库恢复成功完成!"
echo "🛡️  安全备份保存在: ${SAFETY_BACKUP_FILE}.gz"