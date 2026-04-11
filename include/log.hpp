#ifndef LOG_HPP
#define LOG_HPP

#include <iostream>
#include <ctime>
#include <sstream>
#include <unistd.h>
#include <sys/syscall.h>

/*
    高级日志系统
    支持：
    - 时间
    - 线程ID
    - 文件名 + 行号
    - 日志级别
*/

class Log
{
public:

    static void info(const std::string &msg,
                     const char* file,
                     int line)
    {
        print("INFO",msg,file,line);
    }

    static void debug(const std::string &msg,
                      const char* file,
                      int line)
    {
        print("DEBUG",msg,file,line);
    }

    static void error(const std::string &msg,
                      const char* file,
                      int line)
    {
        print("ERROR",msg,file,line);
    }

private:

    static void print(const std::string &level,
                      const std::string &msg,
                      const char* file,
                      int line)
    {
        std::cout
        << getTime()
        << " [" << level << "] "
        << "[TID:" << syscall(SYS_gettid) << "] "
        << file << ":" << line << " | "
        << msg
        << std::endl;
    }

    static std::string getTime()
    {
        time_t now = time(nullptr);
        char buf[64];

        strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S",localtime(&now));

        return std::string(buf);
    }
};

/*
    宏定义（自动带文件名和行号）
*/

#define LOG_INFO(msg) Log::info(msg,__FILE__,__LINE__)
#define LOG_DEBUG(msg) Log::debug(msg,__FILE__,__LINE__)
#define LOG_ERROR(msg) Log::error(msg,__FILE__,__LINE__)

#endif