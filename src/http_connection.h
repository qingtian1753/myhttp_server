#pragma once
#include <chrono>
#include <queue>
#include <string>
#include "uniquefd.h"

struct HttpConnection {
    size_t storedBytes=0;
    UniqueFd fd;
    std::string readBuffer;
    std::string writeBuffer;
    size_t writePos=0;
    std::queue<std::string> writeQueue;
    bool keepAlive=false;
    std::chrono::steady_clock::time_point lastActive=std::chrono::steady_clock::now();

    HttpConnection() = delete;
    explicit HttpConnection(UniqueFd&& clientFd) : fd(std::move(clientFd)) {}

    //删除拷贝构造
    HttpConnection(const HttpConnection&) = delete;
    HttpConnection& operator=(const HttpConnection&) = delete;
    //允许移动
    HttpConnection(HttpConnection&&) = default;
    HttpConnection& operator=(HttpConnection&&) = default;

    void clearWriteBuffer() {
        writeBuffer.clear();
        writePos = 0;
    }
    
    //全部发完返回true
    bool isOutEmpty() const {
        return writeBuffer.empty() && writeQueue.empty();
    }

    void loadOneMessage() {
        if (writeBuffer.empty() && !writeQueue.empty()) {
            writeBuffer = std::move(writeQueue.front());
            writeQueue.pop();
        }
    }

    void enqueueResponse(std::string&& msg) {
        storedBytes +=msg.length();
        writeQueue.push(std::move(msg));
    }

    //判断这个客户端是否是慢客户端，如果积压了太多内容没发就要关闭它
    bool ifSlow(size_t MAXSTORED = 1048576)//1024*1024bytes
    {
        if(storedBytes>MAXSTORED)
            return true;
        return false;
    }
};
