#pragma once
#include <sys/epoll.h>
#include <vector>
#include <unistd.h>

class Epoll {
public:
    explicit Epoll(int maxEvents = 1024);
    ~Epoll();

    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;

    int getfd() const;
    std::vector<epoll_event> wait(int timeout = -1);
    bool addFd(int fd);
    void modEpollToRead(int fd);
    void modEpollToWrite(int fd);
    void delFd(int fd);

private:
    int epfd_;
    epoll_event* events_;
    int maxEvents_;
};
