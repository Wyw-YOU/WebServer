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
#include "http_parser.hpp"
#include "log.hpp"

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

public:

    Server(int port,int threadNum)
        :port(port),pool(threadNum)
    {}

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
        std::string request;
    
        // ===== 1. 读取数据（ET必须循环读） =====
        while(true)
        {
            int len = read(clientfd, buffer, sizeof(buffer));
    
            if(len > 0)
            {
                request.append(buffer, len);
                LOG_DEBUG("read bytes=" + std::to_string(len));
            }
            else if(len == -1 && errno == EAGAIN)
            {
                break;
            }
            else if(len == 0)
            {
                LOG_INFO("client closed fd=" + std::to_string(clientfd));
                close(clientfd);
                clients.erase(clientfd);
                return;
            }
            else
            {
                LOG_ERROR("read error fd=" + std::to_string(clientfd));
                close(clientfd);
                clients.erase(clientfd);
                return;
            }
        }
    
        if(request.empty()) return;
    
        LOG_INFO("HTTP request:\n" + request);
    
        // ===== 2. 解析HTTP =====
        HttpRequest req = HttpParser::parse(request);
    
        LOG_INFO("method=" + req.method + " url=" + req.url);
    
        // ===== 3. POST处理（新增核心） =====
        if(req.method == "POST")
        {
            LOG_INFO("POST body=" + req.body);
    
            auto form = parseForm(req.body);
    
            if(req.url == "/login")
            {
                std::string username = form["username"];
                std::string password = form["password"];
    
                LOG_INFO("login user=" + username);
    
                if(username == "admin" && password == "123")
                {
                    sendFile(clientfd, "www/success.html", req.isKeepAlive());
                }
                else
                {
                    sendFile(clientfd, "www/error.html", req.isKeepAlive());
                }
            }
    
            return;
        }
    
        // ===== 4. GET处理 =====
        if(req.method == "GET")
        {
            std::string filePath = "www";
    
            if(req.url == "/")
                filePath += "/index.html";
            else
                filePath += req.url;
    
            sendFile(clientfd, filePath, req.isKeepAlive());
        }
    }

    void sendFile(int clientfd, const std::string& filePath, bool keepAlive)
    {
        int filefd = open(filePath.c_str(), O_RDONLY);

        if(filefd == -1)
        {
            LOG_ERROR("404 file=" + filePath);
        
            std::string notFound = "www/404.html";
        
            int fd404 = open(notFound.c_str(), O_RDONLY);
        
            if(fd404 == -1)
            {
                // 极端情况：连404页面都没
                std::string res = "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
                write(clientfd, res.c_str(), res.size());
                close(clientfd);
                clients.erase(clientfd);
                return;
            }
        
            struct stat st;
            fstat(fd404, &st);
        
            std::string header =
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: " + std::to_string(st.st_size) + "\r\n\r\n";
        
            write(clientfd, header.c_str(), header.size());
        
            off_t offset = 0;
            while(offset < st.st_size)
            {
                ssize_t sent = sendfile(clientfd, fd404, &offset, st.st_size - offset);
        
                if(sent <= 0)
                {
                    if(errno == EAGAIN) continue;
                    break;
                }
            }
        
            close(fd404);
            close(clientfd);
            clients.erase(clientfd);
            return;
        }

        struct stat st;
        fstat(filefd, &st);

        std::string header =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: " + getMimeType(filePath) + "\r\n"
            "Content-Length: " + std::to_string(st.st_size) + "\r\n";

        if(keepAlive)
            header += "Connection: keep-alive\r\n";
        else
            header += "Connection: close\r\n";

        header += "\r\n";

        write(clientfd, header.c_str(), header.size());

        off_t offset = 0;
        while(offset < st.st_size)
        {
            ssize_t sent = sendfile(clientfd, filefd, &offset, st.st_size - offset);

            if(sent <= 0)
            {
                if(errno == EAGAIN) continue;
                break;
            }
        }

        close(filefd);

        if(!keepAlive)
        {
            close(clientfd);
            clients.erase(clientfd);
        }
        else
        {
            clients[clientfd].last_active = std::chrono::steady_clock::now();
        }
    }

    // 表单解析
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
                std::string key = pair.substr(0, pos);
                std::string value = pair.substr(pos + 1);
                form[key] = value;
            }
        }

        return form;
    }

    /*
        启动服务器
    */
    void start()
    {
        while(true)
        {
            int n = epoll_wait(epollfd,events,MAX_EVENTS,1000);

            if(n == -1)
            {
                LOG_ERROR("epoll_wait error");
                continue;
            }

            if(n == 0)
            {
                // 超时，不打印（避免刷屏）
                continue;
            }

            // 有事件才打印
            LOG_DEBUG("epoll triggered, events=" + std::to_string(n));

            for(int i=0;i<n;i++)
            {
                int fd = events[i].data.fd;

                // 新连接（ET必须循环accept）
                if(fd == listenfd)
                {
                    while(true)
                    {
                        sockaddr_in client_addr;
                        socklen_t len = sizeof(client_addr);

                        int clientfd = accept(listenfd,(sockaddr*)&client_addr,&len);

                        if(clientfd < 0)
                        {
                            if(errno == EAGAIN) break;
                            else break;
                        }

                        char ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET,&client_addr.sin_addr,ip,sizeof(ip));

                        int port = ntohs(client_addr.sin_port);

                        LOG_INFO("new client " + std::string(ip) + ":" + std::to_string(port));

                        setNonBlocking(clientfd);

                        epoll_event ev;
                        ev.events = EPOLLIN | EPOLLET;
                        ev.data.fd = clientfd;

                        epoll_ctl(epollfd,EPOLL_CTL_ADD,clientfd,&ev);

                        clients[clientfd] = {clientfd,std::chrono::steady_clock::now()};
                    }
                }
                else
                {
                    int clientfd = fd;

                    pool.addTask([this,clientfd](){
                        handleClient(clientfd);
                    });
                }
            }

            // 定时器（每轮执行）
            auto now = std::chrono::steady_clock::now();

            for(auto it = clients.begin(); it != clients.end(); )
            {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                    now - it->second.last_active
                ).count();

                if(duration > 60)
                {
                    LOG_INFO("timeout fd=" + std::to_string(it->first));
                    close(it->first);
                    it = clients.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

};

#endif