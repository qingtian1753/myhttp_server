#include "route_handlers.h"
#include "http_response.h"
#include "../util/Log.h"
#include "../ctl/session_manager.h"
#include <sstream>

//定义一个全局的SessionManager
namespace {
    SessionManager g_sessionManager;
}
namespace route {

std::string makeRouteKey(const std::string& method, const std::string& path) {
    return method + " " + path;
}

std::unordered_map<std::string, Handler>& getRouteTable()
{
    static std::unordered_map<std::string, Handler> table = {
    { makeRouteKey("POST", "/api/register"), handleApiRegister },
    { makeRouteKey("POST", "/api/login"), handleApiLogin },
    
    { makeRouteKey("GET", "/api/students"), handleApiGetStudents },
    { makeRouteKey("POST", "/api/students"), handleApiInsert },
    { makeRouteKey("PUT", "/api/students"), handleApiUpdateStudent },  
    { makeRouteKey("DELETE", "/api/students"), handleApiDeleteStudent },
    { makeRouteKey("GET","/api/students/score"),handleApiStudentScore},
    { makeRouteKey("POST","/api/students/score"),handleApiInsertCourse},
    { makeRouteKey("PUT","/api/students/score"),handleApiUpdateCourse},
    { makeRouteKey("DELETE","/api/students/score"),handleApideleteCourse},
    };
    return table;
}


bool dispatch(HttpConnection& client, const HttpRequest& req, Database& db)
{
    std::string purePath;
    std::unordered_map<std::string,std::string>query;
    parseQueryString(req.path,purePath,query);
    auto& table = getRouteTable();
    auto it = table.find(makeRouteKey(req.method, purePath));
    if (it == table.end()) {
        return false;
    }
    it->second(client, req, db);
    return true;
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
        std::string key = urlDecode(token.substr(0,pos));
        form[key] = urlDecode(token.substr(pos + 1));
    }
    return true;
}

bool parseQueryString(const std::string& path,std::string& purePath,std::unordered_map<std::string,std::string>&query)
{
    query.clear();
    std::size_t pos = path.find("?");
    if(pos == std::string::npos)
    {
        purePath = path;
        return true;
    }
    purePath = path.substr(0,pos);
    std::string queryStr = path.substr(pos+1);
    std::stringstream ss(queryStr);
    std::string token;

    while(std::getline(ss,token,'&'))
    {
        std::size_t equal = token.find('=');
        if(equal == std::string::npos) 
            continue;

        std::string key = urlDecode(token.substr(0,equal));
        query[key] = urlDecode(token.substr(equal+1));
    }
    return true;
}
//不将密码明文储存，使用标准库进行哈希
std::string simpleHash(const std::string& input)
{
    //std::hash是一个类模板，这里是构造了一个临时对象，传入字符串哈希后返回size_t类型的哈希值
    return std::to_string(std::hash<std::string>{}(input));
}

void handleApiRegister(HttpConnection& client, const HttpRequest& req, Database& db)
{
    std::unordered_map<std::string, std::string> form;
    if (!parseFormBody(req.body, form) || !form.count("username") || !form.count("password")) {
        client.keepAlive = false;
        client.enqueueResponse(HttpResponse::buildJsonResponse(400, "Bad Request", "{\"code\":400,\"msg\":\"无效请求\"}", false));
        return;
    }

    bool exists = false;
    if (!db.userExists(form["username"], exists)) {
        client.keepAlive = false;
        client.enqueueResponse(HttpResponse::buildJsonResponse(500, "Internal Server Error", "{\"code\":500,\"msg\":\"数据库咱不可访问,请稍后重试\"}", false));
        return;
    }
    if (exists) {
        client.enqueueResponse(HttpResponse::buildJsonResponse(400, "Bad Request", "{\"code\":400,\"msg\":\"用户名已存在\"}", client.keepAlive));
        return;
    }
    if (!db.createUser(form["username"], simpleHash(form["password"]))) {
        client.keepAlive = false;
        client.enqueueResponse(HttpResponse::buildJsonResponse(500, "Internal Server Error", "{\"code\":500,\"msg\":\"注册失败\"}", false));
        return;
    }
    client.enqueueResponse(HttpResponse::buildJsonResponse(200, "OK", "{\"code\":0,\"msg\":\"注册成功\"}", client.keepAlive));
}

