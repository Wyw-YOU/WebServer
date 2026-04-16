#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/epoll.h>
#include <unistd.h>
#include <unordered_map>
#include <chrono>
#include <string>

#include "threadpool.hpp"
#include "user_service.hpp"
#include "http_parser.hpp"
#include "log.hpp"
#include "mysql_pool.hpp"

/**
 * @brief 客户端连接信息
 */
struct Client
{
    int fd;                                      ///< 套接字描述符
    std::chrono::steady_clock::time_point last_active; ///< 最后活跃时间
};

/**
 * @brief 写缓冲区，用于管理 HTTP 响应发送状态
 */
struct WriteBuffer
{
    std::string header;     ///< 响应头字符串
    size_t headerSent = 0;  ///< 已发送的头部字节数

    int fileFd = -1;        ///< 待发送文件的描述符
    off_t offset = 0;       ///< 文件发送偏移量
    size_t fileSize = 0;    ///< 文件总大小

    bool keepAlive = false; ///< 是否保持长连接
};

// 发送状态枚举
enum class SendStatus {
    OK,      // 当前阶段发送完成（数据已全部写入内核缓冲区）
    WOULD_BLOCK,  // socket 缓冲区满，需等待下次 EPOLLOUT 事件
    ERROR    // 发送错误（连接断开、文件读取失败等）
};

/**
 * @brief HTTP 服务器主类
 */
class Server
{
private:
    int port;               ///< 监听端口
    int listenfd;           ///< 监听套接字
    int epollfd;            ///< epoll 实例描述符

    ThreadPool pool;        ///< 线程池

    static const int MAX_EVENTS = 1024; ///< epoll_wait 最大事件数
    epoll_event events[MAX_EVENTS];     ///< 就绪事件数组

    std::unordered_map<int, Client> clients;           ///< 客户端连接表
    std::unordered_map<int, std::string> readBuffers;  ///< 读缓冲区
    std::unordered_map<int, WriteBuffer> writeBuffers; ///< 写缓冲区

    MysqlPool* mysqlPool;       ///< MySQL 连接池
    UserService* userService;   ///< 用户服务

private:
    // ===== 核心模块 =====
    void handleClient(int clientfd);
    void sendFile(int clientfd, const std::string& filePath, bool keepAlive);
    void handleWrite(int clientfd);

    // ===== 工具函数 =====
    void setNonBlocking(int fd);
    std::string getMimeType(const std::string& path);
    std::unordered_map<std::string,std::string> parseForm(const std::string& body);
    void closeConnection(int clientfd);
    void rearmWriteEvent(int clientfd);
    SendStatus sendHeader(int clientfd, WriteBuffer& wb);
    SendStatus sendFileContent(int clientfd, WriteBuffer& wb);

    // ===== epoll 控制 =====
    void addFd(int fd);
    void modFd(int fd);

public:
    Server(int port, int threadNum);
    ~Server();

    bool init();
    void start();
};

#endif // SERVER_HPP