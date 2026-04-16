#ifndef LOG_HPP
#define LOG_HPP

#include <iostream>
#include <ctime>
#include <sstream>
#include <unistd.h>
#include <sys/syscall.h>

/**
 * @brief 高级日志系统
 * 
 * 支持输出：
 * - 时间戳
 * - 线程 ID
 * - 文件名与行号
 * - 日志级别
 */
class Log
{
public:
    /**
     * @brief 输出 INFO 级别日志
     */
    static void info(const std::string &msg,
                     const char* file,
                     int line)
    {
        print("INFO", msg, file, line);
    }

    /**
     * @brief 输出 DEBUG 级别日志
     */
    static void debug(const std::string &msg,
                      const char* file,
                      int line)
    {
        print("DEBUG", msg, file, line);
    }

    /**
     * @brief 输出 ERROR 级别日志
     */
    static void error(const std::string &msg,
                      const char* file,
                      int line)
    {
        print("ERROR", msg, file, line);
    }

private:
    /**
     * @brief 统一打印日志的内部函数
     */
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

    /**
     * @brief 获取当前时间的字符串表示
     * @return 格式：YYYY-MM-DD HH:MM:SS
     */
    static std::string getTime()
    {
        time_t now = time(nullptr);
        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        return std::string(buf);
    }
};

// 便捷宏定义：自动捕获文件名和行号
#define LOG_INFO(msg)  Log::info(msg,  __FILE__, __LINE__)
#define LOG_DEBUG(msg) Log::debug(msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Log::error(msg, __FILE__, __LINE__)

#endif // LOG_HPP