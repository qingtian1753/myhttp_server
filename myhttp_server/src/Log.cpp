#include "Log.h"
#include <ctime>
#include <sstream>
#include <iomanip>

void Log::log(LogLevel level,const char* file,int line,const std::string& msg)
{
    //判断输出方式，如果是错误的话就要cerr，这样输出会立即刷新缓冲区，立即输出
    //(但其实也没有判断的必要，用cout也行，只是我觉得用cerr输出错误会更规范)
    std::ostream& out =(level == LogLevel::error ? std::cerr : std::cout);
    out<<"["<<getCurrentTime()<<"] "
        <<levelToString(level)
       <<"["<<file<<":"<<line<<"] "
       <<msg<<"\n";
}
std::string Log::levelToString(LogLevel level)
{
    switch (level)
    {
        case LogLevel::debug : return "[DEBUG]";
        case LogLevel::info  : return "[INFO]";
        case LogLevel::warn  : return "[WARN]";
        case LogLevel::error : return "[ERROR]";
        default              : return "[UNKNOWN]";
    }
}

std::string Log::getCurrentTime()
{
    //获取从1970年1月1日开始到现在的秒数
    std::time_t now = time(nullptr);
    //本地时间结构体，用来存放本地时间
    std::tm localtime{};

    //POSIX系统下用这个函数计算出本地时间并存储到localtime里
    //时间为UTC时间，比我东八区的时间少8小时，可以修改系统配置来解决
    //sudo timedatectl set-timezone Asia/Shanghai 设置时间为上海时区
    localtime_r(&now,&localtime);
    //用来接收时间字符串
    std::ostringstream oss;
    //将时间按照指定格式传入到oss中
    oss<<std::put_time(&localtime,"%Y-%m-%d %H:%M:%S");

    return oss.str();
}