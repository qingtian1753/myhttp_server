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

    bool hasPendingOutput() const {
        return !writeBuffer.empty() || !writeQueue.empty();
    }

    void loadOneMessage() {
        if (writeBuffer.empty() && !writeQueue.empty()) {
            writeBuffer = std::move(writeQueue.front());
            writeQueue.pop();
        }
    }

    void enqueueResponse(std::string msg) {
        writeQueue.push(std::move(msg));
    }
};
