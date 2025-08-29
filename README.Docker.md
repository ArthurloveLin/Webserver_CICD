# TinyWebServer Docker部署指南

## 🚀 快速开始

### 开发环境部署
```bash
# 基本部署（Web服务器 + MySQL）
docker-compose up -d

# 完整部署（包含Nginx反向代理）
docker-compose --profile with-nginx up -d
```

### 生产环境部署
```bash
# 生产环境部署（推荐）
docker-compose -f docker-compose.yml -f docker-compose.prod.yml up -d

# 使用自定义密码
MYSQL_ROOT_PASSWORD=your_secure_password docker-compose -f docker-compose.yml -f docker-compose.prod.yml up -d
```

## 🛡️ 数据安全保证

### MySQL初始化安全机制
1. **安全初始化**: 使用`CREATE TABLE IF NOT EXISTS`，不会删除现有数据
2. **幂等操作**: 使用`INSERT IGNORE`，避免重复插入
3. **数据持久化**: 数据存储在Docker卷中，容器重启不会丢失
4. **自动备份**: 提供备份和恢复脚本

### 数据备份与恢复
```bash
# 创建备份
./scripts/backup-database.sh

# 恢复数据（会先创建安全备份）
./scripts/restore-database.sh ./backups/webserver_backup_20231128_143022.sql.gz

# 查看备份文件
ls -la backups/
```

## 📁 项目结构
```
.
├── Dockerfile                    # 多阶段构建配置
├── docker-compose.yml           # 主要服务定义
├── docker-compose.override.yml  # 开发环境覆盖
├── .dockerignore                # Docker构建忽略文件
└── docker/
    ├── mysql/
    │   ├── init/                # 数据库初始化脚本
    │   ├── conf/                # MySQL配置文件
    │   └── data/                # 数据持久化目录
    └── nginx/
        ├── nginx.conf           # Nginx主配置
        └── conf.d/              # 虚拟主机配置
```

## 🛠 环境变量配置

### WebServer配置
- `DB_HOST`: 数据库主机 (默认: mysql)
- `DB_USER`: 数据库用户 (默认: root)
- `DB_PASSWORD`: 数据库密码 (默认: webserver123)
- `DB_NAME`: 数据库名称 (默认: webserver)
- `SERVER_PORT`: 服务端口 (默认: 9006)

### MySQL配置
- `MYSQL_ROOT_PASSWORD`: MySQL root密码
- `MYSQL_DATABASE`: 默认数据库名
- `MYSQL_CHARACTER_SET_SERVER`: 字符集配置

## 🔗 端口映射

| 服务 | 内部端口 | 外部端口 | 说明 |
|------|---------|---------|------|
| WebServer | 9006 | 9006 | 应用主端口 |
| MySQL | 3306 | 3306 | 数据库端口 |
| Nginx | 80 | 80 | HTTP代理 |
| Nginx | 443 | 443 | HTTPS代理 |

## 📊 健康检查

所有服务都配置了健康检查：

```bash
# 检查服务状态
docker-compose ps

# 查看健康检查日志
docker-compose logs webserver
docker-compose logs mysql
```

## 🗂 数据持久化

- **MySQL数据**: `./docker/mysql/data`
- **应用日志**: `./logs`
- **上传文件**: `./root/uploads`

## 🔧 开发环境

开发模式下，使用覆盖配置文件：

```bash
# 开发模式启动（包含调试配置）
docker-compose up

# 生产模式启动
docker-compose -f docker-compose.yml up -d
```

## 🧪 测试部署

### 1. 构建和启动服务
```bash
docker-compose up --build
```

### 2. 验证服务运行
```bash
# 检查应用响应
curl http://localhost:9006

# 检查nginx代理（如启用）
curl http://localhost

# 检查健康状态
curl http://localhost/health
```

### 3. 数据库连接测试
```bash
# 连接MySQL
docker exec -it tinywebserver-mysql mysql -u root -pwebserver123 webserver

# 查看初始化数据
mysql> SELECT * FROM user;
mysql> SELECT * FROM articles;
```

## 🚀 生产环境部署建议

### 1. 环境变量配置
创建 `.env` 文件：
```env
MYSQL_ROOT_PASSWORD=your_secure_password
DB_PASSWORD=your_secure_password
SERVER_PORT=9006
```

### 2. 安全加固
```bash
# 使用非root用户
# 配置防火墙规则
# 启用HTTPS（需要SSL证书）
```

### 3. 性能优化
- 调整MySQL配置 (`docker/mysql/conf/my.cnf`)
- 配置Nginx缓存策略
- 设置日志轮转

## 🛡 故障排除

### 常见问题

1. **容器启动失败**
```bash
docker-compose logs webserver
docker-compose logs mysql
```

2. **数据库连接失败**
```bash
# 检查MySQL是否就绪
docker exec tinywebserver-mysql mysqladmin ping -h localhost -u root -p
```

3. **端口冲突**
```bash
# 修改docker-compose.yml中的端口映射
ports:
  - "9007:9006"  # 使用其他端口
```

### 清理和重建

```bash
# 停止并删除容器
docker-compose down

# 删除数据卷（慎用！）
docker-compose down -v

# 重新构建镜像
docker-compose build --no-cache

# 完全重新部署
docker-compose up --build --force-recreate
```

## 📝 日志管理

```bash
# 查看所有服务日志
docker-compose logs

# 查看特定服务日志
docker-compose logs -f webserver

# 查看最近100行日志
docker-compose logs --tail 100 mysql
```

## 🎯 CI/CD集成

该Docker配置已为CI/CD做好准备：

- 多阶段构建优化镜像大小
- 内置测试执行
- 健康检查确保服务质量
- 环境变量配置灵活性

可直接集成到GitHub Actions、GitLab CI或Jenkins等CI/CD系统。