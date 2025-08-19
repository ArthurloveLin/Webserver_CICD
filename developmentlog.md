# TinyWebServer博客系统开发日志

## 项目概述
基于TinyWebServer C++框架开发的完整博客系统，包含文章管理、分类管理、评论系统、管理后台等功能。

## 开发历程

### 第一阶段：项目初始化与架构设计
**时间节点**：开发开始
**目标**：理解用户需求，设计系统架构

#### 用户需求分析
- 用户提供了详细的需求文档 `blog.md`
- 要求在TinyWebServer框架基础上实现完整博客系统
- 需要支持UTF-8中文显示
- 包含前后端完整功能

#### 系统架构设计
1. **数据库设计**
   - 用户表(user)：用户认证和权限管理
   - 文章表(articles)：文章内容、状态管理
   - 分类表(categories)：文章分类
   - 标签表(tags)：文章标签
   - 评论表(comments)：评论系统
   - 点赞表(likes)：点赞功能
   - 设置表(admin_settings)：系统配置

2. **代码架构**
   - `BlogHandler`类：核心业务逻辑处理
   - `TemplateEngine`类：模板渲染引擎
   - HTTP路由集成：扩展TinyWebServer的HTTP处理
   - 数据库集成：复用TinyWebServer的连接池

### 第二阶段：核心功能实现
**时间节点**：第一轮开发
**主要成果**：完成基础博客功能

#### 数据库层实现
- 创建 `blog_schema.sql`：完整的数据库表结构
- 设计触发器：自动更新文章计数
- 外键约束：保证数据完整性
- 索引优化：提升查询性能

#### 后端功能实现
1. **BlogHandler核心功能**
   ```cpp
   // 主要方法
   - render_blog_index()      // 博客首页
   - render_article_detail()  // 文章详情
   - render_admin_dashboard() // 管理后台
   - render_admin_editor()    // 文章编辑器
   - api_create_article()     // 创建文章API
   - api_update_article()     // 更新文章API
   - api_delete_article()     // 删除文章API
   ```

2. **模板引擎实现**
   ```cpp
   // TemplateEngine功能
   - 变量替换：{{variable}}
   - 条件判断：{{#if condition}}
   - 循环处理：{{#each items}}
   - 包含文件：{{> partial}}
   - HTML转义：安全处理
   ```

3. **HTTP路由集成**
   - 扩展 `http_conn.cpp`
   - 博客路由识别：`/blog/*`
   - 静态资源服务：CSS/JS/图片
   - API接口路由：RESTful设计

#### 前端界面实现
1. **响应式CSS设计**
   - `main.css`：主要样式
   - `admin.css`：管理后台样式
   - 移动端适配
   - 现代化UI设计

2. **JavaScript交互**
   - 表单提交处理
   - AJAX请求
   - 错误处理
   - 用户反馈

### 第三阶段：问题发现与修复

#### 问题1：中文编码乱码
**现象**：页面中文显示为乱码
**排查过程**：
1. 检查数据库字符集配置
2. 检查HTTP响应头charset设置
3. 检查编译器编码选项

**解决方案**：
```makefile
# 添加UTF-8编译选项
CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
```
```cpp
// HTTP响应头添加charset
"Content-Type: text/html; charset=utf-8"
```

#### 问题2：MySQL函数编译错误
**现象**：`mysql_real_escape_string`参数不匹配
**解决方案**：
```cpp
// 替换为自定义HTML转义函数
string html_escape(const string& str) {
    // 转义 &, <, >, ", ' 等字符
}
```

#### 问题3：模板引擎编码问题
**现象**：模板文件编码导致中文乱码
**解决方案**：
- 放弃模板文件方式
- 改为直接HTML字符串生成
- 避免文件编码问题

### 第四阶段：核心问题解决

#### 问题4：文章保存失败 - 网络错误
**现象**：
- 前端提示"保存失败：网络错误"
- 后台显示文章创建成功
- 数据实际已保存到数据库

**深度排查过程**：

1. **HTTP方法支持问题**
   ```cpp
   // 发现问题：http_conn.cpp只支持GET和POST
   if (strcasecmp(method, "GET") == 0)
       m_method = GET;
   else if (strcasecmp(method, "POST") == 0)
       m_method = POST;
   else
       return BAD_REQUEST; // PUT/DELETE被拒绝
   ```

2. **方法覆盖机制实现**
   ```cpp
   // 添加_method字段处理
   string actual_method = method;
   if (method == "POST" && !post_data.empty()) {
       map<string, string> form_data = parse_post_data(post_data);
       if (!form_data["_method"].empty()) {
           actual_method = form_data["_method"];
       }
   }
   ```

3. **JSON响应格式问题**
   ```
   // 错误的响应格式（响应头连在一起）
   HTTP/1.1 200 OKContent-Type: application/json...
   
   // 正确的响应格式应该有\r\n分隔
   HTTP/1.1 200 OK\r\n
   Content-Type: application/json; charset=utf-8\r\n
   ```

