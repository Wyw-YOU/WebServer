#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "threadpool.hpp"
#include "http_parser.hpp"
#include "log.hpp"

/*
    高性能WebServer
    epoll(ET) + Reactor + ThreadPool
*/

class Server
{

private:

    int port;           // 服务器端口

    int listenfd;       // 监听socket

    int epollfd;        // epoll描述符

    ThreadPool pool;    // 线程池

    static const int MAX_EVENTS = 1024;

    epoll_event events[MAX_EVENTS];

public:

    /*
        构造函数
        port：监听端口
        threadNum：线程池数量
    */
    Server(int port,int threadNum)
        :port(port),pool(threadNum)
    {

    }

    /*
        设置socket为非阻塞
    */
    void setNonBlocking(int fd)
    {
        int flags = fcntl(fd,F_GETFL);

        fcntl(fd,F_SETFL,flags | O_NONBLOCK);
    }

    /*
        初始化服务器
    */
    bool init()
    {

        listenfd = socket(AF_INET,SOCK_STREAM,0);

        if(listenfd < 0)
        {
            Log::error("socket create failed");
            return false;
        }

        sockaddr_in addr;

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        bind(listenfd,(sockaddr*)&addr,sizeof(addr));

        listen(listenfd,128);

        // 设置非阻塞
        setNonBlocking(listenfd);

        // 创建epoll
        epollfd = epoll_create(1);

        epoll_event ev;

        ev.events = EPOLLIN;

        ev.data.fd = listenfd;

        epoll_ctl(epollfd,EPOLL_CTL_ADD,listenfd,&ev);

        Log::info("Server start success");

        return true;
    }

    /*
    处理客户端请求
    使用ET模式必须循环读取
    使用sendfile实现零拷贝发送文件
    */
    void handleClient(int clientfd)
    {

        char buffer[4096];

        std::string request;

        // ET模式读取必须循环
        while(true)
        {

            int len = read(clientfd,buffer,sizeof(buffer));

            if(len > 0)
            {
                request.append(buffer,len);
            }
            else
            {
                break;
            }

        }

        if(request.empty())
        {
            close(clientfd);
            return;
        }

        Log::info("HTTP request received");

        // 解析HTTP请求
        HttpRequest req = HttpParser::parse(request);

        Log::info("Method: " + req.method);
        Log::info("URL: " + req.url);
        Log::info("Version: " + req.version);

        if(req.headers.count("User-Agent"))
        {
            Log::info("User-Agent: " + req.headers["User-Agent"]);
        }

        // 构造文件路径
        std::string filePath = "www";

        if(req.url == "/")
            filePath += "/index.html";
        else
            filePath += req.url;

        // 打开文件
        int filefd = open(filePath.c_str(),O_RDONLY);

        if(filefd == -1)
        {

            std::string notFound =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "404 Not Found";

            write(clientfd,notFound.c_str(),notFound.size());

            close(clientfd);

            return;
        }

        // 获取文件信息
        struct stat file_stat;

        fstat(filefd,&file_stat);

        // 构造HTTP响应头
        std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: " + std::to_string(file_stat.st_size) + "\r\n"
        "\r\n";

        write(clientfd,header.c_str(),header.size());

        // 使用sendfile发送文件（零拷贝）
        sendfile(clientfd,filefd,nullptr,file_stat.st_size);

        close(filefd);

        close(clientfd);

    }

    /*
        启动服务器
    */
    void start()
    {

        while(true)
        {

            int n = epoll_wait(epollfd,events,MAX_EVENTS,-1);

            for(int i=0;i<n;i++)
            {

                int fd = events[i].data.fd;

                // 新客户端连接
                if(fd == listenfd)
                {

                    int clientfd = accept(listenfd,nullptr,nullptr);

                    setNonBlocking(clientfd);

                    epoll_event ev;

                    ev.events = EPOLLIN | EPOLLET;

                    ev.data.fd = clientfd;

                    epoll_ctl(epollfd,EPOLL_CTL_ADD,clientfd,&ev);

                    Log::info("client connected");

                }
                else
                {

                    int clientfd = fd;

                    // Reactor模式
                    pool.addTask([this,clientfd](){

                        handleClient(clientfd);

                    });

                }

            }

        }

    }

};

#endif