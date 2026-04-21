#include <arpa/inet.h>
#include <cerrno>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>

#include "main_reactor.h"
#include "eventloop.h"
#include "socket_util.h"
#include "Log.h"

MainReactor::MainReactor(int port,int reactor_size,const std::string& host,const std::string& username,const std::string& password,const std::string& databasename)
:reactor_size_(reactor_size)
,running_(true)
,port_(port)
,cur_(0)
{
    epoll_.addFd(STDIN_FILENO);
    reactors_ = new EventLoop[reactor_size_];
    for(int i=0;i<reactor_size_;++i)
    {
        threads.emplace_back(std::thread([this,i,host,username,password,databasename](){
            reactors_[i].loop(host,username,password,databasename);
        }));
    }
}
MainReactor::~MainReactor()
{
    delete []reactors_;
}
void MainReactor::enqueueNewFd(UniqueFd&& clientfd)
{  
    reactors_[cur_].enqueueNewConn(std::move(clientfd));
    cur_ = (cur_+1)%reactor_size_;
}
void MainReactor::enqueueNewFd(std::vector<std::vector<UniqueFd>>& fds)
{
    for(int i=0;i<reactor_size_;++i)
    {
        if(fds[i].empty())
            continue;

        reactors_[i].enqueueNewConn(fds[i]);
    }
}
void MainReactor::run()
{
    if(!init())
    {
        LOG_ERROR("init() failed!");
        return ;
    }
    while(running_)
    {
        std::vector<epoll_event>events = epoll_.wait();
        if(events.size() == 1 && events[0].data.fd == -1)
                return ;

        for(auto& event : events)
        {
            int fd = event.data.fd;
            if(fd == listenfd_.getfd())
            {
                handleAccept();
            }
            else if(fd == STDIN_FILENO)
            {
                handleExit();
                break;
            } else {
                LOG_ERROR("fd != listenfd_.getfd()???!!!");
                return ;
            }
        }
    }
    for(int i=0;i<reactor_size_;++i)
    {
        reactors_[i].stop();
        threads[i].join();
    }
}
bool MainReactor::init()
{
    if(!createListenSocket() || !epoll_.addFd(listenfd_.getfd()))
        return false;

    return true;
}

bool MainReactor::createListenSocket()
{
    UniqueFd fd(::socket(AF_INET, SOCK_STREAM, 0));
    if (!fd.valid()) {
        LOG_ERROR("socket create failed");
        return false;
    }

    int reuse = 1;
    if (setsockopt(fd.getfd(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        LOG_ERROR("setsockopt reuseaddr failed");
        return false;
    }

    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port_);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd.getfd(), reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) == -1 ||
        listen(fd.getfd(), SOMAXCONN) == -1) {
        LOG_ERROR("bind or listen failed");
        return false;
    }

    if (!SocketUtil::setSocketNonblockNoDelay(fd.getfd())) {
        LOG_ERROR("set listen socket nonblock nodelay failed");
        return false;
    }

    listenfd_ = std::move(fd);
    return true;
}

void MainReactor::handleAccept()
{
    std::vector<std::vector<UniqueFd>>fds(reactor_size_);
    int accept=0;
    constexpr int max_size = 100;
    int index=cur_;
    while (true) 
    {
        UniqueFd clientFd(::accept(listenfd_.getfd(), nullptr, nullptr));
        if (!clientFd.valid()) 
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
            LOG_ERROR("accept failed");
            return;
        }

        if (!SocketUtil::setSocketNonblockNoDelay(clientFd.getfd())) 
        {
            LOG_ERROR("set client socket nonblock failed");
            continue;
        }
        LOG_INFO("new http client fd=" + std::to_string(clientFd.getfd()));
        ++accept;
        fds[index].emplace_back(std::move(clientFd));
        index = (index+1)%reactor_size_;
        if(accept == max_size)
        {
            cur_=index;
            enqueueNewFd(fds);
            accept = 0;
            //处理那些fd，然后清空桶
            //fds.assign(reactor_size_,{});
            std::vector<std::vector<UniqueFd>>temp(reactor_size_);
            fds.swap(temp);
        }
    }
    if(accept>0)
    {
        cur_=index;
        enqueueNewFd(fds);
    }
}

void MainReactor::handleExit()
{
    std::string cmd;
    std::getline(std::cin,cmd);
    if(cmd == "quit")
        running_=false;
}