# TinyWebServer CI/CD 流程指南

本文档指导你如何使用已配置好的 CI/CD 流水线来自动化构建、测试和部署 TinyWebServer 项目。

## 📋 前提条件

### 1. GitHub 仓库设置
确保你的项目已推送到 GitHub 仓库，且包含以下文件：
- `.github/workflows/` 目录下的所有 workflow 文件
- `Dockerfile` 和 `docker-compose.yml`
- `CMakeLists.txt` 构建配置

### 2. 必需的 GitHub Secrets
在 GitHub 仓库的 Settings → Secrets and variables → Actions 中配置：

```
DOCKER_USERNAME: 你的 Docker Hub 用户名
DOCKER_PASSWORD: 你的 Docker Hub 访问令牌
```

**获取 Docker Hub 访问令牌步骤：**
1. 登录 [Docker Hub](https://hub.docker.com)
2. 点击右上角头像 → Account Settings
3. 点击 Security → New Access Token
4. 创建令牌并复制到 GitHub Secrets

## 🚀 CI/CD 流程概览

### 流水线触发条件
- **推送代码** 到 `main` 或 `develop` 分支
- **创建 Pull Request** 到 `main` 分支
- **发布 Release** 版本
- **推送标签** (格式：`v*`)

### 流水线阶段

#### 1. 构建和测试 (build-and-test)
- 安装依赖 (CMake, MySQL, OpenSSL 等)
- 编译项目和测试
- 启动 MySQL 服务进行集成测试
- 运行单元测试
- 上传构建产物

#### 2. 代码质量检查 (code-quality)
- 静态代码分析 (cppcheck)
- 代码格式检查 (clang-format)

#### 3. Docker 镜像构建 (docker-build-and-push)
- 构建多平台 Docker 镜像 (amd64/arm64)
- 推送到 Docker Hub 和 GitHub Container Registry
- 生成软件物料清单 (SBOM)
- 安全漏洞扫描 (Trivy)

#### 4. 部署 (deploy) - 仅发布时触发
- 生产环境部署准备
- 创建部署状态记录

## 📝 操作步骤

### 第一次设置流程

1. **配置 Docker Hub 访问**
   ```bash
   # 在 GitHub 仓库中添加必需的 Secrets
   DOCKER_USERNAME: 你的Docker Hub用户名
   DOCKER_PASSWORD: 你的Docker Hub访问令牌
   ```

2. **推送代码到 GitHub**
   ```bash
   git add .
   git commit -m "初始化 CI/CD 配置"
   git push origin CICD
   ```

3. **创建 Pull Request**
   - 在 GitHub 上创建从 `CICD` 到 `main` 分支的 Pull Request
   - 这将触发构建和测试流程

### 日常开发流程

#### 功能开发
```bash
# 1. 创建功能分支
git checkout -b feature/new-feature

# 2. 开发和提交代码
git add .
git commit -m "实现新功能"
git push origin feature/new-feature

# 3. 创建 Pull Request
# 在 GitHub 上创建 PR，自动触发测试流程
```

#### 发布流程
```bash
# 1. 合并到 main 分支
git checkout main
git merge feature/new-feature

# 2. 创建版本标签
git tag -a v1.0.0 -m "发布版本 v1.0.0"
git push origin v1.0.0

# 3. 在 GitHub 上创建 Release
# 这将触发完整的构建、测试、打包和部署流程
```

## 🔍 流程监控

### 查看构建状态
1. 进入 GitHub 仓库页面
2. 点击 "Actions" 选项卡
3. 查看各个 workflow 的执行状态

### 构建失败处理
- **测试失败**: 检查代码逻辑和单元测试
- **编译错误**: 查看构建日志，修复编译问题
- **Docker 构建失败**: 检查 Dockerfile 配置
- **推送失败**: 验证 Docker Hub 凭据

### 成功标志
- ✅ 所有测试通过
- ✅ Docker 镜像成功构建并推送
- ✅ 代码质量检查通过
- ✅ 安全扫描无高危漏洞

## 🛠️ 本地测试

在推送代码前，建议进行本地测试：

```bash
# 编译项目
mkdir -p build && cd build
cmake ..
make server -j$(nproc)

# 运行 Docker 构建测试
docker build -t tinywebserver-test .

# 使用 docker-compose 测试
docker-compose up --build
```

## 🎯 高级配置

### 自定义构建环境
修改 `.github/workflows/ci-cd.yml` 中的构建步骤：
- 调整编译器版本
- 添加新的依赖库
- 修改测试配置

### 部署到生产环境
在 `deploy` 阶段添加实际的部署脚本：
```yaml
- name: Deploy to production
  run: |
    # 示例：使用 SSH 部署到服务器
    # ssh user@server 'cd /app && docker-compose pull && docker-compose up -d'
```

### 通知配置
添加构建状态通知（邮件、Slack 等）：
```yaml
- name: Notify on failure
  if: failure()
  uses: actions/github-script@v7
  with:
    script: |
      // 发送通知逻辑
```

## 📚 相关文档

- [GitHub Actions 官方文档](https://docs.github.com/en/actions)
- [Docker Hub 官方文档](https://docs.docker.com/docker-hub/)
- [CMake 构建系统](https://cmake.org/documentation/)

## 🚨 注意事项

1. **安全性**: 确保敏感信息仅通过 GitHub Secrets 传递
2. **资源消耗**: GitHub Actions 有使用限制，优化构建流程
3. **分支保护**: 建议设置 main 分支保护规则，要求 PR 通过检查后才能合并
4. **版本管理**: 使用语义化版本标签 (v1.0.0, v1.1.0 等)

---

现在你可以开始使用这个完整的 CI/CD 流程了！每次代码变更都会自动触发相应的构建和测试流程。