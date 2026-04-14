# myhttp_server


基于 C++11/14 和 epoll 的高性能静态资源服务器与用户认证系统。支持高并发连接、keep-alive 复用、MySQL 用户注册/登录，以及慢客户端保护机制。

##  核心特性

- **事件驱动架构**：采用 epoll (ET 模式) + 非阻塞 I/O，支持单线程高并发。
- **HTTP/1.1 协议解析**：根据"\r\n\r\n"与"Content-Length"正确处理分段到达的请求头部与 Body。
- **连接复用**：支持 HTTP/1.1 keep-alive，减少 TCP 握手开销。
- **数据库集成**：MySQL 预处理语句防 SQL 注入，实现用户注册与登录。
- **慢客户端保护**：基于待发送字节数阈值的连接淘汰机制，防止内存堆积。
- **RAII 资源管理**：文件描述符、MySQL 连接自动释放。
- **静态资源服务**：支持 HTML/CSS/JS/图片等常见 MIME 类型，防止路径穿越攻击。

## 🛠 技术栈

- **语言**：C++11/14
- **网络**：epoll、非阻塞 I/O、Reactor 模型
- **数据库**：MySQL C API、PreparedStatement
- **工具**：Make/G++、Linux 环境

##  项目结构
.
├── src/
│ ├── main.cpp # 入口
│ ├── http_server.cpp/.h # 服务器主逻辑
│ ├── epoll.cpp/.h # epoll 封装
│ ├── http_parser.cpp/.h # HTTP 请求解析器
│ ├── http_response.cpp/.h # 响应构建器
│ ├── http_connection.h # 连接上下文
│ ├── database.cpp/.h # MySQL 封装
│ ├── route_handlers.cpp/.h # API 路由分发
│ ├── uniquefd.h # RAII 文件描述符
│ ├── socket_util.cpp/.h # socket 辅助函数
│ └── Log.cpp/.h # 简易日志
├── www/ # 静态资源根目录
├── Makefile
└── README.md

### 依赖
- Linux 环境 (epoll)
- MySQL Server (5.7+)
- g++ (支持 C++14)

### 编译

git clone https://github.com/你的ID/myhttp_server.git
cd TinyHttpServer
make
