#include <iostream>
#include <atomic>

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
    static bool enabled(LogLevel level);
    static void log(LogLevel level,const char* file,int line,const std::string& msg);
    static void setLevel(LogLevel level);
private:
    //将等级格式化为string
    static std::string levelToString(LogLevel level);

    static std::string getCurrentTime();

    static LogLevel getLevel();
private:
    static std::atomic<LogLevel>current_log_level;
};

//注意是两个_ 不是一个_
//__FILE__ __LINE__在预处理阶段由编译器进行替换，__file__为当前文件名字(有的编译器会替换为路径)
//__LINE__会替换为使用__LINE__的行号

//低级别日志不要输出
#define LOG_DEBUG(msg) if(Log::enabled(LogLevel::debug))Log::log(LogLevel::debug,__FILE__, __LINE__,msg)
#define LOG_INFO(msg)  if(Log::enabled(LogLevel::info)) Log::log(LogLevel::info, __FILE__, __LINE__,msg)
#define LOG_WARN(msg)  if(Log::enabled(LogLevel::warn)) Log::log(LogLevel::warn, __FILE__, __LINE__,msg)
#define LOG_ERROR(msg) if(Log::enabled(LogLevel::error))Log::log(LogLevel::error,__FILE__, __LINE__,msg)
