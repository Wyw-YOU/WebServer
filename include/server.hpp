#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <errno.h>

#include <unordered_map>
#include <chrono>
#include <string>
#include <sstream>

#include "threadpool.hpp"
#include "user_service.hpp"
#include "http_parser.hpp"
#include "log.hpp"
#include "mysql_pool.hpp"  // ✅ 显式包含

/*
    客户端连接信息
*/
struct Client
{
    int fd;
    std::chrono::steady_clock::time_point last_active;
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
    std::unordered_map<int, std::string> readBuffers;   // fd → 未处理完的请求数据

    MysqlPool* mysqlPool;
    UserService* userService;  // ✅ 改为指针（关键）

    // ===== 前置声明（关键修复）=====
    void sendFile(int clientfd, const std::string& filePath, bool keepAlive);

public:
    Server(int port,int threadNum)
        :port(port),pool(threadNum)
    {
        // ===== 初始化连接池 =====
        mysqlPool = new MysqlPool(
            5,
            "127.0.0.1",
            "root",
            "Aa962464.",
            "webserver",
            3306
        );

        // ===== 初始化用户服务 =====
        userService = new UserService(mysqlPool);

        LOG_INFO("MySQL pool init success");
    }

    /*
        设置非阻塞
    */
    void setNonBlocking(int fd)
    {
        int flags = fcntl(fd,F_GETFL);
        fcntl(fd,F_SETFL,flags | O_NONBLOCK);
    }

    /*
        初始化
    */
    bool init()
    {
        listenfd = socket(AF_INET,SOCK_STREAM,0);

        int opt = 1;
        setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        bind(listenfd,(sockaddr*)&addr,sizeof(addr));
        listen(listenfd,128);

        setNonBlocking(listenfd);

        epollfd = epoll_create(1);

        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = listenfd;

        epoll_ctl(epollfd,EPOLL_CTL_ADD,listenfd,&ev);

        LOG_INFO("Server start success");

        return true;
    }

    /*
        MIME类型
    */
    std::string getMimeType(const std::string& path)
    {
        if(path.find(".html") != std::string::npos) return "text/html";
        if(path.find(".css")  != std::string::npos) return "text/css";
        if(path.find(".js")   != std::string::npos) return "application/javascript";
        if(path.find(".png")  != std::string::npos) return "image/png";
        if(path.find(".jpg")  != std::string::npos) return "image/jpeg";
        return "text/plain";
    }

    /*
        处理客户端
    */
    void handleClient(int clientfd)
    {
        LOG_INFO("handleClient fd=" + std::to_string(clientfd));

        char buffer[4096];

        // ===== 读入数据（ET模式必须循环）=====
        while(true)
        {
            int len = read(clientfd, buffer, sizeof(buffer));

            if(len > 0)
            {
                readBuffers[clientfd].append(buffer, len);
            }
            else if(len == -1 && errno == EAGAIN)
            {
                break;
            }
            else
            {
                LOG_INFO("client closed fd=" + std::to_string(clientfd));
                close(clientfd);
                clients.erase(clientfd);
                readBuffers.erase(clientfd);
                return;
            }
        }

        std::string &request = readBuffers[clientfd];

        if(request.empty()) return;

        LOG_DEBUG("raw request size=" + std::to_string(request.size()));

        // ===== 解析 HTTP =====
        HttpRequest req = HttpParser::parse(request);

        // ===== 分包处理（关键）=====
        if(req.method == "POST" && req.headers.count("Content-Length"))
        {
            int len = std::stoi(req.headers["Content-Length"]);

            if(req.body.size() < len)
            {
                LOG_DEBUG("POST body incomplete, wait more data");
                return; // ❗继续等待数据
            }
        }

        LOG_INFO("HTTP request: " + req.method + " " + req.url);

        // ===== 处理请求 =====
        if(req.method == "POST")
        {
            auto form = parseForm(req.body);

            std::string user = form["username"];
            std::string pwd  = form["password"];

            if(req.url == "/login")
            {
                if(userService->login(user, pwd))
                    sendFile(clientfd,"www/success.html",req.isKeepAlive());
                else
                    sendFile(clientfd,"www/error.html",req.isKeepAlive());
            }
            else if(req.url == "/register")
            {
                if(userService->registerUser(user, pwd))
                    sendFile(clientfd,"www/success.html",req.isKeepAlive());
                else
                    sendFile(clientfd,"www/error.html",req.isKeepAlive());
            }
        }
        else
        {
            std::string filePath = "www";
            if(req.url == "/") filePath += "/index.html";
            else filePath += req.url;

            sendFile(clientfd, filePath, req.isKeepAlive());
        }

        // ===== 请求处理完，清空缓冲区 =====
        readBuffers.erase(clientfd);
    }

    // 表单解析（保持不变）
    std::unordered_map<std::string,std::string>
    parseForm(const std::string& body)
    {
        std::unordered_map<std::string,std::string> form;
        std::stringstream ss(body);
        std::string pair;

        while(std::getline(ss, pair, '&'))
        {
            int pos = pair.find('=');
            if(pos != std::string::npos)
            {
                form[pair.substr(0,pos)] = pair.substr(pos+1);
            }
        }
        return form;
    }

    // sendFile 保持你原来的（无需改）

    /*
        启动服务器（保持不变）
    */
    void start();

};

#endif