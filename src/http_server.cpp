#include <arpa/inet.h>
#include <cerrno>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

#include "http_server.h"
#include "Log.h"
#include "http_parser.h"
#include "http_response.h"
#include "route_handlers.h"
#include "socket_util.h"

HttpServer::HttpServer(int port, std::string wwwRoot, int maxEvents)
    : epoll_(maxEvents), port_(port), running_(true), wwwRoot_(std::move(wwwRoot)) {}

HttpServer::~HttpServer() = default;

bool HttpServer::init(const std::string& host,const std::string& owner_name,const std::string& password,const std::string& database_name,const int& port)
{
    if (!createListenSocket() || !initDatabase(host, owner_name, password, database_name, port)) 
        return false;

    if (!epoll_.addFd(listenFd_.getfd())) {
        LOG_ERROR("add listen fd to epoll failed");
        return false;
    }
    return true;
}

bool HttpServer::createListenSocket()
{
    UniqueFd fd(::socket(AF_INET, SOCK_STREAM, 0));
    if (!fd.valid()) {
        LOG_ERROR("socket create failed");
        return false;
    }
        

    int reuse = 1;
    if (setsockopt(fd.getfd(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        LOG_ERROR("setsockopt reuseaddr failed");
        return false;
    }

    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port_);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd.getfd(), reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) == -1 ||
        listen(fd.getfd(), 256) == -1) {
        LOG_ERROR("bind or listen failed");
        return false;
    }

    if (!SocketUtil::setSocketNonblockNoDelay(fd.getfd())) {
        LOG_ERROR("set listen socket nonblock nodelay failed");
        return false;
    }

    listenFd_ = std::move(fd);
    LOG_INFO("http server started on port=" + std::to_string(port_));
    return true;
}

//连接数据库                 
bool HttpServer::initDatabase(const std::string& host,const std::string& owner_name,const std::string& password,const std::string& database_name,int port)
{
    if (!db_.connect(host, owner_name, password, database_name, port)) {
        LOG_WARN("database connect failed, API register/login will not work");
        return false;
    }
    LOG_INFO("database connected");
    return true;
}

void HttpServer::run()
{
    while (running_) {
        std::vector<epoll_event> events = epoll_.wait(-1);
        if (events.empty()) {
            if (errno == EINTR) continue;

            LOG_ERROR("epoll_wait error!");
            return ;
        }

        for (const auto& event : events) {
            int fd = event.data.fd;
            uint32_t ev = event.events;

            if (fd == listenFd_.getfd()) {
                handleAccept();
                continue;
            }
            if (ev & EPOLLIN) {
                handleReadEvent(fd);
            }
            if (!findClient(fd)) continue;
            if (ev & EPOLLOUT) {
                handleWriteEvent(fd);
            }
            if (!findClient(fd)) continue;

            if (ev & (EPOLLERR | EPOLLHUP)) {
                closeClient(fd);
            }
        }
    }
}

void HttpServer::handleAccept()
{
    while (true) {
        UniqueFd clientFd(::accept(listenFd_.getfd(), nullptr, nullptr));
        if (!clientFd.valid()) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            LOG_ERROR("accept failed");
            return;
        }

        if (!SocketUtil::setSocketNonblockNoDelay(clientFd.getfd())) {
            LOG_ERROR("set client socket nonblock failed");
            continue;
        }

        if (!epoll_.addFd(clientFd.getfd())) {
            LOG_ERROR("add client fd to epoll failed");
            continue;
        }

        int key = clientFd.getfd();
        clients_.emplace(key, HttpConnection(std::move(clientFd)));
        LOG_INFO("new http client fd=" + std::to_string(key));
    }
}

void HttpServer::handleReadEvent(int fd) { recvAll(fd); }
void HttpServer::handleWriteEvent(int fd) { flushOneClient(fd); }

//查找这个fd，如果存在则返回对应的指针否则则为nullptr
HttpConnection* HttpServer::findClient(int fd)
{
    auto it = clients_.find(fd);
    if (it == clients_.end()) return nullptr;
    return &it->second;
}

void HttpServer::recvAll(int fd)
{
    HttpConnection* client = findClient(fd);
    if (!client) return;
    //如果是慢客户发来的请求，直接关闭(慢客户端指的是一直发请求，但是又不接收数据导致服务器的数据一直发不出去，堆积在消息队列里，占用服务器内存)
    if(client->ifSlow())
    {
        LOG_WARN("the slow client is closed!");
        closeClient(fd);
        return ;
    }

    while (true) {
        char buffer[4096];
        ssize_t bytesRead = ::recv(fd, buffer, sizeof(buffer), 0);
        if (bytesRead > 0) {
            client->readBuffer.append(buffer, static_cast<size_t>(bytesRead));
            client->lastActive = std::chrono::steady_clock::now();
            continue;
        }
        if (bytesRead == 0) {
            LOG_INFO("client fd=" + std::to_string(fd) + " disconnected");
            closeClient(fd);
            return;
        }
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;

        LOG_WARN("recv failed, closing client fd=" + std::to_string(fd));
        closeClient(fd);
        return;
    }
    handleReadBuffer(fd);
}

