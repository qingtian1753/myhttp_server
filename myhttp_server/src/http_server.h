#pragma once
#include <unordered_map>
#include <string>
#include "database.h"
#include "epoll.h"
#include "http_connection.h"
#include "http_request.h"
#include "uniquefd.h"

class HttpServer {
public:
    explicit HttpServer(int port, std::string wwwRoot = "./www", int maxEvents = 1024);
    ~HttpServer();

    bool init();
    void run();

private:
    bool createListenSocket();
    bool initDatabase();

    void handleAccept();
    void handleReadEvent(int fd);
    void handleWriteEvent(int fd);
    void recvAll(int fd);
    void handleReadBuffer(int fd);
    void handleRequest(int fd, const HttpRequest& req);
    void flushOneClient(int fd);
    void closeClient(int fd);


    HttpConnection* findClient(int fd);
    bool mapUrlToFilePath(const std::string& path, std::string& filePath);
    void handleStaticFile(HttpConnection& client, const HttpRequest& req);

private:
    Epoll epoll_;
    int port_;
    UniqueFd listenFd_;
    bool running_;
    std::string wwwRoot_;
    Database db_;
    std::unordered_map<int, HttpConnection> clients_;
};
