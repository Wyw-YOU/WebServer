#include <sys/uio.h>  // write
#include <cstring>

#include "../include/server.hpp"

void Server::start()
{
    while(true)
    {
        int n = epoll_wait(epollfd, events, MAX_EVENTS, -1);

        for(int i = 0; i < n; i++)
        {
            int fd = events[i].data.fd;

            // 新连接
            if(fd == listenfd)
            {
                while(true)
                {
                    int clientfd = accept(listenfd, nullptr, nullptr);

                    if(clientfd == -1)
                    {
                        if(errno == EAGAIN) break;
                        else break;
                    }

                    setNonBlocking(clientfd);

                    epoll_event ev;
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = clientfd;

                    epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &ev);

                    clients[clientfd] = {
                        clientfd,
                        std::chrono::steady_clock::now()
                    };

                    LOG_INFO("new client fd=" + std::to_string(clientfd));
                }
            }
            else
            {
                // 交给线程池处理
                pool.addTask([this, fd](){
                    handleClient(fd);
                });
            }
        }
    }
}


/*
    发送文件（支持Keep-Alive）
*/
void Server::sendFile(int clientfd, const std::string& filePath, bool keepAlive)
{
    LOG_INFO("sendFile: " + filePath + " fd=" + std::to_string(clientfd));

    int fd = open(filePath.c_str(), O_RDONLY);
    if(fd == -1)
    {
        LOG_ERROR("file not found: " + filePath);

        std::string notFound =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<h1>404 Not Found</h1>";

        write(clientfd, notFound.c_str(), notFound.size());
        return;
    }

    struct stat st;
    fstat(fd, &st);

    std::string header =
        "HTTP/1.1 200 OK\r\n" +
        std::string("Content-Type: ") + getMimeType(filePath) + "\r\n" +
        "Content-Length: " + std::to_string(st.st_size) + "\r\n" +
        (keepAlive ? "Connection: keep-alive\r\n" : "Connection: close\r\n") +
        "\r\n";

    // 发送 header
    write(clientfd, header.c_str(), header.size());

    // 零拷贝发送文件
    off_t offset = 0;
    while(offset < st.st_size)
    {
        ssize_t sent = sendfile(clientfd, fd, &offset, st.st_size - offset);
        if(sent <= 0)
        {
            if(errno == EAGAIN) continue;
            break;
        }
    }

    close(fd);

    if(!keepAlive)
    {
        close(clientfd);
        clients.erase(clientfd);
        LOG_INFO("connection closed fd=" + std::to_string(clientfd));
    }
}