**最终解决方案**：
1. 扩展HTTP方法支持：
   ```cpp
   else if (strcasecmp(method, "PUT") == 0) {
       m_method = PUT;
       cgi = 1;
   }
   else if (strcasecmp(method, "DELETE") == 0) {
       m_method = DELETE;  
       cgi = 1;
   }
   ```

2. 简化JSON响应：
   ```cpp
   // 只返回JSON数据，让HTTP层处理响应头
   string BlogHandler::build_json_response(const string& json_data, int status_code) {
       return json_data;
   }
   ```

#### 问题5：评论功能页面跳转
**现象**：提交评论后跳转到奇怪页面
**原因**：传统form POST提交会导致页面跳转
**解决方案**：
```javascript
// 改为AJAX提交
document.getElementById('comment-form').addEventListener('submit', function(e) {
    e.preventDefault();
    
    fetch('/blog/api/comments', {
        method: 'POST',
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            alert('评论发布成功！');
            // 清空表单而不是跳转
        }
    });
});
```

#### 问题6：数据库连接配置
**现象**：API报错"Table 'webserver.articles' doesn't exist"
**原因**：数据库配置不匹配
**解决方案**：
- 创建数据库迁移脚本
- 统一使用webserver数据库
- 修改现有user表结构以支持博客功能

### 第五阶段：调试与优化

#### 调试策略
1. **分层调试**：
   - 前端：浏览器开发者工具
   - 后端：printf调试输出
   - 数据库：SQL查询验证

2. **关键调试点**：
   ```cpp
   // 请求处理调试
   printf("Blog request: %s %s (actual method: %s)\n", 
          method.c_str(), url.c_str(), actual_method.c_str());
   
   // SQL执行调试
   printf("Executing SQL: %s\n", query.str().c_str());
   
   // 响应数据调试
   printf("Returning JSON data only: %s\n", result.c_str());
   ```

3. **前端错误处理增强**：
   ```javascript
   .then(response => response.text().then(text => {
       console.log('Response text:', text);
       try {
           return JSON.parse(text);
       } catch (e) {
           throw new Error('Invalid JSON: ' + text);
       }
   }))
   ```

### 第六阶段：最终优化与测试

#### 代码优化
1. **错误处理完善**
2. **用户体验改进**
3. **性能优化**
4. **安全性加固**

#### 测试验证
1. **功能测试**：
   - 文章创建/编辑/删除
   - 评论发布
   - 分类管理
   - 管理后台

2. **边界测试**：
   - 空数据处理
   - 错误输入处理
   - 网络异常处理

## 技术栈总结

### 后端技术
- **语言**：C++14
- **框架**：TinyWebServer
- **数据库**：MySQL 8.0
- **编译器**：GCC
- **构建工具**：Make

### 前端技术
- **语言**：HTML5, CSS3, JavaScript ES6
- **设计模式**：响应式设计
- **交互**：原生JavaScript + Fetch API

### 开发工具
- **编辑器**：Claude Code
- **版本控制**：Git
- **数据库管理**：MySQL命令行
- **调试工具**：浏览器开发者工具 + printf调试

## 关键技术难点与解决方案

### 1. C++与Web开发结合
**挑战**：C++不是传统Web开发语言
**解决**：
- 字符串处理优化
- HTTP协议实现
- JSON数据处理
- 模板引擎设计

### 2. 字符编码处理
**挑战**：中文UTF-8编码在C++中的处理
**解决**：
- 编译器编码选项配置
- HTTP响应头正确设置
- 数据库字符集统一

### 3. HTTP协议扩展
**挑战**：原框架只支持基础HTTP方法
**解决**：
- 扩展方法解析
- 实现方法覆盖机制
- 保持向后兼容

### 4. 前后端数据交互
**挑战**：JSON响应格式问题
**解决**：
- 简化响应处理
- 增强错误处理
- 完善调试机制

## 项目成果

### 功能完成度
- ✅ 博客首页展示
- ✅ 文章详情页面
- ✅ 文章创建/编辑/删除
- ✅ 分类管理
- ✅ 评论系统
- ✅ 管理后台
- ✅ 响应式设计
- ✅ UTF-8中文支持

### 代码质量
- **可维护性**：模块化设计，职责分离
- **可扩展性**：插件化架构，易于功能扩展
- **健壮性**：完善的错误处理机制
- **性能**：数据库连接池，SQL优化

### 技术创新点
1. **C++模板引擎**：自研轻量级模板引擎
2. **HTTP方法扩展**：支持RESTful API设计
3. **方法覆盖机制**：优雅处理PUT/DELETE请求
4. **一体化架构**：前后端一体化部署

## 经验总结

### 技术经验
1. **系统性思考**：从需求分析到架构设计再到实现
2. **问题定位能力**：分层调试，逐步排除
3. **代码调试技巧**：多层次调试信息输出
4. **Web协议理解**：深入理解HTTP协议细节

### 开发流程
1. **需求理解**：充分理解用户需求文档
2. **架构设计**：先设计再编码
3. **迭代开发**：小步快跑，逐步完善
4. **问题驱动**：以解决实际问题为导向

