#ifndef MYSQL_POOL_HPP
#define MYSQL_POOL_HPP

#include <mysql/mysql.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>

class MysqlPool
{
private:
    std::queue<MYSQL*> connQueue;
    std::mutex mtx;
    std::condition_variable cv;

    std::string host, user, password, db;
    int port;

public:
    MysqlPool(int size,
              const std::string& host,
              const std::string& user,
              const std::string& password,
              const std::string& db,
              int port)
        :host(host),user(user),password(password),db(db),port(port)
    {
        for(int i=0;i<size;i++)
        {
            MYSQL* conn = mysql_init(nullptr);

            conn = mysql_real_connect(
                conn,
                host.c_str(),
                user.c_str(),
                password.c_str(),
                db.c_str(),
                port,
                nullptr,
                0
            );

            connQueue.push(conn);
        }
    }

    MYSQL* getConn()
    {
        std::unique_lock<std::mutex> lock(mtx);

        while(connQueue.empty())
        {
            cv.wait(lock);
        }

        MYSQL* conn = connQueue.front();
        connQueue.pop();
        return conn;
    }

    void releaseConn(MYSQL* conn)
    {
        std::lock_guard<std::mutex> lock(mtx);
        connQueue.push(conn);
        cv.notify_one();
    }
};

#endif