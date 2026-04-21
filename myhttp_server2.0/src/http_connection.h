#pragma once
#include <chrono>
#include <queue>
#include <string>
#include "uniquefd.h"

struct HttpConnection {
    UniqueFd fd;
    std::string readBuffer;
    std::string writeBuffer;
    size_t writePos = 0;
    std::queue<std::string> writeQueue;
    bool keepAlive = false;
    std::chrono::steady_clock::time_point lastActive = std::chrono::steady_clock::now();
    //size_t storedBytes = 0;
    HttpConnection() = default;
    explicit HttpConnection(UniqueFd&& clientFd) : fd(std::move(clientFd)) {}

    HttpConnection(const HttpConnection&) = delete;
    HttpConnection& operator=(const HttpConnection&) = delete;
    HttpConnection(HttpConnection&&) = default;
    HttpConnection& operator=(HttpConnection&&) = default;

    void clearWriteBuffer() {
        writeBuffer.clear();
        writePos = 0;
    }

    // bool ifSlow(size_t MAX_BYTES =1048576)
    // {
    //     if(storedBytes>MAX_BYTES)
    //         return true;
        
    //     return false;
    // }
    // void addBytes(size_t bytes)
    // {
    //     storedBytes += bytes;
    // }
    // void subtractBytes(size_t bytes)
    // {
    //     storedBytes -= bytes;
    // }
    bool hasPendingOutput() const {
        return writeBuffer.empty() && writeQueue.empty();
    }

    void loadOneMessage() {
        if (writeBuffer.empty() && !writeQueue.empty()) {
            writeBuffer = std::move(writeQueue.front());
            writeQueue.pop();
        }
    }

    void enqueueResponse(std::string msg) {
        writeQueue.push(std::move(msg));
        //addBytes(msg.length());
    }
};
