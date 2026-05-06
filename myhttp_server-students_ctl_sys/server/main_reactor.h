#include "../src/uniquefd.h"
#include "../src/epoll.h"
#include <thread>
#include <vector>
class Epoll;
class EventLoop;
class MainReactor{

public:
    MainReactor(int port,int reactor_size ,const std::string& host,const std::string& username,const std::string& password,const std::string& databasename);
    ~MainReactor();
    void run();
private:
    void enqueueNewFd(UniqueFd && clientfd);
    bool init();
    bool createListenSocket();
    void handleAccept();
    void handleExit();
private:
    int port_;
    Epoll epoll_;
    UniqueFd listenfd_;
    EventLoop* reactors_;
    int cur_;
    int reactor_size_;
    bool running_;
    std::vector<std::thread>threads;
};