void HttpServer::handleReadBuffer(int fd)
{
    HttpConnection* client = findClient(fd);
    if (!client) return;

    while (!client->readBuffer.empty()) {
        HttpRequest req;
        HttpParser::ParseStatus status = HttpParser::tryParseOne(client->readBuffer, req);
        if (status == HttpParser::ParseStatus::Incomplete) {
            return;
        }
        if (status == HttpParser::ParseStatus::BadRequest) {
            client->keepAlive = false;
            client->enqueueResponse(HttpResponse::buildErrorPage(400, "Bad Request", wwwRoot_ + "/400.html", false));
            epoll_.modEpollToWrite(fd);
            return;
        }
        handleRequest(fd, req);
        if (!findClient(fd)) return;
    }
}

void HttpServer::handleRequest(int fd, const HttpRequest& req)
{
    HttpConnection* client = findClient(fd);
    if (!client) return;

    auto it = req.headers.find("Connection");
    bool keepAlive = false;
   
    if (req.version == "HTTP/1.1")
    { //http/1.1默认keepAlive是true，再判断是否设置为了false
        keepAlive = true;
        if (it != req.headers.end() && it->second == "close") keepAlive = false;
    } else if (req.version == "HTTP/1.0") 
    { // http/1.0默认是false
        keepAlive = false;
        if (it != req.headers.end() && it->second == "keep-alive") keepAlive = true;
    }

    client->keepAlive = keepAlive;

    LOG_INFO("fd=" + std::to_string(fd) + " " + req.method + " " + req.path);

    //先判断是不是访问接口，并更改epoll_event模式，等待发送消息
    if (route::dispatch(*client, req, db_)) {
        epoll_.modEpollToWrite(fd);
        return;
    }

    //如果不是访问接口，那么判断是不是GET请求，如果是则处理访问静态文件
    if (req.method == "GET") {
        handleStaticFile(*client, req);
        epoll_.modEpollToWrite(fd);
        return;
    }

    //如果请求是post但找不到接口或者是其他请求就返回这段html
    client->enqueueResponse(HttpResponse::buildErrorPage(405, "Method Not Allowed", wwwRoot_ + "/405.html", client->keepAlive));
    epoll_.modEpollToWrite(fd);
}

//将Url路径映射成磁盘文件路径
bool HttpServer::mapUrlToFilePath(const std::string& path, std::string& filePath)
{
    std::string cleanPath = path;

    //很多静态资源查询后面都会带查询参数
    //Url里?后面的是查询参数，不属于资源路径所以要截掉
    size_t qpos = cleanPath.find('?');
    if (qpos != std::string::npos) cleanPath = cleanPath.substr(0, qpos);

    //如果不是/开头那么直接返回false，错误路径
    if (cleanPath.empty() || cleanPath[0] != '/') return false;

    //不允许路径穿越，..会访问上级文件
    if (cleanPath.find("..") != std::string::npos) return false;

    //如果访问根路径，那么直接返回这段html
    if (cleanPath == "/") {
        filePath = wwwRoot_ + "/index.html";
        return true;
    }

    //否则则去找这个文件是否存在，存在则返回数据
    filePath = wwwRoot_ + cleanPath;
    return true;
}

void HttpServer::handleStaticFile(HttpConnection& client, const HttpRequest& req)
{
    std::string filePath;
    //如果访问路径有问题那么就设置keepAlive为false，不保持连接了
    if (!mapUrlToFilePath(req.path, filePath)) {
        client.keepAlive = false;
        client.enqueueResponse(HttpResponse::buildErrorPage(400, "Bad Request", wwwRoot_ + "/400.html", false));
        return;
    }

    //判断是否是要用二进制读取文件
    bool binary = filePath.find(".png") != std::string::npos || filePath.find(".jpg") != std::string::npos ||
                  filePath.find(".jpeg") != std::string::npos || filePath.find(".gif") != std::string::npos ||
                  filePath.find(".ico") != std::string::npos;

    std::ifstream file(filePath, binary ? std::ios::binary : std::ios::in);
    if (!file) {
        client.enqueueResponse(HttpResponse::buildErrorPage(404, "Not Found", wwwRoot_ + "/404.html", client.keepAlive));
        return;
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    client.enqueueResponse(HttpResponse::buildFileResponse(200, "OK", filePath, oss.str(), client.keepAlive));
}

void HttpServer::flushOneClient(int fd)
{
    HttpConnection* client = findClient(fd);
    if (!client) return;
    while (true) {
        //判断是否发完,发完则进入if判断连接状态来决定是否要保持连接
        if (client->isOutEmpty()) {
            if (client->keepAlive) {
                epoll_.modEpollToRead(fd);
            } else {
                closeClient(fd);
            }
            return;
        }

        client->loadOneMessage();
        size_t& pos = client->writePos;
        std::string& msg = client->writeBuffer;
        while (pos < msg.size()) 
        {
            ssize_t bytesSend = ::send(fd, msg.data() + pos, msg.size() - pos, 0);
            if (bytesSend > 0) 
            {
                client->storedBytes-=static_cast<size_t>(bytesSend);
                pos += static_cast<size_t>(bytesSend);
                continue;
            }
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;

            LOG_WARN("send failed, closing client fd=" + std::to_string(fd));
            closeClient(fd);
            return;
        }
        client->clearWriteBuffer();
    }
}



void HttpServer::closeClient(int fd)
{
    auto it =clients_.find(fd);
    if(it == clients_.end())
        return ;
    epoll_.delFd(fd);
    //unordered_map<>擦除时会调用类的析构函数
    clients_.erase(it);
    LOG_INFO("client fd=" + std::to_string(fd) + " closed");
}