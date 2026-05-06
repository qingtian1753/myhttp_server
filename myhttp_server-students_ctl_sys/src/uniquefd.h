#pragma once
#include <unistd.h>

class UniqueFd {
public:
    UniqueFd() : fd_(-1) {}
    explicit UniqueFd(int fd) : fd_(fd) {}
    ~UniqueFd() { reset(); }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept : fd_(other.release()) {}
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    int getfd() const { return fd_; }
    bool valid() const { return fd_ != -1; }

    int release() {
        int tmp = fd_;
        fd_ = -1;
        return tmp;
    }

    void reset(int newfd = -1) {
        if (fd_ != -1) {
            ::close(fd_);
        }
        fd_ = newfd;
    }

private:
    int fd_;
};
