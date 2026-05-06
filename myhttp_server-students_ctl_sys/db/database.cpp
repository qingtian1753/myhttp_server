#include "database.h"
#include "../util/Log.h"
#include "../util/socket_util.h"
#include <cstring>
#include <sstream>

Database::Database() : conn_(nullptr) {}
Database::~Database() { close(); }

bool Database::connect(const std::string& host,
                       const std::string& user,
                       const std::string& password,
                       const std::string& dbname,
                       unsigned int port)
{
    conn_ = mysql_init(nullptr);
    if (!conn_) {
        LOG_ERROR("mysql_init failed");
        return false;
    }
    if (!mysql_real_connect(conn_, host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), port, nullptr, 0)) {
        LOG_ERROR(std::string("mysql_real_connect failed: ") + mysql_error(conn_));
        close();
        return false;
    }
    return true;
}

void Database::close()
{
    if (conn_) {
        mysql_close(conn_);
        conn_ = nullptr;
    }
}

bool Database::connected() const { return conn_ != nullptr; }

bool Database::userExists(const std::string& username, bool& exists)
{
    exists = false;
    if (!conn_) return false;

    const char* sql = "SELECT id FROM users WHERE username = ? LIMIT 1";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, sql, std::strlen(sql)) != 0) {
        LOG_ERROR(std::string("mysql_stmt_prepare failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND param[1]{};
    unsigned long usernameLen = static_cast<unsigned long>(username.size());
    param[0].buffer_type = MYSQL_TYPE_STRING;
    param[0].buffer = const_cast<char*>(username.c_str());
    param[0].buffer_length = usernameLen;
    param[0].length = &usernameLen;

    if (mysql_stmt_bind_param(stmt, param) != 0) {
        LOG_ERROR(std::string("mysql_stmt_bind_param failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    if (mysql_stmt_execute(stmt) != 0) {
        LOG_ERROR(std::string("mysql_stmt_execute failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    int id = 0;
    unsigned long idLen = 0;
    bool isNull = false;
    MYSQL_BIND result[1]{};
    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = (char*)&id;
    result[0].buffer_length = sizeof(id);
    result[0].length = &idLen;
    result[0].is_null = &isNull;

    if (mysql_stmt_bind_result(stmt, result) != 0) {
        LOG_ERROR(std::string("mysql_stmt_bind_result failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    if (mysql_stmt_store_result(stmt) != 0) {
        LOG_ERROR(std::string("mysql_stmt_store_result failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    int fetchResult = mysql_stmt_fetch(stmt);
    exists = (fetchResult == 0);
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}

bool Database::createUser(const std::string& username, const std::string& passwordHash)
{
    if (!conn_) return false;
    //编写预处理语句
    const char* sql = "INSERT INTO users (username, password_hash) VALUES (?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, sql, std::strlen(sql)) != 0) {
        LOG_ERROR(std::string("mysql_stmt_prepare failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND param[2]{};
    unsigned long usernameLen = static_cast<unsigned long>(username.size());
    unsigned long hashLen = static_cast<unsigned long>(passwordHash.size());

    param[0].buffer_type = MYSQL_TYPE_STRING;
    param[0].buffer = const_cast<char*>(username.c_str());
    param[0].buffer_length = usernameLen;
    param[0].length = &usernameLen;

    param[1].buffer_type = MYSQL_TYPE_STRING;
    param[1].buffer = const_cast<char*>(passwordHash.c_str());
    param[1].buffer_length = hashLen;
    param[1].length = &hashLen;

    if (mysql_stmt_bind_param(stmt, param) != 0) {
        LOG_ERROR(std::string("mysql_stmt_bind_param failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    if (mysql_stmt_execute(stmt) != 0) {
        LOG_ERROR(std::string("mysql_stmt_execute failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    mysql_stmt_close(stmt);
    return true;
}

//取出哈希后的密码(也就是存放在数据库里的密码);
bool Database::getPasswordHash(const std::string& username, std::string& passwordHash)
{
    passwordHash.clear();
    if (!conn_) return false;

    const char* sql = "SELECT password_hash FROM users WHERE username = ? LIMIT 1";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, sql, std::strlen(sql)) != 0) {
        LOG_ERROR(std::string("mysql_stmt_prepare failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND param[1]{};
    unsigned long usernameLen = static_cast<unsigned long>(username.size());
    param[0].buffer_type = MYSQL_TYPE_STRING;
    param[0].buffer = const_cast<char*>(username.c_str());
    param[0].buffer_length = usernameLen;
    param[0].length = &usernameLen;

    if (mysql_stmt_bind_param(stmt, param) != 0) {
        LOG_ERROR(std::string("mysql_stmt_bind_param failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    if (mysql_stmt_execute(stmt) != 0) {
        LOG_ERROR(std::string("mysql_stmt_execute failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    char hashBuf[256]{};
    unsigned long hashLen = 0;
    //isNull用来判断数据库查询出来的数据是否为空
    bool isNull = 0;
    MYSQL_BIND result[1]{};
    result[0].buffer_type = MYSQL_TYPE_STRING;
    result[0].buffer = hashBuf;
    result[0].buffer_length = sizeof(hashBuf);
    result[0].length = &hashLen;
    result[0].is_null = &isNull;

    if (mysql_stmt_bind_result(stmt, result) != 0) {
        LOG_ERROR(std::string("mysql_stmt_bind_result failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    if (mysql_stmt_store_result(stmt) != 0) {
        LOG_ERROR(std::string("mysql_stmt_store_result failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    //取出这一行,密码存放在hashBuf里
    int fetchResult = mysql_stmt_fetch(stmt);
    //成功读到一行,并且这一行不为空
    //mysql行是从0开始的
    if (fetchResult == 0 && !isNull) {
        //assign:重新给这个字符串赋值,将hashBuf的前hashLen个字符赋值给passwordHash
        passwordHash.assign(hashBuf, hashLen);
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        return true;
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return false;
}

bool Database::insertStudent(std::string& username,const int& id,const std::string& name,const std::string& class_name,const int& age,const std::string& gender)
{
    if(!conn_)
        return false;

    const char* sql = "insert into students (id,name,class_name,age,gender,owner_name) values(?,?,?,?,?,?)";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if(!stmt)
        return false;
        
    if(mysql_stmt_prepare(stmt,sql,std::strlen(sql)) !=0)
    {
        LOG_ERROR(std::string("mysql_stmt_prepare failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;        
    }
    MYSQL_BIND param[6]{};
    param[0].buffer = (char*)&id;
    param[0].buffer_type = MYSQL_TYPE_LONG;

    unsigned long name_len = name.length();
    param[1].buffer = const_cast<char*>(name.c_str());
    param[1].buffer_type = MYSQL_TYPE_STRING;
    param[1].length = &name_len;

    unsigned long class_len = class_name.length();
    param[2].buffer = const_cast<char*>(class_name.c_str());
    param[2].buffer_type = MYSQL_TYPE_STRING;
    param[2].length = &class_len;

    param[3].buffer = (char*)&age;
    param[3].buffer_type = MYSQL_TYPE_LONG;

    unsigned long gender_len = gender.length();
    param[4].buffer = const_cast<char*>(gender.c_str());
    param[4].buffer_type = MYSQL_TYPE_STRING;
    param[4].length = &gender_len;

    unsigned long user_len = username.length();
    param[5].buffer = const_cast<char*>(username.c_str());
    param[5].buffer_type = MYSQL_TYPE_STRING;
    param[5].length = &user_len;

    if (mysql_stmt_bind_param(stmt, param) != 0) 
    {
        LOG_ERROR(std::string("mysql_stmt_bind_param failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) 
    {
        LOG_ERROR(std::string("mysql_stmt_execute failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

bool Database::studentExists(std::string&username,const int& id,bool& exist)
{
    exist = false;
    if(!conn_)
        return false;

    const char* sql = "select id from students where id = ? and owner_name = ?";

    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if(!stmt)
        return false;

    if(mysql_stmt_prepare(stmt,sql,std::strlen(sql))!=0)
    {
        LOG_ERROR(std::string("mysql_stmt_prepare failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    MYSQL_BIND param[2]{};
    param[0].buffer = (char*)&id;
    param[0].buffer_type = MYSQL_TYPE_LONG;

    unsigned long len = username.length();
    param[1].buffer = const_cast<char*>(username.c_str());
    param[1].length = &len;
    param[1].buffer_type = MYSQL_TYPE_STRING;

    if(mysql_stmt_bind_param(stmt,param)!= 0)
    {
        LOG_ERROR(std::string("mysql_stmt_bind_param failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;        
    }
    if(mysql_stmt_execute(stmt) !=0)
    {
        LOG_ERROR(std::string("mysql_stmt_execute failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    MYSQL_BIND result[1]{};
    int select_id = 0;
    result[0].buffer=(char*)&select_id;
    result[0].buffer_type=MYSQL_TYPE_LONG;

    if(mysql_stmt_bind_result(stmt,result) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_bind_result failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;     
    }

    //正常模式下取数据是逐行读取，效率不高，使用了这个之后就会一次性读完
    if(mysql_stmt_store_result(stmt) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_store_result failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;     
    }
    int line = mysql_stmt_fetch(stmt);
    exist = (line== 0);
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}


bool Database::updateStudent(std::string& username,const int& id,const std::string& name,const std::string& class_name,const int& age,const std::string& gender,const int& old_id)
{
    if(!conn_)
        return false;
    
    const char* sql = "update students set id = ?,name = ?,class_name = ?,age = ?,gender = ? where id = ? and owner_name = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if(!stmt)
        return false;

    if(mysql_stmt_prepare(stmt,sql,std::strlen(sql)) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_prepare failed!: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    
    MYSQL_BIND param[7]{};

    unsigned long name_len = static_cast<unsigned long>(name.size());
    unsigned long class_len = static_cast<unsigned long>(class_name.size());
    unsigned long gender_len = static_cast<unsigned long>(gender.size());
    unsigned long user_len = static_cast<unsigned long>(username.size());

    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = (char*)&id;

    param[1].buffer_type = MYSQL_TYPE_STRING;
    param[1].buffer = const_cast<char*>(name.c_str());
    param[1].length = &name_len;

    param[2].buffer_type = MYSQL_TYPE_STRING;
    param[2].buffer = const_cast<char*>(class_name.c_str());
    param[2].length = &class_len;

    param[3].buffer_type = MYSQL_TYPE_LONG;
    param[3].buffer = (char*)&age;

    param[4].buffer_type = MYSQL_TYPE_STRING;
    param[4].buffer = const_cast<char*>(gender.c_str());
    param[4].length = &gender_len;

    param[5].buffer_type = MYSQL_TYPE_LONG;
    param[5].buffer = (char*)&old_id;


    param[6].buffer_type = MYSQL_TYPE_STRING;
    param[6].buffer = const_cast<char*>(username.c_str());
    param[6].length = &user_len;

    if (mysql_stmt_bind_param(stmt, param) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_bind_param failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_execute failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}
bool Database::deleteStudent(std::string& username,const int& id)
{
    if(!conn_)
        return false;

    const char* sql = "delete from students where id = ? and owner_name = ? ";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if(!stmt)
        return false;
    if(mysql_stmt_prepare(stmt,sql,std::strlen(sql))!=0)
    {
        LOG_ERROR(std::string("mysql_stmt_prepare failed!: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    
    MYSQL_BIND param[2]{};
    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = (char*)&id;

    unsigned long len = username.length();
    param[1].buffer_type = MYSQL_TYPE_STRING;
    param[1].buffer = const_cast<char*>(username.c_str());
    param[1].length = &len;

    if(mysql_stmt_bind_param(stmt,param) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_bind_param failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if(mysql_stmt_execute(stmt) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_execute failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    mysql_stmt_close(stmt);
    return true;
}

//统一查询接口
bool Database::listStudents(std::string& username,std::string& jsonData,int page,int size,const std::string& idFilter,const std::string& nameFilter,const std::string& classFilter)
{
    if(!conn_)
        return false;

    //limit的起始索引
    int offset = (page-1)*size;

    //动态拼写SQL语句
    std::ostringstream where;
    where<<"where owner_name='";
    where<<username<<"' ";
    if(!idFilter.empty())
    {
        where<<"AND id = "<<idFilter<<" ";
    }
    //采用模糊匹配名字
    if(!nameFilter.empty())
    {
        //缓冲区只少要有2*str.size()+1这么大，所以用char数组就不太行了，
        //用string提前分配大小，后续要收回
        std::string escaped;
        escaped.resize(2*nameFilter.size()+1);
        //将名字转义，防sql注入，但不够安全
        unsigned long len = mysql_real_escape_string(conn_,escaped.data(),nameFilter.c_str(),nameFilter.size());
        escaped.resize(len);
        where<<"and name like '%"<<escaped<<"%' ";
    }
    if(!classFilter.empty())
    {
        std::string escaped;
        escaped.resize(2*classFilter.size()+1);
        unsigned long len = mysql_real_escape_string(conn_,escaped.data(),classFilter.c_str(),classFilter.size());
        escaped.resize(len);
        where<<"and class_name like '%"<<escaped<<"%' ";
    }
    std::string whereStr = where.str();

    //查符合条件的总数
    std::string countSql = "select count(*) from students "+whereStr;
    //执行
    if(mysql_query(conn_,countSql.c_str()) != 0)
    {
        LOG_ERROR(std::string("count query failed: ")+ mysql_error(conn_));
        return false;
    }
    //一次性获取所有查询结果
    MYSQL_RES* countRes = mysql_store_result(conn_);
    if(!countRes)
    {
        LOG_ERROR(std::string("mysql_store_result failed: ") + mysql_error(conn_));
        return false;
    }

    //MYSQL_ROW底层是char**
    MYSQL_ROW countRow = mysql_fetch_row(countRes);
    int total = 0;
    //先判断查到没有
    if(countRow && countRow[0])
    {
        total = std::stoi(countRow[0]);
    }
    mysql_free_result(countRes);


    //查询当页数据
    std::ostringstream dataSql;
    dataSql<<"select * from students "<<whereStr<<" limit "<<offset<<", "<<size;
    if(mysql_query(conn_,dataSql.str().c_str())!=0)
    {
        LOG_ERROR(std::string("mysql_query failed: ")+mysql_error(conn_));
        return false;
    }

    MYSQL_RES* res = mysql_store_result(conn_);
    if(!res)
    {
        LOG_ERROR(std::string("mysql_store_result failed: ")+mysql_error(conn_));
        return false;
    }

    //拼JSON
    std::ostringstream oss;
    oss << R"({"code":0,"msg":"success","data":{)";
    oss << R"("total":)" << total << ",";
    oss << R"("page":)"  << page << ",";
    oss << R"("size":)"  << size << ",";
    oss << R"("list":[)";

    MYSQL_ROW row;
    bool first = true;
    //循环读取所有数据
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        if(!first) 
            oss << ",";
        first = false;

        oss << "{";
        oss << "\"id\":"<<SocketUtil::escapeJson(row[0]?row[0]:"") << ",";
        oss << "\"name\":\""<<SocketUtil::escapeJson(row[1]?row[1]:"") << "\",";
        oss << "\"class_name\":\""<<SocketUtil::escapeJson(row[2]?row[2]:"") << "\",";
        oss << "\"age\":"<<SocketUtil::escapeJson(row[3]?row[3]:"") << ",";
        oss << "\"gender\":\""<<SocketUtil::escapeJson(row[4]?row[4]:"") << "\",";
        oss << "\"created_time\":\""<<SocketUtil::escapeJson(row[5]?row[5]:"") << "\",";
        oss << "\"updated_time\":\""<<SocketUtil::escapeJson(row[6]?row[6]:"") << "\"";
        oss << "}";
    }
    oss <<"]}}";
    jsonData = oss.str();
    mysql_free_result(res);
    return true;
}

//查询学生成绩(id要在进入函数前检验合法性)
bool Database::getStudentScore(std::string& jsonData,std::string& username,const int& id,const std::string& course_nameFilter)
{
    jsonData.clear();
    //先确认该学生是否存在
    bool exist = false;
    if(!studentExists(username,id,exist) || !exist)
        return false;

    if(!conn_)
        return false;

    //再看能不能查询到对应课程成绩
    std::ostringstream where;
    where << "where student_id = " << id << " ";
    where << "and owner_name = '"<<username<<"' ";
    //先判断课程名字是否为空来判断要不要联合课程名字来查询
    if(!course_nameFilter.empty())
    {
        std::string escaped;
        escaped.resize(2*course_nameFilter.length()+1);
        unsigned long len = mysql_real_escape_string(conn_,escaped.data(),course_nameFilter.c_str(),course_nameFilter.length());
        escaped.resize(len);
        where << "and course_name like '%"<<escaped<<"%'"<<" ";
    }
    std::string sql = std::string("select course_name , course_score from course_info ") + where.str();
    if(mysql_query(conn_,sql.c_str()) != 0)
    {
        LOG_ERROR(std::string("mysql_query failed: ")+mysql_error(conn_));
        return false;
    }
    MYSQL_RES* res = mysql_store_result(conn_);
    if(!res)
    {
        LOG_ERROR(std::string("mysql_store_result failed: ") + mysql_error(conn_));
        return false;
    }

    std::ostringstream oss;
    oss<<R"({"code":0,"msg":"success","data":{"courses":[)";
    bool first = true;
    MYSQL_ROW row;
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        if(!first)
            oss << ",";
        first = false;

        oss << "{";
        oss << "\"course_name\":\"" << SocketUtil::escapeJson(row[0]?row[0]:"")<<"\",";
        oss << "\"course_score\":"  << (row[1]?row[1]:"-1")<<"";
        oss << "}";
    }

    oss << "]}}";
    jsonData = oss.str();
    mysql_free_result(res);
    return true;
}
//添加课程和成绩
bool Database::insertStudentScore(std::string& username,const int& id,const std::string& course_name,const int& course_score)
{
    if(!conn_)
        return false;

    bool exist = false;
    if(!studentExists(username,id,exist) || !exist)
        return false;

    const char* sql = "insert into course_info (student_id, course_name, course_score, owner_name)"
                      "values (?, ?, ?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if(!stmt)
        return false;

    if(mysql_stmt_prepare(stmt,sql,std::strlen(sql))!=0)
    {
        LOG_ERROR(std::string("mysql_stmt_prepare failed!: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    
    //student_id(int),course_name,course_score(int)
    MYSQL_BIND param[4]{};
    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = (char*)&id;

    unsigned long len = course_name.length();
    param[1].buffer_type = MYSQL_TYPE_STRING;
    param[1].buffer = const_cast<char*>(course_name.c_str());
    param[1].length = &len;

    param[2].buffer_type = MYSQL_TYPE_LONG;
    param[2].buffer = (char*)&course_score;

    unsigned long user_len = username.length();
    param[3].buffer_type = MYSQL_TYPE_STRING;
    param[3].buffer = const_cast<char*>(username.c_str());
    param[3].length = &user_len;

    if (mysql_stmt_bind_param(stmt, param) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_bind_param failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_execute failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

bool Database::updateStudentScore(std::string& username,const int& id,const std::string& course_name,const int& course_score,const std::string& old_course_name)
{
    if(!conn_)
        return false;

    bool exist = false;
    if(!studentExists(username,id,exist) || !exist)
        return false;

    const char* sql = "update course_info set course_name = ? , course_score = ? where student_id = ? and course_name = ? and owner_name = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if(!stmt)
        return false;

    if(mysql_stmt_prepare(stmt,sql,std::strlen(sql))!=0)
    {
        LOG_ERROR(std::string("mysql_stmt_prepare failed!: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    
    //course_name,course_score(int)
    MYSQL_BIND param[5]{};

    unsigned long len = course_name.length();
    param[0].buffer_type = MYSQL_TYPE_STRING;
    param[0].buffer = const_cast<char*>(course_name.c_str());
    param[0].length = &len;

    param[1].buffer_type = MYSQL_TYPE_LONG;
    param[1].buffer = (char*)&course_score;

    param[2].buffer_type = MYSQL_TYPE_LONG;
    param[2].buffer = (char*)&id;

    unsigned long old_len = old_course_name.length();
    param[3].buffer_type = MYSQL_TYPE_STRING;
    param[3].buffer = const_cast<char*>(old_course_name.c_str());
    param[3].length = &old_len;

    unsigned long user_len = username.length();
    param[4].buffer_type = MYSQL_TYPE_STRING;
    param[4].buffer = const_cast<char*>(username.c_str());
    param[4].length = &user_len;

    if (mysql_stmt_bind_param(stmt, param) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_bind_param failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_execute failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}
bool Database::deleteStudentScore(std::string& username,const int& id,const std::string& course_name)
{
    if(!conn_)
        return false;

    bool exist = false;
    if(!studentExists(username,id,exist) || !exist)
        return false;

    const char* sql = "delete from course_info where student_id = ? and course_name = ? and owner_name = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if(!stmt)
        return false;

    if(mysql_stmt_prepare(stmt,sql,std::strlen(sql))!=0)
    {
        LOG_ERROR(std::string("mysql_stmt_prepare failed!: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    
    MYSQL_BIND param[3]{};
    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = (char*)&id;

    unsigned long len = course_name.length();
    param[1].buffer_type = MYSQL_TYPE_STRING;
    param[1].buffer = const_cast<char*>(course_name.c_str());
    param[1].length = &len;

    unsigned long user_len = username.length();
    param[2].buffer_type = MYSQL_TYPE_STRING;
    param[2].buffer = const_cast<char*>(username.c_str());
    param[2].length = &user_len;

    if(mysql_stmt_bind_param(stmt,param) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_bind_param failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if(mysql_stmt_execute(stmt) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_execute failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    mysql_stmt_close(stmt);
    return true;   
}

bool Database::courseExist(std::string& username,const int& id,const std::string& course_name,bool& exist)
{

    const char* sql = "select student_id from course_info where student_id = ? and course_name = ? and owner_name = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if(!stmt)
        return false;
    
    if(mysql_stmt_prepare(stmt,sql,std::strlen(sql))!=0)
    {
        LOG_ERROR(std::string("mysql_stmt_prepare failed: ") + mysql_stmt_error(stmt));
        return false;
    }

    MYSQL_BIND param[3]{};
    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = (char*)&id;
    
    unsigned long len = course_name.length();
    param[1].buffer_type = MYSQL_TYPE_STRING;
    param[1].buffer = const_cast<char*>(course_name.c_str());
    param[1].length = &len;

    unsigned long user_len = username.length();
    param[2].buffer_type = MYSQL_TYPE_STRING;
    param[2].buffer = const_cast<char*>(username.c_str());
    param[2].length = &user_len;

    if(mysql_stmt_bind_param(stmt,param)!= 0)
    {
        LOG_ERROR(std::string("mysql_stmt_bind_param failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;        
    }
    if(mysql_stmt_execute(stmt) !=0)
    {
        LOG_ERROR(std::string("mysql_stmt_execute failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND result[1]{};
    int select_id = 0;
    result[0].buffer=(char*)&select_id;
    result[0].buffer_type=MYSQL_TYPE_LONG;

    if(mysql_stmt_bind_result(stmt,result) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_bind_result failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;     
    }

    if(mysql_stmt_store_result(stmt) != 0)
    {
        LOG_ERROR(std::string("mysql_stmt_store_result failed: ") + mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;     
    }
    int line = mysql_stmt_fetch(stmt);
    exist = (line== 0);
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;  
}