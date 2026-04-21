#include "database.h"
#include "Log.h"
#include <cstring>

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
        mysql_close(conn_);
        conn_ = nullptr;
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
    result[0].buffer = &id;
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
