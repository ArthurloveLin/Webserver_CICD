# 密码加密升级工具

## 📁 文件说明

- `generate_admin_password.cpp` - 密码生成工具源码
- `generate_admin_password` - 编译后的可执行文件
- `rebuild_database.sql` - 数据库清理脚本

## 🚀 升级步骤

### 1. 重建数据库
```bash
mysql -u root -p密码 webserver < rebuild_database.sql
mysql -u root -p密码 webserver < ../vblog.sql
```

### 2. 生成admin加密密码
```bash
./generate_admin_password
```
复制输出的SQL语句并执行。

### 3. 完成升级
重启服务器，使用admin/admin123登录测试。

## ✅ 升级效果

- ✅ 密码从明文升级为SHA-256+Salt加密
- ✅ 防止数据库密码泄露风险  
- ✅ 向后兼容，性能无影响