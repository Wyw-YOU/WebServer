#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

/*
    简单线程池实现
    用于处理客户端请求任务
*/

class ThreadPool
{
private:

    std::vector<std::thread> workers;     // 工作线程

    std::queue<std::function<void()>> tasks; // 任务队列

    std::mutex queue_mutex;               // 队列锁

    std::condition_variable condition;    // 条件变量

public:

    /*
        构造函数
        创建指定数量线程
    */
    ThreadPool(int threadNum)
    {
        for(int i=0;i<threadNum;i++)
        {
            workers.emplace_back([this](){

                while(true)
                {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);

                        condition.wait(lock,[this]{
                            return !tasks.empty();
                        });

                        task = tasks.front();
                        tasks.pop();
                    }

                    task();
                }

            });
        }
    }

    /*
        添加任务
    */
    void addTask(std::function<void()> task)
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            tasks.push(task);
        }

        condition.notify_one();
    }

};

#endif