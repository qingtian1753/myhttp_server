#include "route_handlers.h"
#include "http_response.h"
#include "Log.h"
#include <sstream>
#include "epoll.h"

namespace route {

std::string makeRouteKey(const std::string& method, const std::string& path) {
    return method + " " + path;
}

std::unordered_map<std::string, Handler>& getRouteTable()
{
    static std::unordered_map<std::string, Handler> table = {
        { makeRouteKey("GET", "/api/hello"), handleApiHello },
        { makeRouteKey("POST", "/api/echo"), handleApiEcho },
        { makeRouteKey("POST", "/api/register"), handleApiRegister },
        { makeRouteKey("POST", "/api/login"), handleApiLogin },
    };
    return table;
}

//判断是否是访问接口，如果是则处理，否则则返回false
bool dispatch(HttpConnection& client, const HttpRequest& req, Database& db)
{
    auto& table = getRouteTable();
    auto it = table.find(makeRouteKey(req.method, req.path));
    if (it == table.end()) {
        return false;
    }
    it->second(client, req, db);
    return true;
}

//正确地将字符串转化为json字符串
std::string escapeJson(const std::string& s)
{
    std::string out;
    for (char ch : s) {
        switch (ch) {
            //如果是",那么就加入\\\",因为json字符串里不允许出现双引号，
            //所以\\转义为\，再加上\"转义为"
            case '"': out += "\\\""; break;

            /*如果是'\\',那么要返回'\\'就必须再加一个'\\'，因为一个'\\'会转义为'\'
            例如echo那段json,客户端发了\\hello,那么为了返回\\hello，需要将源字符串的
            \\转为\\\\才行 */
            case '\\': out += "\\\\"; break;

            //常见的转义字符不必多说，肯定要转义的，但是还有一些不常见，我暂时不写了，这样已经够用了
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += ch; break;
        }
    }
    return out;
}

bool parseFormBody(const std::string& body, std::unordered_map<std::string, std::string>& form)
{
    std::stringstream ss(body);
    std::string token;
    //getline 默认使用'\n'作为分隔符的，传入'&'以'&'作为分隔符
    //客户端是采用username=...&password=...来完成注册的
    while (std::getline(ss, token, '&')) {
        size_t pos = token.find('=');
        if (pos == std::string::npos) return false;
        form[token.substr(0, pos)] = token.substr(pos + 1);
    } 
    return true;
}

//不将密码明文储存，使用标准库进行哈希
std::string simpleHash(const std::string& input)
{
    //std::hash是一个类模板，这里是构造了一个临时对象，传入字符串哈希后返回size_t类型的哈希值
    return std::to_string(std::hash<std::string>{}(input));
}

void handleApiHello(HttpConnection& client, const HttpRequest&, Database&)
{
    client.enqueueResponse(HttpResponse::buildJsonResponse(200, "OK", "{\"message\":\"hello\"}", client.keepAlive));
}

void handleApiEcho(HttpConnection& client, const HttpRequest& req, Database&)
{
    std::string body = "{\"echo\":\"" + escapeJson(req.body) + "\"}";
    client.enqueueResponse(HttpResponse::buildJsonResponse(200, "OK", body, client.keepAlive));
}

void handleApiRegister(HttpConnection& client, const HttpRequest& req, Database& db)
{
    std::unordered_map<std::string, std::string> form;
    if (!parseFormBody(req.body, form) || !form.count("username") || !form.count("password")) {
        client.keepAlive = false;
        client.enqueueResponse(HttpResponse::buildJsonResponse(400, "Bad Request", "{\"code\":400,\"message\":\"invalid request\"}", false));
        return;
    }

    bool exists = false;
    if (!db.userExists(form["username"], exists)) {
        client.keepAlive = false;
        client.enqueueResponse(HttpResponse::buildJsonResponse(500, "Internal Server Error", "{\"code\":500,\"message\":\"database error\"}", false));
        return;
    }
    if (exists) {
        client.enqueueResponse(HttpResponse::buildJsonResponse(400, "Bad Request", "{\"code\":400,\"message\":\"user already exists\"}", client.keepAlive));
        return;
    }
    if (!db.createUser(form["username"], simpleHash(form["password"]))) {
        client.keepAlive = false;
        client.enqueueResponse(HttpResponse::buildJsonResponse(500, "Internal Server Error", "{\"code\":500,\"message\":\"create user failed\"}", false));
        return;
    }
    client.enqueueResponse(HttpResponse::buildJsonResponse(200, "OK", "{\"code\":200,\"message\":\"register success\"}", client.keepAlive));
}

void handleApiLogin(HttpConnection& client, const HttpRequest& req, Database& db)
{
    std::unordered_map<std::string, std::string> form;
    //count函数返回与给定键值匹配的键个数，因为map只允许一个键因此返回值只会是1或0
    if (!parseFormBody(req.body, form) || !form.count("username") || !form.count("password")) {
        client.keepAlive = false;
        client.enqueueResponse(HttpResponse::buildJsonResponse(400, "Bad Request", "{\"code\":400,\"message\":\"invalid request\"}", false));
        return;
    }

    std::string storedHash;
    if (!db.getPasswordHash(form["username"], storedHash)) {
        client.enqueueResponse(HttpResponse::buildJsonResponse(404, "Not Found", "{\"code\":404,\"message\":\"user not found\"}", client.keepAlive));
        return;
    }
    if (storedHash != simpleHash(form["password"])) {
        client.enqueueResponse(HttpResponse::buildJsonResponse(401, "Unauthorized", "{\"code\":401,\"message\":\"wrong password\"}", client.keepAlive));
        return;
    }
    client.enqueueResponse(HttpResponse::buildJsonResponse(200, "OK", "{\"code\":200,\"message\":\"login success\"}", client.keepAlive));
}

}
