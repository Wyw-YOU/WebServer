#ifndef LOG_HPP
#define LOG_HPP

#include <iostream>
#include <ctime>

/*
    简单日志系统
    用于打印服务器运行信息
*/

class Log
{
public:

    // INFO日志
    static void info(const std::string &msg)
    {
        std::cout << getTime() << " [INFO] " << msg << std::endl;
    }

    // ERROR日志
    static void error(const std::string &msg)
    {
        std::cout << getTime() << " [ERROR] " << msg << std::endl;
    }

private:

    // 获取当前时间
    static std::string getTime()
    {
        time_t now = time(nullptr);
        char buf[64];

        strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S",localtime(&now));

        return std::string(buf);
    }
};

#endif