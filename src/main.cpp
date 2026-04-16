/**
 * @file main.cpp
 * @brief 程序入口，创建并启动 HTTP 服务器
 */

 #include "../include/server.hpp"

 /**
  * @brief 主函数
  * @return 0 正常退出，-1 初始化失败
  */
 int main()
 {
     // 创建服务器实例：监听端口 9006，线程池大小 4
     Server server(9006, 4);
 
     // 初始化服务器（创建 socket、epoll 等）
     if (!server.init())
         return -1;
 
     // ===== 初始化测试用户（仅需执行一次）=====
     // server.userService.login("admin", "123")
     //     || server.userService.registerUser("admin", "123");
 
     // 启动事件循环，开始处理客户端请求
     server.start();
 
     return 0;
 }