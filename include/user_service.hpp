#ifndef USER_SERVICE_HPP
#define USER_SERVICE_HPP

#include <mysql/mysql.h>
#include <string>
#include <iostream>
#include <crypt.h>

// ✅ 必须包含连接池头文件（核心修复）
#include "mysql_pool.hpp"

/*
    用户业务模块
    功能：
    - 登录
    - 注册
    - 密码加盐哈希
    - SQL防注入
*/
class UserService
{
private:
    MysqlPool* pool;  // 数据库连接池

public:
    // 构造函数
    UserService(MysqlPool* pool)
        : pool(pool) {}

    /*
        加盐哈希（SHA-512）
        $6$ 表示 SHA-512
    */
    std::string hashPassword(const std::string& pwd)
    {
        std::string salt = "$6$randomsalt$";

        char* res = crypt(pwd.c_str(), salt.c_str());

        return std::string(res);
    }

    /*
        登录逻辑
    */
    bool login(const std::string& user, const std::string& pwd)
    {
        MYSQL* conn = pool->getConn();

        // ===== SQL防注入 =====
        char user_safe[256];
        mysql_real_escape_string(conn,
                                 user_safe,
                                 user.c_str(),
                                 user.length());

        std::string sql =
            "SELECT password FROM users WHERE username='" +
            std::string(user_safe) + "'";

        if(mysql_query(conn, sql.c_str()))
        {
            pool->releaseConn(conn);
            return false;
        }

        MYSQL_RES* res = mysql_store_result(conn);
        if(!res)
        {
            pool->releaseConn(conn);
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        if(!row)
        {
            mysql_free_result(res);
            pool->releaseConn(conn);
            return false;
        }

        std::string db_hash = row[0];

        mysql_free_result(res);
        pool->releaseConn(conn);

        // 使用数据库hash作为salt验证
        char* verify = crypt(pwd.c_str(), db_hash.c_str());

        return db_hash == std::string(verify);
    }

    /*
        注册逻辑
    */
    bool registerUser(const std::string& user, const std::string& pwd)
    {
        MYSQL* conn = pool->getConn();

        // ===== SQL防注入 =====
        char user_safe[256];
        mysql_real_escape_string(conn,
                                 user_safe,
                                 user.c_str(),
                                 user.length());

        // ===== 检查用户是否存在 =====
        std::string check =
            "SELECT * FROM users WHERE username='" +
            std::string(user_safe) + "'";

        mysql_query(conn, check.c_str());
        MYSQL_RES* res = mysql_store_result(conn);

        if(mysql_fetch_row(res))
        {
            mysql_free_result(res);
            pool->releaseConn(conn);
            return false; // 用户已存在
        }
        mysql_free_result(res);

        // ===== 加密密码 =====
        std::string hash = hashPassword(pwd);

        std::string sql =
            "INSERT INTO users(username,password) VALUES('" +
            std::string(user_safe) + "','" + hash + "')";

        bool ok = mysql_query(conn, sql.c_str()) == 0;

        pool->releaseConn(conn);
        return ok;
    }
};

#endif