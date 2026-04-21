#pragma once
#include <mysql/mysql.h>
#include <string>

class Database {
public:
    Database();
    ~Database();

    bool connect(const std::string& host,
                 const std::string& user,
                 const std::string& password,
                 const std::string& dbname,
                 unsigned int port);
    void close();
    bool connected() const;

    bool userExists(const std::string& username, bool& exists);
    bool createUser(const std::string& username, const std::string& passwordHash);
    bool getPasswordHash(const std::string& username, std::string& passwordHash);

private:
    MYSQL* conn_;
};
