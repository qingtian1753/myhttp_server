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
    bool studentExists(std::string&username,const int& id,bool& exist);
    bool insertStudent(std::string& username,const int& id,const std::string& name,const std::string& class_name,const int& age,const std::string& gender);
    bool updateStudent(std::string& username,const int& id,const std::string& name,const std::string& class_name,const int& age,const std::string& gender,const int& old_id);
    bool deleteStudent(std::string& username,const int& id);
    bool listStudents(std::string& username,std::string& jsonData,int page,int size,const std::string& idFilter,const std::string& nameFilter,const std::string& classFilter);
    bool getStudentScore(std::string& jsonData,std::string& username,const int& id,const std::string& course_nameFilter);
    bool insertStudentScore(std::string& username,const int& id,const std::string& course_name,const int& course_score);
    bool updateStudentScore(std::string& username,const int& id,const std::string& course_name,const int& course_score,const std::string& old_course_name);
    bool deleteStudentScore(std::string& username,const int& id,const std::string& course_name);
    

    bool courseExist(std::string& username,const int& id,const std::string& course_name,bool& exist);

private:
    MYSQL* conn_;
};
