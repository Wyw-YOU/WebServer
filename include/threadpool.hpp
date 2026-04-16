#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

/**
 * @brief 简单线程池实现
 * 
 * 用于处理客户端请求任务，工作线程持续从任务队列中取出任务执行。
 */
class ThreadPool
{
private:
    std::vector<std::thread> workers;           ///< 工作线程集合
    std::queue<std::function<void()>> tasks;    ///< 任务队列
    std::mutex queue_mutex;                     ///< 队列互斥锁
    std::condition_variable condition;          ///< 条件变量（用于通知新任务）

public:
    /**
     * @brief 构造函数：创建指定数量的工作线程
     * @param threadNum 线程数量
     */
    ThreadPool(int threadNum)
    {
        for (int i = 0; i < threadNum; i++)
        {
            workers.emplace_back([this]() {
                while (true)
                {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this] {
                            return !tasks.empty();
                        });

                        task = tasks.front();
                        tasks.pop();
                    }

                    task(); // 执行任务
                }
            });
        }
    }

    /**
     * @brief 向线程池添加一个任务
     * @param task 可调用对象（函数、lambda 等）
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

#endif // THREADPOOL_HPP