void handleApiLogin(HttpConnection& client, const HttpRequest& req, Database& db)
{
    std::unordered_map<std::string, std::string> form;
    //count函数返回与给定键值匹配的键个数，因为map只允许一个键因此返回值只会是1或0
    if (!parseFormBody(req.body, form) || !form.count("username") || !form.count("password")) {
        client.keepAlive = false;
        client.enqueueResponse(HttpResponse::buildJsonResponse(400, "Bad Request", "{\"code\":1,\"msg\":\"无效请求\"}", false));
        return;
    }
    bool exist = false;
    if(!db.userExists(form["username"],exist))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse
            (404, "Not Found", 
            "{\"code\":1,\"msg\":\"数据库暂不可用\"}", 
            client.keepAlive));
        return ;
    }
    if(!exist)
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse
            (404, "Not Found", 
            "{\"code\":1,\"msg\":\"用户名不存在!\"}", 
            client.keepAlive));
        return;      
    }
    std::string storedHash;
    if (!db.getPasswordHash(form["username"], storedHash)) {
        client.enqueueResponse(HttpResponse::buildJsonResponse
            (404, "Not Found", 
            "{\"code\":1,\"msg\":\"数据库暂不可用\"}", 
            client.keepAlive));
        return;
    }
    if (storedHash != simpleHash(form["password"])) {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            401, "Unauthorized", 
            "{\"code\":1,\"msg\":\"密码错误\"}", 
            client.keepAlive));
        return;
    }
    //登录成功，创建cookie，添加会话id
    std::string sessionId = g_sessionManager.createSession(form["username"]);
    std::string cookie = "session_id="+sessionId+";Path=/;HttpOnly";//path=/表示任何路径都可以携带cookie访问
    //HttpOnly：禁止客户端通过JavaScript脚本进行读取和修改cookie，保障安全
    client.enqueueResponse(HttpResponse::buildJsonResponseWithCookie(200, "OK", "{\"code\":0,\"msg\":\"login success\"}",cookie, client.keepAlive));

}

void handleApiInsert(HttpConnection& client,const HttpRequest&req,Database& db)
{
    std::string username;
    if(!requireLogin(client,req,username))
        return ;
    std::unordered_map<std::string,std::string>form;
    if(!parseFormBody(req.body,form))
    {
        client.keepAlive = false;
        client.enqueueResponse(HttpResponse::buildJsonResponse(400, "Bad Request", "{\"code\":1,\"msg\":\"invalid request\"}", false));
        return;
    }
    if(!form.count("id") || !form.count("name") || !form.count("class_name")
       || !form.count("age") || !form.count("gender"))
       {
            client.enqueueResponse(HttpResponse::buildJsonResponse(400, "Bad Request", "{\"code\":1,\"msg\":\"invalid request\"}", false));
            return ;
       }
    int id;
    std::string name;
    std::string class_name;
    int age;
    std::string gender;
    
    try{
        id = std::stoi(form["id"]);
        age = std::stoi(form["age"]);
    }catch(...)
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request", 
            "{\"code\":1,\"msg\":\"无效请求!\"}",
             false));
        return ;
    }
    name = form["name"];
    class_name = form["class_name"];
    gender = form["gender"];

    bool exist = false;
    if(!db.studentExists(username,id,exist))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(400, "Bad Request", "{\"code\":1,\"msg\":\"数据库暂不可用!\"}", false));
        return;
    }

    if(exist)
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(400, "Bad Request", "{\"code\":1,\"msg\":\"该学生已经存在!\"}", false));
        return;
    }

    if (!db.insertStudent(username,id,name,class_name,age,gender)) 
    {
        client.keepAlive = false;
        client.enqueueResponse(HttpResponse::buildJsonResponse(500, "Internal Server Error", "{\"code\":1,\"msg\":\"添加失败,数据库暂不可用!\"}", false));
        return;
    }
    client.enqueueResponse(HttpResponse::buildJsonResponse(200, "OK", "{\"code\":0,\"msg\":\"添加成功!\"}", client.keepAlive));
}
//根据前端提交表单来决定怎么查，看路径后会不会跟参数
void handleApiGetStudents(HttpConnection& client,const HttpRequest&req,Database& db)
{
    //先判断是否登录
    std::string username;
    if(!requireLogin(client,req,username))
        return ;

    //取出查询参数
    std::string purePath;
    std::unordered_map<std::string,std::string> query;
    parseQueryString(req.path,purePath,query);
    //支持分页查询，page是当前页数，size是一页最多包含多少条数据
    int page = 1;
    int size = 10;
    try{
        if(query.count("page"))
            page = std::stoi(query["page"]);
        if(query.count("size"))
            size = std::stoi(query["size"]);
    }catch(...){
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400,"Bad Request",
            R"({"code" : 1, "msg":"invalid page or size"})",
            client.keepAlive
        ));
        return ;
    }
    if(page <= 0 ) page = 1;
    if(size <= 0 ) size = 10;
    if(size > 100) size = 100;

    std::string idFilter = query.count("id") ? query["id"]:"";
    std::string nameFilter = query.count("name") ? query["name"]:"";
    std::string classFilter = query.count("class_name") ? query["class_name"]:"";
    int trueId = -1;
    //先确保id是合理的数字避免"123abc","abc","00012"
    if(!idFilter.empty())
    {
        try{
            size_t pos = 0;
            trueId = std::stoi(idFilter,&pos);
            if(pos!=idFilter.size())
            {
                client.enqueueResponse(HttpResponse::buildJsonResponse(
                400,"Bad Request",
                R"({"code" : 1, "msg":"无效的学号!"})",
                client.keepAlive
                ));     
                return;
            }
        } catch(...){
            client.enqueueResponse(HttpResponse::buildJsonResponse(
                400,"Bad Request",
                R"({"code" : 1, "msg":"无效的学号!"})",
                client.keepAlive
            ));     
            return;
        }
    }
    if(trueId > 0)
        idFilter = std::to_string(trueId);

    std::string jsonData;
    if(!db.listStudents(username,jsonData,page,size,idFilter,nameFilter,classFilter))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(500,"list failed!","{\"code\":1,\"msg\":\"数据库暂不可用!\"}",client.keepAlive));
    }
    else
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(200,"successed!",jsonData,client.keepAlive));
    }
}

