#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include "http_connection.h"
#include "http_request.h"
#include "database.h"

class Epoll;
namespace route {
    using Handler = std::function<void(HttpConnection&, const HttpRequest&, Database&)>;

    std::string makeRouteKey(const std::string& method, const std::string& path);
    std::unordered_map<std::string, Handler>& getRouteTable();
    bool dispatch(HttpConnection& client, const HttpRequest& req, Database& db);

    void handleApiHello(HttpConnection& client, const HttpRequest& req, Database& db);
    void handleApiEcho(HttpConnection& client, const HttpRequest& req, Database& db);
    void handleApiRegister(HttpConnection& client, const HttpRequest& req, Database& db);
    void handleApiLogin(HttpConnection& client, const HttpRequest& req, Database& db);

    bool parseFormBody(const std::string& body, std::unordered_map<std::string, std::string>& form);
    std::string simpleHash(const std::string& input);
    std::string escapeJson(const std::string& s);
    // void enqueueResponse(HttpConnection& client,std::string&& msg);
    
}