### 调试方法论
1. **现象观察**：准确描述问题现象
2. **假设验证**：基于经验提出假设并验证
3. **分层排查**：从前端到后端到数据库逐层排查
4. **工具运用**：充分利用各种调试工具

## 未来改进方向

### 功能扩展
- [ ] 文章搜索功能
- [ ] 用户注册登录
- [ ] 文章标签系统
- [ ] RSS订阅
- [ ] 文章统计分析

### 技术优化
- [ ] 缓存机制实现
- [ ] SQL注入防护加强
- [ ] 性能监控
- [ ] 日志系统完善
- [ ] 单元测试覆盖

### 部署优化
- [ ] Docker容器化
- [ ] CI/CD流水线
- [ ] 负载均衡支持
- [ ] 数据库读写分离

### 第七阶段：数据库连接稳定性问题修复

#### 问题7：服务器运行期间文章消失和登录权限丢失
**时间节点**：2025年8月19日
**现象**：
- 服务器运行一段时间后，文章会消失
- 用户登录权限莫名其妙地丢失
- 数据库查询可能返回空结果

**问题分析**：
通过深入分析发现这是一个典型的数据库连接超时问题：

1. **MySQL连接超时配置**：
   ```sql
   -- MySQL默认超时设置
   wait_timeout = 28800        -- 8小时空闲后断开连接
   interactive_timeout = 28800 -- 8小时交互超时
   ```

2. **连接池缺陷**：
   - 原始连接池没有设置MySQL连接选项
   - 缺乏连接保活机制（ping或reconnect）
   - 当MySQL服务器主动断开连接后，应用程序仍在使用失效连接
   - 导致查询返回错误结果或空结果

3. **Session管理问题**：
   - Session数据存储在内存中，服务器重启会丢失
   - 缺乏Session过期清理机制
   - 没有Session生命周期管理

**根本原因**：
```cpp
// 原始代码在sql_connection_pool.cpp中
con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), 
                        DBName.c_str(), Port, NULL, 0);
// 缺少连接选项设置，没有自动重连机制
```

**解决方案**：

1. **数据库连接池优化**：
   ```cpp
   // 在连接初始化时添加MySQL选项
   bool reconnect = true;
   mysql_options(con, MYSQL_OPT_RECONNECT, &reconnect);
   mysql_options(con, MYSQL_SET_CHARSET_NAME, "utf8mb4");
   
   // 设置连接超时
   unsigned int timeout = 3600; // 1小时
   mysql_options(con, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
   mysql_options(con, MYSQL_OPT_READ_TIMEOUT, &timeout);
   mysql_options(con, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
   
   // 设置session级别超时
   mysql_query(con, "SET SESSION wait_timeout = 7200"); // 2小时
   mysql_query(con, "SET SESSION interactive_timeout = 7200");
   ```

2. **连接健康检查机制**：
   ```cpp
   // 在GetConnection()方法中添加连接检查
   if (mysql_ping(con) != 0) {
       LOG_ERROR("MySQL connection lost, attempting to reconnect...");
       mysql_close(con);
       
       // 重新创建连接
       con = mysql_init(con);
       // ... 重新设置连接选项和重连
   }
   ```

3. **Session管理改进**：
   ```cpp
   // 添加Session过期机制
   struct UserSession {
       static const time_t SESSION_TIMEOUT = 7200; // 2小时超时
       
       bool is_expired() const {
           return (time(nullptr) - last_access) > SESSION_TIMEOUT;
       }
       
       void update_access_time() {
           last_access = time(nullptr);
       }
   };
   
   // 自动清理过期Session
   void cleanup_expired_sessions() {
       // 遍历并删除过期的session
   }
   ```

**修复效果**：
- ✅ 解决了数据库连接超时导致的文章消失问题
- ✅ 修复了登录权限意外丢失的问题
- ✅ 增强了系统的长期运行稳定性
- ✅ 添加了自动重连机制，提高了可靠性
- ✅ 优化了Session生命周期管理

**技术要点**：
- **连接保活**：使用mysql_ping()检查连接状态
- **自动重连**：MYSQL_OPT_RECONNECT选项
- **超时配置**：合理设置各种超时参数
- **Session管理**：内存Session + 过期清理机制
- **错误处理**：完善的连接失败处理逻辑

这次修复解决了影响系统稳定性的重大问题，确保博客系统能够长期稳定运行。

## 结语

这次TinyWebServer博客系统的开发是一次完整的全栈开发实践，从最初的需求分析到最终的问题解决，体现了软件开发的完整生命周期。

通过这个项目，我们不仅实现了一个功能完善的博客系统，更重要的是积累了：
- C++Web开发的实践经验
- 复杂问题的分析和解决能力
- 全栈开发的系统性思维
- 调试和优化的方法论
- 数据库连接管理的实战经验

这些经验将为后续的项目开发提供宝贵的参考和指导。

---

**开发团队**：Claude Code + 用户协作开发  
**开发时间**：2025年8月15日 - 2025年8月19日  
**项目状态**：核心功能完成，稳定性问题已修复