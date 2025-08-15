# TinyWebServer博客系统开发需求指南

## 项目概述

基于[TinyWebServer](https://github.com/qinguoyi/TinyWebServer)框架开发一个功能完整的博客系统。TinyWebServer是一个Linux下的C++轻量级Web服务器，具有完善的并发处理、HTTP解析、数据库连接池等基础设施。

## 技术背景

### TinyWebServer现有架构
- **并发模型**: 线程池 + 非阻塞socket + epoll(ET/LT) + 事件处理(Reactor/Proactor)
- **HTTP解析**: 状态机解析GET/POST请求
- **数据库**: MySQL连接池，支持用户注册登录
- **日志系统**: 同步/异步日志记录
- **定时器**: 处理非活动连接
- **现有模块结构**:
  ```
  ├── lock/           # 线程同步机制
  ├── threadpool/     # 半同步半反应堆线程池
  ├── http/           # HTTP连接处理
  ├── timer/          # 定时器处理
  ├── log/            # 日志系统
  ├── CGImysql/       # 数据库连接池
  ├── webserver.h     # 主服务器类
  └── main.cpp        # 入口程序
  ```

## 需求分析

### 功能需求

#### 1. 用户权限系统
- **管理员账户**: 拥有完全权限（发帖、编辑、删除、管理用户）
- **游客账户**: 只读权限（浏览文章、评论、点赞）
- **权限控制**: 基于session/token的身份验证

#### 2. 博客核心功能
- **文章管理**: 创建、编辑、删除、发布/草稿状态
- **分类标签**: 文章分类和标签系统
- **评论系统**: 游客可评论，支持回复
- **点赞功能**: 文章和评论的点赞统计
- **搜索功能**: 按标题、内容、标签搜索

#### 3. 内容展示
- **首页**: 文章列表（分页）
- **文章详情页**: 完整文章 + 评论区
- **分类页面**: 按分类浏览文章
- **归档页面**: 按时间归档
- **管理后台**: 文章管理界面

### 技术需求

#### 1. 模块设计决策

**新增模块建议**:
```
├── blog/           # 新增博客核心模块
│   ├── article/    # 文章处理
│   ├── comment/    # 评论处理
│   ├── user/       # 用户权限扩展
│   └── template/   # 模板渲染
├── static/         # 静态资源
│   ├── css/
│   ├── js/
│   └── images/
└── templates/      # HTML模板
    ├── admin/      # 管理界面
    └── public/     # 公共页面
```

**模块集成方式**:
- 扩展现有`http/http_conn.cpp`的路由处理
- 利用现有数据库连接池
- 集成到现有请求处理流程中

#### 2. 数据库设计

**核心表结构**:

```sql
-- 用户表（扩展现有user表）
ALTER TABLE user ADD COLUMN (
    user_id INT AUTO_INCREMENT PRIMARY KEY,
    email VARCHAR(100),
    role ENUM('admin', 'guest') DEFAULT 'guest',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP
);

-- 文章表
CREATE TABLE articles (
    article_id INT AUTO_INCREMENT PRIMARY KEY,
    title VARCHAR(255) NOT NULL,
    content TEXT NOT NULL,
    summary TEXT,
    author_id INT,
    category_id INT,
    status ENUM('draft', 'published') DEFAULT 'draft',
    view_count INT DEFAULT 0,
    like_count INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (author_id) REFERENCES user(user_id)
);

-- 分类表
CREATE TABLE categories (
    category_id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 标签表
CREATE TABLE tags (
    tag_id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(50) NOT NULL UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 文章标签关系表
CREATE TABLE article_tags (
    article_id INT,
    tag_id INT,
    PRIMARY KEY (article_id, tag_id),
    FOREIGN KEY (article_id) REFERENCES articles(article_id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(tag_id) ON DELETE CASCADE
);

-- 评论表
CREATE TABLE comments (
    comment_id INT AUTO_INCREMENT PRIMARY KEY,
    article_id INT,
    user_name VARCHAR(50),
    email VARCHAR(100),
    content TEXT NOT NULL,
    parent_id INT DEFAULT NULL,
    like_count INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (article_id) REFERENCES articles(article_id) ON DELETE CASCADE,
    FOREIGN KEY (parent_id) REFERENCES comments(comment_id) ON DELETE SET NULL
);

-- 点赞记录表
CREATE TABLE likes (
    like_id INT AUTO_INCREMENT PRIMARY KEY,
    user_identifier VARCHAR(100), -- IP或用户ID
    target_type ENUM('article', 'comment'),
    target_id INT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY unique_like (user_identifier, target_type, target_id)
);
```

#### 3. HTTP路由扩展

**在http_conn.cpp中添加路由处理**:

```cpp
// 需要处理的URL模式
/blog/                    # 首页文章列表
/blog/article/{id}        # 文章详情
/blog/category/{id}       # 分类页面
/blog/admin/              # 管理后台
/blog/admin/new           # 新建文章
/blog/admin/edit/{id}     # 编辑文章
/blog/api/comment         # 评论API
/blog/api/like           # 点赞API
/blog/search             # 搜索功能
```

**请求处理流程**:
1. 扩展`parse_request_line()`识别博客路由
2. 在`do_request()`中调用相应的博客处理函数
3. 实现JSON API响应（用于AJAX请求）
4. 集成模板渲染系统

#### 4. 模板系统

**模板渲染方案**:
- 简单字符串替换模板引擎
- 支持变量替换：`{{variable}}`
- 支持循环：`{{#each items}}...{{/each}}`
- 支持条件：`{{#if condition}}...{{/if}}`

**模板文件组织**:
```
templates/
├── layout.html          # 基础布局
├── index.html           # 首页
├── article.html         # 文章详情
├── category.html        # 分类页面
├── admin/
│   ├── dashboard.html   # 管理首页
│   ├── editor.html      # 文章编辑器
│   └── list.html        # 文章列表管理
└── partials/
    ├── header.html      # 页头
    ├── footer.html      # 页尾
    └── comment.html     # 评论组件
```

#### 5. 静态资源处理

**资源组织**:
```
static/
├── css/
│   ├── main.css         # 主样式
│   ├── admin.css        # 管理界面样式
│   └── editor.css       # 编辑器样式
├── js/
│   ├── main.js          # 主要交互
│   ├── comment.js       # 评论功能
│   ├── like.js          # 点赞功能
│   └── editor.js        # 富文本编辑器
└── images/
    ├── logo.png
    └── default-avatar.png
```

**静态文件服务**:
- 修改TinyWebServer的静态文件处理逻辑
- 支持CSS、JS、图片等MIME类型
- 实现缓存头设置

#### 6. 会话管理

**Session实现**:
- 基于Cookie的简单session
- 在内存中维护session池
- 管理员登录状态管理
- 游客身份标识（IP based）

**权限验证**:
```cpp
class AuthManager {
public:
    bool isAdmin(const std::string& sessionId);
    bool isValidSession(const std::string& sessionId);
    std::string createSession(const std::string& username, UserRole role);
    void destroySession(const std::string& sessionId);
};
```

#### 7. API接口设计

**RESTful API**:
```
GET    /blog/api/articles          # 获取文章列表
GET    /blog/api/articles/{id}     # 获取文章详情
POST   /blog/api/articles          # 创建文章（管理员）
PUT    /blog/api/articles/{id}     # 更新文章（管理员）
DELETE /blog/api/articles/{id}     # 删除文章（管理员）

POST   /blog/api/comments          # 添加评论
GET    /blog/api/comments/{id}     # 获取文章评论
POST   /blog/api/likes             # 点赞/取消点赞

GET    /blog/api/categories        # 获取分类列表
GET    /blog/api/tags              # 获取标签列表
GET    /blog/api/search            # 搜索接口
```

## 实现优先级

### Phase 1: 基础框架
1. 数据库表结构创建
2. 基础路由系统实现
3. 简单模板引擎开发
4. 静态资源服务扩展

### Phase 2: 核心功能
1. 文章CRUD操作
2. 用户权限系统
3. 基础前端界面
4. 管理后台

### Phase 3: 高级功能
1. 评论系统
2. 点赞功能
3. 搜索功能
4. 分页和优化

### Phase 4: 优化增强
1. 缓存机制
2. 性能优化
3. 安全增强
4. 用户体验改进

## 技术挑战与解决方案

### 1. 并发安全
- **问题**: 多线程下的数据一致性
- **解决**: 利用现有的数据库连接池，使用事务保证一致性

### 2. 内存管理
- **问题**: 模板和会话数据的内存占用
- **解决**: 实现简单的LRU缓存，定期清理过期session

### 3. 性能优化
- **问题**: 数据库查询效率
- **解决**: 添加适当索引，实现查询结果缓存

### 4. 安全考虑
- **问题**: SQL注入、XSS攻击
- **解决**: 参数化查询，输出转义，CSRF防护

## 开发指导原则

1. **保持兼容**: 不破坏TinyWebServer现有功能
2. **模块化设计**: 新功能独立模块，便于维护
3. **性能优先**: 利用现有的高性能基础设施
4. **渐进开发**: 分阶段实现，确保每个阶段都可运行
5. **代码质量**: 遵循现有代码风格，添加适当注释

## 预期成果

完成后的博客系统将具备：
- 完整的文章发布和管理功能
- 用户友好的前端界面
- 高并发处理能力（继承TinyWebServer优势）
- 扩展性强的模块化架构
- 适合简历展示的技术亮点

这个项目将充分展现对C++系统编程、Web开发、数据库设计等技能的掌握，具有很好的简历价值。