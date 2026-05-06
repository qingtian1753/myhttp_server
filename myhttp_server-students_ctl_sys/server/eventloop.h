#include <unordered_map>
#include <queue>
#include <mutex>
#include <memory>
#include <string>
#include <vector>
#include "../src/http_request.h"
#include "../db/database.h"
#include "../src/epoll.h"
//#include "threadpool.h"

class UniqueFd;
class HttpConnection;

class EventLoop
{
public:
    EventLoop();
    ~EventLoop();

    void loop(const std::string& host,const std::string& username,const std::string& password,const std::string& databasename);
    void enqueueNewConn(UniqueFd&& clientfd);
    void stop();
private:
    bool init(const std::string& host,const std::string& username,const std::string& password,const std::string& databasename);
    void handleWakeup();
    void handleClientEvent();
    void handleReadEvent(int fd);
    void handleWriteEvent(int fd);
    void closeClient(int fd);
    HttpConnection* findClient(int fd);
    void recvAll(int fd);
    void handleReadBuffer(int fd);
    void handleRequest(int fd, const HttpRequest& req);
    bool initDatabase(const std::string& host,const std::string& username,const std::string& password,const std::string& databasename);
    void flushOneClient(int fd);
    bool mapUrlToFilePath(const std::string& path, std::string& filePath);
    void handleStaticFile(HttpConnection& client, const HttpRequest& req);
private:
    Epoll epoll_;
    //通过wakefd的事件告诉epoll，连接队列里有新连接，不然它一直卡在epoll_wait可不行
    UniqueFd wakefd_;
    std::mutex mutex_;
    std::queue<UniqueFd>newConnQueue_;
    std::unordered_map<int,std::unique_ptr<HttpConnection>>connections_;
    bool running_;
    Database db_;
    std::string wwwRoot_;
    // ThreadPool threads;
};