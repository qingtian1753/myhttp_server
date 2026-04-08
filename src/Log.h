#include <iostream>

//三种等级
enum class LogLevel{
    debug,
    info,
    warn,
    error,
};
class Log{
    
public:

public:
    static void log(LogLevel level,const char* file,int line,const std::string& msg);

private:
    //将等级格式化为string
    static std::string levelToString(LogLevel level);

    static std::string getCurrentTime();
};

//是两个_ 不是一个_
//__FILE__ __LINE__在预处理阶段由编译器进行替换，__file__为当前文件名字(有的编译器会替换为路径)
//__LINE__会替换为使用__LINE__的行号
#define LOG_DEBUG(msg) Log::log(LogLevel::debug,__FILE__, __LINE__,msg)
#define LOG_INFO(msg)  Log::log(LogLevel::info, __FILE__, __LINE__,msg)
#define LOG_WARN(msg)  Log::log(LogLevel::warn, __FILE__, __LINE__,msg)
#define LOG_ERROR(msg) Log::log(LogLevel::error,__FILE__, __LINE__,msg)
