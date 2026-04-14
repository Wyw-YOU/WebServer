#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/epoll.h>
#include <unordered_map>
#include <chrono>
#include <string>

#include "threadpool.hpp"
#include "user_service.hpp"
#include "http_parser.hpp"
#include "log.hpp"
#include "mysql_pool.hpp"

struct Client
{
    int fd;
    std::chrono::steady_clock::time_point last_active;
};

struct WriteBuffer
{
    std::string header;
    size_t headerSent = 0;

    int fileFd = -1;
    off_t offset = 0;
    size_t fileSize = 0;

    bool keepAlive = false;
};

class Server
{
private:
    int port;
    int listenfd;
    int epollfd;

    ThreadPool pool;

    static const int MAX_EVENTS = 1024;
    epoll_event events[MAX_EVENTS];

    std::unordered_map<int, Client> clients;
    std::unordered_map<int, std::string> readBuffers;
    std::unordered_map<int, WriteBuffer> writeBuffers;

    MysqlPool* mysqlPool;
    UserService* userService;

private:
    // ===== 核心模块 =====
    void handleClient(int clientfd);
    void sendFile(int clientfd, const std::string& filePath, bool keepAlive);
    void handleWrite(int clientfd);

    // ===== 工具函数 =====
    void setNonBlocking(int fd);
    std::string getMimeType(const std::string& path);
    std::unordered_map<std::string,std::string> parseForm(const std::string& body);

    // ===== epoll控制 =====
    void addFd(int fd);
    void modFd(int fd);

public:
    Server(int port,int threadNum);
    ~Server();

    bool init();
    void start();
};

#endif