void handleApiUpdateStudent(HttpConnection& client,const HttpRequest&req,Database& db)
{
    std::string username;
    if(!requireLogin(client,req,username))
        return ;
    std::unordered_map<std::string, std::string> form;
    if (!parseFormBody(req.body, form)) {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效请求!\"}",
            client.keepAlive));
        return;
    }

    if (!form.count("old_id") || !form.count("id") || !form.count("name") || !form.count("class_name")
        || !form.count("age") || !form.count("gender")) {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效请求!\"}",
            client.keepAlive));
        return;
    }

    int id = 0;
    int age = 0;
    int old_id=0;
    try {
        old_id = std::stoi(form["old_id"]);
        id = std::stoi(form["id"]);
        age = std::stoi(form["age"]);
    } catch (...) {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效请求!\"}",
            client.keepAlive));
        return;
    }

    //先判断旧学号是否存在
    bool old_exist = false;
    if (!db.studentExists(username,old_id, old_exist)) {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            500, "Internal Server Error",
            "{\"code\":1,\"msg\":\"数据库暂不可用\"}",
            client.keepAlive));
        return;
    }

    if (!old_exist) {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            404, "Not Found",
            "{\"code\":1,\"msg\":\"the old_id is not exist!\"}",
            client.keepAlive));
        return;
    }
    //如果旧学号存在再来判断新学号是否存在
    //如果旧学号等于新学号，没问题，直接改自己，如果旧学号不等于新学号，那么就有问题了
    //得先判断新学号是否存在，不然改旧学号的人直接把新学号那人改了就不行了
    bool new_exist = false;
    if(!db.studentExists(username,id,new_exist))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
        404, "Not Found",
        "{\"code\":1,\"msg\":\"the new_id query failed!\"}",
        client.keepAlive));
        return ;
    }
    if(new_exist && old_id != id)
    {
        //新学号存在且旧与新不相等，那么不允许修改
        client.enqueueResponse(HttpResponse::buildJsonResponse(
        404, "Not Found",
        "{\"code\":1,\"msg\":\"the new_id is already exist!\"}",
        client.keepAlive));
        return ;
    }

    if (!db.updateStudent(username,id, form["name"], form["class_name"], age, form["gender"],old_id)) {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            500, "Internal Server Error",
            "{\"code\":1,\"msg\":\"更新失败!\"}",
            client.keepAlive));
        return;
    }

    client.enqueueResponse(HttpResponse::buildJsonResponse(
        200, "OK",
        "{\"code\":0,\"msg\":\"更新成功!\"}",
        client.keepAlive));
}
void handleApiDeleteStudent(HttpConnection& client,const HttpRequest&req,Database& db)
{
    std::string username;
    if(!requireLogin(client,req,username))
        return ;
    std::unordered_map<std::string,std::string>form;
    if(!parseFormBody(req.body,form) || !form.count("id"))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效请求\"}",
            client.keepAlive));
        return;
    }
    bool exist = false;
    int id = 0;
    try{
        id =std::stoi(form["id"]);
    }catch(...)
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
        400, "Bad Request",
        "{\"code\":1,\"msg\":\"invalid id\"}",
        client.keepAlive));
        return ;
    }
    if(!db.studentExists(username,id,exist))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
        400, "Bad Request",
        "{\"code\":1,\"msg\":\"query failed!\"}",
        client.keepAlive));
        return ;
    }
    if(!exist)
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
        400, "Bad Request",
        "{\"code\":1,\"msg\":\"the student is not exist!\"}",
        client.keepAlive));
        return ;
    }
    if(!db.deleteStudent(username,id))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
        400, "Bad Request",
        "{\"code\":1,\"msg\":\"delete student failed!\"}",
        client.keepAlive));
        return ;     
    }
    client.enqueueResponse(HttpResponse::buildJsonResponse(
    200, "OK",
    "{\"code\":0,\"msg\":\"OK!\"}",
    client.keepAlive));
}
//查询成绩接口，前端提交id
void handleApiStudentScore(HttpConnection& client,const HttpRequest&req,Database& db)
{
    std::string username;
    if(!requireLogin(client,req,username))
        return ;

    std::unordered_map<std::string,std::string>query;
    std::string purePath;
    if(!parseQueryString(req.path,purePath,query) || query.count("id") == 0)
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效请求\"}",
            client.keepAlive));
        return ;
    }
    int id = -1;
    try{
            size_t pos = 0;
            id = std::stoi(query["id"],&pos);
            if(pos!=query["id"].size())
            {
                client.enqueueResponse(HttpResponse::buildJsonResponse(
                400,"Bad Request",
                R"({"code" : 1, "msg":"无效的学号!"})",
                client.keepAlive
                ));     
                return;
            }
    }catch(...){
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效的学号\"}",
            client.keepAlive));
        return ;
    }
    if(id<1)
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效的学号\"}",
            client.keepAlive));
        return ;        
    }
    std::string course_nameFilter = (query.count("course_name") == 0 ? "":query["course_name"]);
    std::string jsonData;
    if(!db.getStudentScore(jsonData,username,id,course_nameFilter))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"数据库暂不可用，请稍后访问\"}",
            client.keepAlive));
        return;
    }
    client.enqueueResponse(HttpResponse::buildJsonResponse(
            200, "OK",
            jsonData,
            client.keepAlive));
}

