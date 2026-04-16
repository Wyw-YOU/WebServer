#ifndef MYSQL_POOL_HPP
#define MYSQL_POOL_HPP

#include <mysql/mysql.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>

/**
 * @brief MySQL 连接池
 * 
 * 实现简单的连接复用，线程安全。
 */
class MysqlPool
{
private:
    std::queue<MYSQL*> connQueue;       ///< 连接队列
    std::mutex mtx;                     ///< 互斥锁
    std::condition_variable cv;         ///< 条件变量（用于等待可用连接）

    std::string host;      ///< 数据库主机
    std::string user;      ///< 用户名
    std::string password;  ///< 密码
    std::string db;        ///< 数据库名
    int port;              ///< 端口号

public:
    /**
     * @brief 构造函数：初始化连接池
     * @param size      连接池大小
     * @param host      主机地址
     * @param user      用户名
     * @param password  密码
     * @param db        数据库名称
     * @param port      端口号
     */
    MysqlPool(int size,
              const std::string& host,
              const std::string& user,
              const std::string& password,
              const std::string& db,
              int port)
        : host(host)
        , user(user)
        , password(password)
        , db(db)
        , port(port)
    {
        for (int i = 0; i < size; i++)
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

    /**
     * @brief 获取一个可用连接（若无空闲则阻塞等待）
     * @return MYSQL 连接指针
     */
    MYSQL* getConn()
    {
        std::unique_lock<std::mutex> lock(mtx);
        while (connQueue.empty())
        {
            cv.wait(lock);
        }

        MYSQL* conn = connQueue.front();
        connQueue.pop();
        return conn;
    }

    /**
     * @brief 归还连接到池中
     * @param conn 要归还的连接指针
     */
    void releaseConn(MYSQL* conn)
    {
        std::lock_guard<std::mutex> lock(mtx);
        connQueue.push(conn);
        cv.notify_one();
    }
};

#endif // MYSQL_POOL_HPP