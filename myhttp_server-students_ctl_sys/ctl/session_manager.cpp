#include "session_manager.h"
#include <random>
#include <sstream>

std::string SessionManager::generateSessionId()
{
    //定义一个静态字符集，待会做会话id从这里面抽
    static const char charset[]=
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    //随机生成器种子
    std::random_device rd;
    //一个随机数算法，加入了random_device作为种子，这两个在一起几乎是真随机了·
    std::mt19937 gen(rd());
    //均匀分布整数：产生一个范围在[0,sizeof(charset)-2]的整数
    //因为charset时char[]，后面编译器会加上\0,所以是-2
    std::uniform_int_distribution<> dist(0,sizeof(charset)-2);
    std::string id;
    //能提前预留就提前预留，避免扩容
    id.reserve(32);
    for(int i=0;i<32;++i)
    {
        //在产生整数时才将dist与引擎gen绑定在一起，dist底层是重载了()运算符的，接收引擎做参数
        //生成一个范围在定义时就确定了的整数，并且保证严格均匀分布
        id.push_back(charset[dist(gen)]);
    }
    return id;
}
//为这个用户创建一个sessionId
std::string SessionManager::createSession(const std::string& username)
{
    std::lock_guard<std::mutex>lock(mutex_);
    std::string sessionId = generateSessionId();
    sessions_[sessionId] = username;
    return sessionId;
}

//检验这个sessionId是否有效，并取出对应用户名
//根据用户名来管理数据
bool SessionManager::validateSession(const std::string& sessionId,std::string& username)
{
    std::lock_guard<std::mutex>lock(mutex_);
    auto it = sessions_.find(sessionId);
    if(it == sessions_.end())
        return false;
    
    username = it->second;
    return true;
}

void SessionManager::removeSession(const std::string& sessionId)
{
    std::lock_guard<std::mutex>lock(mutex_);
    sessions_.erase(sessionId);
}