void handleApiInsertCourse(HttpConnection& client,const HttpRequest&req,Database& db)
{
    std::string username;
    if(!requireLogin(client,req,username))
        return ;
    std::unordered_map<std::string,std::string>query;
    if(!parseFormBody(req.body,query) || !query.count("id") || !query.count("course_name") || !query.count("course_score"))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效请求\"}",
            client.keepAlive));
        return ;
    }
    int id = -1;
    int course_score = -1;
    try{
        id = std::stoi(query["id"]);
        course_score = std::stoi(query["course_score"]);
    }catch(...){
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效的数据\"}",
            client.keepAlive));
        return ;
    }
    if(id<1 || course_score<0)
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效的数据\"}",
            client.keepAlive));
        return ;        
    }
    bool course_exist =false;
    //先判断能不能插入课程信息
    if(!db.courseExist(username,id,query["course_name"],course_exist))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"数据库暂不可用，请稍后访问\"}",
            client.keepAlive));
        return;        
    }
    if(course_exist)
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"该课程成绩已存在\"}",
            client.keepAlive));
        return;       
    }
    std::string course_name = (query.count("course_name") == 0 ? "":query["course_name"]);
    if(!db.insertStudentScore(username,id,course_name,course_score))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"数据库暂不可用，请稍后访问\"}",
            client.keepAlive));
        return;
    }
    client.enqueueResponse(HttpResponse::buildJsonResponse(
            200, "OK",
            "{\"code\":0,\"msg\":\"添加成功\"}",
            client.keepAlive));
    
}
void handleApiUpdateCourse(HttpConnection& client,const HttpRequest&req,Database& db)
{
    std::string username;
    if(!requireLogin(client,req,username))
        return ;
    std::unordered_map<std::string,std::string>query;
    if(!parseFormBody(req.body,query) || !query.count("id") 
    || !query.count("course_name") || !query.count("course_score")
    || !query.count("old_course_name"))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效请求\"}",
            client.keepAlive));
        return ;
    }
    int id = -1;
    int course_score = -1;
    try{
        id = std::stoi(query["id"]);
        course_score = std::stoi(query["course_score"]);
    }catch(...){
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效的数据\"}",
            client.keepAlive));
        return ;
    }
    if(id<1 || course_score<0)
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效的数据\"}",
            client.keepAlive));
        return ;        
    }
    if(!db.updateStudentScore(username,id,query["course_name"],course_score,query["old_course_name"]))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"数据库暂不可用，请稍后访问\"}",
            client.keepAlive));
        return;
    }
    client.enqueueResponse(HttpResponse::buildJsonResponse(
            200, "OK",
            "{\"code\":0,\"msg\":\"更新成功\"}",
            client.keepAlive));   
}
void handleApideleteCourse(HttpConnection& client,const HttpRequest&req,Database& db)
{
    std::string username;
    if(!requireLogin(client,req,username))
        return ;
    std::unordered_map<std::string,std::string>query;
    if(!parseFormBody(req.body,query) || !query.count("id") 
    || !query.count("course_name"))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效请求\"}",
            client.keepAlive));
        return ;
    }
    int id = -1;
    try{
        id = std::stoi(query["id"]);
    }catch(...){
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效的数据\"}",
            client.keepAlive));
        return ;
    }
    if(id<1)
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"无效的数据\"}",
            client.keepAlive));
        return ;        
    }
    if(!db.deleteStudentScore(username,id,query["course_name"]))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            400, "Bad Request",
            "{\"code\":1,\"msg\":\"数据库暂不可用，请稍后访问\"}",
            client.keepAlive));
        return;
    }
    client.enqueueResponse(HttpResponse::buildJsonResponse(
            200, "OK",
            "{\"code\":0,\"msg\":\"删除成功\"}",
            client.keepAlive));    
}
//十六进制字符转数字
int hexValue(char c)
{
    if(c >='0' && c <= '9') return (c-'0');
    if(c >='a' && c <= 'f') return 10+(c-'a');
    if(c >='A' && c <= 'F') return 10+(c-'A');
    return -1;
}

