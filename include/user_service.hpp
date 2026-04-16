#ifndef USER_SERVICE_HPP
#define USER_SERVICE_HPP

#include <mysql/mysql.h>
#include <string>
#include <iostream>
#include <crypt.h>

// 必须包含连接池头文件
#include "mysql_pool.hpp"

/**
 * @brief 用户业务模块
 * 
 * 提供登录、注册功能，包含密码加盐哈希与 SQL 防注入处理。
 */
class UserService
{
private:
    MysqlPool* pool; ///< 数据库连接池指针

public:
    /**
     * @brief 构造函数
     * @param pool 外部传入的连接池
     */
    UserService(MysqlPool* pool)
        : pool(pool)
    {
    }

    /**
     * @brief 使用 SHA-512 加盐哈希密码
     * @param pwd 原始密码
     * @return 哈希后的字符串（含盐值）
     */
    std::string hashPassword(const std::string& pwd)
    {
        std::string salt = "$6$randomsalt$"; // SHA-512 盐值前缀
        char* res = crypt(pwd.c_str(), salt.c_str());
        return std::string(res);
    }

    /**
     * @brief 用户登录验证
     * @param user 用户名
     * @param pwd  密码
     * @return true 登录成功，false 失败
     */
    bool login(const std::string& user, const std::string& pwd)
    {
        MYSQL* conn = pool->getConn();

        // SQL 防注入转义
        char user_safe[256];
        mysql_real_escape_string(conn,
                                 user_safe,
                                 user.c_str(),
                                 user.length());

        std::string sql =
            "SELECT password FROM users WHERE username='" +
            std::string(user_safe) + "'";

        if (mysql_query(conn, sql.c_str()))
        {
            pool->releaseConn(conn);
            return false;
        }

        MYSQL_RES* res = mysql_store_result(conn);
        if (!res)
        {
            pool->releaseConn(conn);
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        if (!row)
        {
            mysql_free_result(res);
            pool->releaseConn(conn);
            return false;
        }

        std::string db_hash = row[0];

        mysql_free_result(res);
        pool->releaseConn(conn);

        // 使用数据库中存储的 hash 作为盐值进行验证
        char* verify = crypt(pwd.c_str(), db_hash.c_str());
        return db_hash == std::string(verify);
    }

    /**
     * @brief 用户注册
     * @param user 用户名
     * @param pwd  密码
     * @return true 注册成功，false 失败（如用户已存在）
     */
    bool registerUser(const std::string& user, const std::string& pwd)
    {
        MYSQL* conn = pool->getConn();

        // SQL 防注入转义
        char user_safe[256];
        mysql_real_escape_string(conn,
                                 user_safe,
                                 user.c_str(),
                                 user.length());

        // 检查用户是否已存在
        std::string check =
            "SELECT * FROM users WHERE username='" +
            std::string(user_safe) + "'";

        mysql_query(conn, check.c_str());
        MYSQL_RES* res = mysql_store_result(conn);

        if (mysql_fetch_row(res))
        {
            mysql_free_result(res);
            pool->releaseConn(conn);
            return false; // 用户已存在
        }
        mysql_free_result(res);

        // 加密密码并插入数据库
        std::string hash = hashPassword(pwd);

        std::string sql =
            "INSERT INTO users(username, password) VALUES('" +
            std::string(user_safe) + "','" + hash + "')";

        bool ok = (mysql_query(conn, sql.c_str()) == 0);

        pool->releaseConn(conn);
        return ok;
    }
};

#endif // USER_SERVICE_HPP