#include "../include/server.hpp"

#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <errno.h>
#include <sstream>
#include <cstring>

/////////////////////////////////////////////////////
// 构造 & 析构
/////////////////////////////////////////////////////

Server::Server(int port,int threadNum)
    :port(port),pool(threadNum)
{
    mysqlPool = new MysqlPool(
        5,"127.0.0.1","root","Aa962464.","webserver",3306
    );

    userService = new UserService(mysqlPool);

    LOG_INFO("MySQL pool init success");
}

Server::~Server()
{
    close(listenfd);
    close(epollfd);
    delete userService;
    delete mysqlPool;
}

/////////////////////////////////////////////////////
// 工具函数
/////////////////////////////////////////////////////

void Server::setNonBlocking(int fd)
{
    int flags = fcntl(fd,F_GETFL);
    fcntl(fd,F_SETFL,flags | O_NONBLOCK);
}

void Server::addFd(int fd)
{
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.fd = fd;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

void Server::modFd(int fd)
{
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.fd = fd;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}

std::string Server::getMimeType(const std::string& path)
{
    if(path.find(".html") != std::string::npos) return "text/html";
    if(path.find(".css")) return "text/css";
    if(path.find(".js")) return "application/javascript";
    if(path.find(".png")) return "image/png";
    if(path.find(".jpg")) return "image/jpeg";
    return "text/plain";
}

std::unordered_map<std::string,std::string>
Server::parseForm(const std::string& body)
{
    std::unordered_map<std::string,std::string> form;
    std::stringstream ss(body);
    std::string pair;

    while(std::getline(ss, pair, '&'))
    {
        int pos = pair.find('=');
        if(pos != std::string::npos)
            form[pair.substr(0,pos)] = pair.substr(pos+1);
    }
    return form;
}

/////////////////////////////////////////////////////
// 初始化
/////////////////////////////////////////////////////

bool Server::init()
{
    listenfd = socket(AF_INET,SOCK_STREAM,0);

    int opt = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    sockaddr_in addr{};
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

/////////////////////////////////////////////////////
// 主循环
/////////////////////////////////////////////////////

void Server::start()
{
    while(true)
    {
        int n = epoll_wait(epollfd, events, MAX_EVENTS, -1);

        for(int i = 0; i < n; i++)
        {
            int fd = events[i].data.fd;

            if(fd == listenfd)
            {
                while(true)
                {
                    int clientfd = accept(listenfd,nullptr,nullptr);

                    if(clientfd == -1)
                    {
                        if(errno == EAGAIN) break;
                        else break;
                    }

                    setNonBlocking(clientfd);
                    addFd(clientfd);

                    clients[clientfd] = {
                        clientfd,
                        std::chrono::steady_clock::now()
                    };

                    LOG_INFO("new client fd=" + std::to_string(clientfd));
                }
            }
            else
            {
                if(events[i].events & EPOLLIN)
                {
                    pool.addTask([this, fd](){
                        handleClient(fd);
                    });
                }

                if(events[i].events & EPOLLOUT)
                {
                    pool.addTask([this, fd](){
                        handleWrite(fd);
                    });
                }
            }
        }
    }
}

/////////////////////////////////////////////////////
// 核心：处理客户端（含分包 + ONESHOT）
/////////////////////////////////////////////////////

void Server::handleClient(int clientfd)
{
    LOG_INFO("handleClient fd=" + std::to_string(clientfd));

    char buffer[4096];

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
        else if(len == 0)
        {
            LOG_INFO("peer closed (client FIN)fd=" + std::to_string(clientfd));
            close(clientfd);
            clients.erase(clientfd);
            readBuffers.erase(clientfd);
            return;
        }
        else
        {
            return;
        }
    }

    std::string &buf = readBuffers[clientfd];
    if(buf.empty()) return;

    while(true)
    {
        size_t pos = buf.find("\r\n\r\n");
        if(pos == std::string::npos) break;

        size_t headerLen = pos + 4;
        std::string headerPart = buf.substr(0, headerLen);
        HttpRequest req = HttpParser::parse(headerPart);

        size_t totalLen = headerLen;

        if(req.method == "POST" && req.headers.count("Content-Length"))
        {
            int bodyLen = std::stoi(req.headers["Content-Length"]);
            totalLen += bodyLen;

            if(buf.size() < totalLen) break;

            req = HttpParser::parse(buf.substr(0, totalLen));
        }

        LOG_INFO("HTTP " + req.method + " " + req.url);

        // ===== 业务处理 =====
        if(req.method == "POST")
        {
            auto form = parseForm(req.body);
            std::string user = form["username"];
            std::string pwd  = form["password"];

            if(req.url == "/login")
            {
                sendFile(clientfd,
                    userService->login(user,pwd) ?
                    "www/success.html" : "www/error.html",
                    req.isKeepAlive());
            }
        }
        else
        {
            std::string path = (req.url == "/") ?
                "www/index.html" : "www" + req.url;

            sendFile(clientfd, path, req.isKeepAlive());
        }

        buf.erase(0, totalLen);

        if(!req.isKeepAlive())
        {
            close(clientfd);
            clients.erase(clientfd);
            readBuffers.erase(clientfd);
            return;
        }

        if(buf.empty()) break;
    }

    // 🔥 关键：重新激活 ONESHOT
    modFd(clientfd);
}

/////////////////////////////////////////////////////
// sendFile
/////////////////////////////////////////////////////

void Server::sendFile(int clientfd,
    const std::string& filePath,
    bool keepAlive)
{
    LOG_INFO("prepare sendFile: " + filePath);
    LOG_DEBUG("switch to EPOLLOUT fd=" + std::to_string(clientfd));

    WriteBuffer wb;

    int fd = open(filePath.c_str(), O_RDONLY);
    if(fd == -1)
    {
    LOG_ERROR("file not found");

    wb.header =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n\r\n";

    wb.keepAlive = false;
    }
    else
    {
    struct stat st;
    fstat(fd, &st);

    wb.header =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: " + getMimeType(filePath) + "\r\n"
    "Content-Length: " + std::to_string(st.st_size) + "\r\n" +
    (keepAlive ? "Connection: keep-alive\r\n" : "Connection: close\r\n") +
    "\r\n";

    wb.fileFd = fd;
    wb.fileSize = st.st_size;
    wb.offset = 0;
    wb.keepAlive = keepAlive;
    }

    wb.headerSent = 0;

    writeBuffers[clientfd] = wb;

    // 🔥 切换到写事件
    epoll_event ev;
    ev.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
    ev.data.fd = clientfd;

    if(epoll_ctl(epollfd, EPOLL_CTL_MOD, clientfd, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl MOD EPOLLOUT failed fd=" + std::to_string(clientfd));
    }
    else
    {
        LOG_DEBUG("switch to EPOLLOUT fd=" + std::to_string(clientfd));
    }

    handleWrite(clientfd);
}

/////////////////////////////////////////////////////
// handleWrite
/////////////////////////////////////////////////////

void Server::handleWrite(int clientfd)
{
    LOG_INFO("handleWrite fd=" + std::to_string(clientfd));
    auto &wb = writeBuffers[clientfd];

    // ===== 1. 发送 header =====
    while(wb.headerSent < wb.header.size())
    {
        ssize_t n = write(clientfd,
            wb.header.c_str() + wb.headerSent,
            wb.header.size() - wb.headerSent);

        if(n > 0)
        {
            wb.headerSent += n;
        }
        else if(n == -1 && errno == EAGAIN)
        {
            goto rearm;
        }
        else
        {
            goto close_conn;
        }
    }

    // ===== 2. 发送文件 =====
    while(wb.fileFd != -1 && wb.offset < wb.fileSize)
    {
        ssize_t n = sendfile(clientfd,
                             wb.fileFd,
                             &wb.offset,
                             wb.fileSize - wb.offset);

        if(n > 0)
        {
            continue;
        }
        else if(n == -1 && errno == EAGAIN)
        {
            goto rearm;
        }
        else
        {
            goto close_conn;
        }
    }

    // ===== 3. 发送完成 =====
    if(wb.fileFd != -1)
        close(wb.fileFd);

    if(wb.keepAlive)
    {
        LOG_INFO("write done keep-alive fd=" + std::to_string(clientfd));

        writeBuffers.erase(clientfd);
        modFd(clientfd); // 切回读
        return;
    }

close_conn:
    LOG_INFO("write done close fd=" + std::to_string(clientfd));

    close(clientfd);
    clients.erase(clientfd);
    readBuffers.erase(clientfd);
    writeBuffers.erase(clientfd);
    return;

rearm:
    epoll_event ev;
    ev.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
    ev.data.fd = clientfd;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, clientfd, &ev);
}