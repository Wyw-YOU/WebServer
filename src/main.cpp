#include "../include/server.hpp"

/*
    程序入口
*/

int main()
{

    // 创建服务器
    Server server(9006,4);

    // 初始化
    if(!server.init())
        return -1;

    // // ===== 初始化测试用户（只执行一次）=====
    // server.userService.login("admin","123") 
    // || server.userService.registerUser("admin","123");

    // 启动服务器
    server.start();

    return 0;
}