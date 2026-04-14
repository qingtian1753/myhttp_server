#include "epoll.h"
#include "Log.h"

Epoll::Epoll(int maxEvents) : epfd_(epoll_create1(0)), events_(nullptr), maxEvents_(maxEvents)
{
    if (epfd_ < 0) {
        LOG_ERROR("epoll_create1 failed");
    }
    events_ = new epoll_event[maxEvents_];
}

Epoll::~Epoll()
{
    if (epfd_ != -1) {
        close(epfd_);
    }
    delete[] events_;
}

int Epoll::getfd() const
{
    return epfd_;
}

std::vector<epoll_event> Epoll::wait(int timeout)
{
    std::vector<epoll_event> fds;
    int n = epoll_wait(epfd_, events_, maxEvents_, timeout);
    if (n < 0) {
        return fds;
    }
    for (int i = 0; i < n; ++i) {
        fds.emplace_back(events_[i]);
    }
    return fds;
}

bool Epoll::addFd(int fd)
{
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        LOG_ERROR("epoll addFd failed");
        return false;
    }
    return true;
}

void Epoll::modEpollToRead(int fd)
{
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
        LOG_ERROR("modEpollToRead failed");
    }
}

void Epoll::modEpollToWrite(int fd)
{
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
    if (epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
        LOG_ERROR("modEpollToWrite failed");
    }
}

void Epoll::delFd(int fd)
{
    if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        LOG_ERROR("delFd failed");
    }
}
