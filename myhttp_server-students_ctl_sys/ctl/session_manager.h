#pragma once
#include <string>
#include <unordered_map>
#include <mutex>

class SessionManager{
    
public:
    std::string createSession(const std::string& username);
    bool validateSession(const std::string& sessionId,std::string& username);
    void removeSession(const std::string& sessionId);

private:
    std::string generateSessionId();

private:
    std::unordered_map<std::string,std::string>sessions_;
    std::mutex mutex_;

};