//浏览器遇到中文会按UTF-8编码，" "会编成+，中文会编成十六进制，
//采用%xx%xx%xx的格式，所以要取出xx还原为真实字节数
std::string urlDecode(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for(size_t i=0; i<s.size();++i)
    {
        if(s[i] == '+')
        {
            out.push_back(' ');
        } else if(s[i]=='%' && i+2 < s.size())
        {
            int hi = hexValue(s[i+1]);
            int lo = hexValue(s[i+2]);
            if(hi  != -1 && lo != -1)
            {
                out.push_back(static_cast<char>(hi*16 + lo));
                i+=2;
            } else {
                out.push_back(s[i]);
            }
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}


std::string getSessionIdFromCookie(const HttpRequest& req)
{
    //浏览器发送的Cookie:里会有Session_id=...;还有其他的东西
    //例如theme=dark;...所以需要找分号，如果没找到分号说明只有Session_id
    auto it = req.headers.find("Cookie");
    if(it == req.headers.end())
        return "";//没有cookie说明没登录

    const std::string& cookieStr = it->second;
    std::string key = "session_id=";
    std::size_t pos = cookieStr.find(key);
    //没有session_id，直接return
    if(pos == std::string::npos)
        return "";
        
    pos += key.size();
    //现在pos是SessionId的起始下标了

    //找第一个分号，
    std::size_t end = cookieStr.find(";",pos);
    if(end == std::string::npos)
        end = cookieStr.size();
    return cookieStr.substr(pos,end-pos);
}

bool requireLogin(HttpConnection& client,const HttpRequest& req,std::string& username)
{
    std::string sessionId = getSessionIdFromCookie(req);
    //没有cookie，直接返回false
    if(sessionId.empty())
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            401, "Unauthorized",
            "{\"code\":401,\"msg\":\"请先登录\"}",
            client.keepAlive
        ));
        return false;        
    }
    //sessionId无效，直接返回false
    if(!g_sessionManager.validateSession(sessionId,username))
    {
        client.enqueueResponse(HttpResponse::buildJsonResponse(
            401, "Unauthorized",
            "{\"code\":401,\"msg\":\"请重新登录\"}",
            client.keepAlive
        ));
        return false;
    }
    return true;
}
//如果这人没登录，返回false，不允许访问index.html，需要跳转到登录界面
bool checkIndex(const HttpRequest& req)
{
    std::string sessionId = route::getSessionIdFromCookie(req);
    std::string username;
    
    if(sessionId.empty() || !g_sessionManager.validateSession(sessionId,username))
            return false;

    return true;
}


}
