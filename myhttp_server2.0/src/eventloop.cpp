#include <arpa/inet.h>
#include <cerrno>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include "uniquefd.h"
#include "eventloop.h"
#include "Log.h"
#include "http_parser.h"
#include "http_response.h"
#include "route_handlers.h"
#include "socket_util.h"
#include "route_handlers.h"

EventLoop::EventLoop(): running_(true)
,epoll_()
,wwwRoot_("./www")
,wakefd_(eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC))
{
    if(wakefd_.getfd() == -1)
    {
        LOG_ERROR("wakefd_ create failed!");
    }
    epoll_.addFd(wakefd_.getfd());
    epoll_.modEpollToRead(wakefd_.getfd());

}

EventLoop::~EventLoop(){}

bool EventLoop::init(const std::string& host,const std::string& username,const std::string& password,const std::string& databasename)
{
    if(!initDatabase(host,username,password,databasename))
        return false;

    return true;
}
void EventLoop::loop(const std::string& host,const std::string& username,const std::string& password,const std::string& databasename)
{
    if(!init(host,username,password,databasename))
    {
        LOG_ERROR("eventloop initdatabase failed!");
        return ;
    }

    while (running_) 
    {
        std::vector<epoll_event> events = epoll_.wait(-1);
        if (events.empty()) {
            if (errno == EINTR) continue;

            LOG_ERROR("epoll_wait error!");
            return ;
        }

        for (const auto& event : events)
        {
            int fd = event.data.fd;
            uint32_t ev = event.events;
            //有新连接，入队
            if(fd == wakefd_.getfd())
            {
                if(!running_)
                    break;

                handleWakeup();
                continue;
            }
            if (ev & EPOLLIN) {
                handleReadEvent(fd);
            }
            if (ev & EPOLLOUT) {
                handleWriteEvent(fd);
            }

            if (ev & (EPOLLERR | EPOLLHUP)) {
                closeClient(fd);
            }
        }
    }
}

void EventLoop::stop()
{
    running_ = false;
    uint64_t stop = 1;
    write(wakefd_.getfd(),(void*)&stop,sizeof(stop));
}
void EventLoop::enqueueNewConn(UniqueFd&& clientfd)
{
    uint64_t one = 1;
    {
        std::unique_lock<std::mutex>lock(mutex_);
        newConnQueue_.emplace(std::move(clientfd));
    }
    write(wakefd_.getfd(),(void*)&one,sizeof(one));
}
void EventLoop::enqueueNewConn(std::vector<UniqueFd>& clientfd)
{
    uint64_t one = 1;
    {
        std::unique_lock<std::mutex>lock(mutex_);
        for(auto& fd : clientfd)
        {
            newConnQueue_.emplace(std::move(fd));
        }
    }
    write(wakefd_.getfd(),(void*)&one,sizeof(one));
}
//处理新连接
void EventLoop::handleWakeup()
{
    uint64_t one = 1;
    std::queue<UniqueFd>fds;
    read(wakefd_.getfd(),(void*)&one,sizeof(one));
    {
        std::unique_lock<std::mutex>lock(mutex_);
        std::swap(fds,newConnQueue_);
    }
   
    while(!fds.empty())
    {
        std::unique_ptr<HttpConnection>newClient = std::make_unique<HttpConnection>(std::move(fds.front()));
        fds.pop();
        int clientfd = (newClient.get()->fd).getfd();
        epoll_.addFd(clientfd,EPOLLIN | EPOLLET);
        connections_.emplace(clientfd,std::move(newClient));
    }
}

void EventLoop::handleReadEvent(int fd)
{
    recvAll(fd);
}
void EventLoop::handleWriteEvent(int fd)
{
    flushOneClient(fd);
}

void EventLoop::closeClient(int fd)
{
    auto it = connections_.find(fd);
    if (it == connections_.end())
            return;

    epoll_.delFd(fd);
    connections_.erase(fd);
    LOG_INFO("client fd="+std::to_string(fd)+"is closed!");
}

HttpConnection* EventLoop::findClient(int fd)
{
    auto it = connections_.find(fd);
    if(it == connections_.end())   
        return nullptr;

    return it->second.get();
}

void EventLoop::recvAll(int fd)
{
    HttpConnection* client = findClient(fd);
    if (!client) return;

    //如果是慢客户就关闭连接，并且返回，防止浪费内存资源
    // if(client->ifSlow())
    // {
    //     closeClient(fd);
    //     return ;
    // }
    while (true) {
        char buffer[4096];
        ssize_t bytesRead = ::recv(fd, buffer, sizeof(buffer), 0);
        if (bytesRead > 0) {
            client->readBuffer.append(buffer, static_cast<size_t>(bytesRead));
            //client->lastActive = std::chrono::steady_clock::now();
            continue;
        }
        if (bytesRead == 0) {
            LOG_INFO("client fd=" + std::to_string(fd) + " disconnected");
            closeClient(fd);
            return;
        }
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        //LOG_WARN("recv failed, closing client fd=" + std::to_string(fd));
        closeClient(fd);
        return;
    }
    handleReadBuffer(fd);
}

void EventLoop::handleReadBuffer(int fd)
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

void EventLoop::handleRequest(int fd, const HttpRequest& req)
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

bool EventLoop::initDatabase(const std::string& host,const std::string& username,const std::string& password,const std::string& databasename)
{
    if (!db_.connect(host,username, password, databasename, 3306)) 
    {
        LOG_WARN("database connect failed, API register/login will not work");
        return false;
    }
    LOG_INFO("database connected");
    return true;
}

void EventLoop::flushOneClient(int fd)
{
    HttpConnection* client = findClient(fd);
    if (!client) return;

    while (true) {
        if (client->hasPendingOutput()) {
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
        while (pos < msg.size()) {
            ssize_t bytesSend = ::send(fd, msg.data() + pos, msg.size() - pos, 0);
            if (bytesSend > 0) {
                //client->subtractBytes(static_cast<size_t>(bytesSend));
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
bool EventLoop::mapUrlToFilePath(const std::string& path, std::string& filePath)
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
void EventLoop::handleStaticFile(HttpConnection& client, const HttpRequest& req)
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