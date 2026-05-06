#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include "../ctl/http_connection.h"
#include "http_request.h"
#include "../db/database.h"

namespace route {
    using Handler = std::function<void(HttpConnection&, const HttpRequest&, Database&)>;

    std::string makeRouteKey(const std::string& method, const std::string& path);
    std::unordered_map<std::string, Handler>& getRouteTable();

    //查询是否访问接口，判断并调用接口函数处理业务
    bool dispatch(HttpConnection& client, const HttpRequest& req, Database& db);

    void handleApiRegister(HttpConnection& client, const HttpRequest& req, Database& db);
    void handleApiLogin(HttpConnection& client, const HttpRequest& req, Database& db);
    void handleApiInsert(HttpConnection& client,const HttpRequest&req,Database& db);
    void handleApiGetStudents(HttpConnection& client,const HttpRequest&req,Database& db);
    void handleApiUpdateStudent(HttpConnection& client,const HttpRequest&req,Database& db);
    void handleApiDeleteStudent(HttpConnection& client,const HttpRequest&req,Database& db);
    void handleApiStudentScore(HttpConnection& client,const HttpRequest&req,Database& db);
    void handleApiInsertCourse(HttpConnection& client,const HttpRequest&req,Database& db);
    void handleApiUpdateCourse(HttpConnection& client,const HttpRequest&req,Database& db);
    void handleApideleteCourse(HttpConnection& client,const HttpRequest&req,Database& db);

    //查询时常常要输入查询参数，所以路径req.path通常为/api/...?...=...&...=...路径后面
    //跟了一堆参数，所以需要解析出路劲和查询参数
    bool parseQueryString(const std::string& path,std::string& purePath,std::unordered_map<std::string,std::string>&query);
    //解析请求体数据
    bool parseFormBody(const std::string& body, std::unordered_map<std::string, std::string>& form);
    std::string simpleHash(const std::string& input);

    //这两个函数用来做解码
    int hexValue(char c);
    std::string urlDecode(const std::string& s);

    //解析请求头里的Cookie
    std::string getSessionIdFromCookie(const HttpRequest& req);
    //查询是否登录,登录了就给username赋值
    bool requireLogin(HttpConnection& client,const HttpRequest& req,std::string& username);
    bool checkIndex(const HttpRequest& req);
}
