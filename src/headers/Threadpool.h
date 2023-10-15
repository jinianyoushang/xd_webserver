//
// Created by root on 6/5/23.
// 线程池类

#ifndef WEBSERVER_THREADPOOL_H
#define WEBSERVER_THREADPOOL_H

#include <pthread.h>
#include <exception>
#include <vector>
#include <list>
#include <iostream>
#include <string>
#include <functional>
#include <condition_variable>
#include <future>
#include "blockingconcurrentqueue.h"

class ThreadPool {
private:
    class ThreadWorker // 内置线程工作类
    {
    private:
        int m_id; // 工作id
        ThreadPool *m_pool; // 所属线程池
    public:
        // 构造函数
        ThreadWorker(ThreadPool *pool, const int id) : m_pool(pool), m_id(id) {
        }

        // 重载()操作
        void operator()() {
            std::function<void()> func = nullptr; // 定义基础函数类func
            while (!m_pool->m_shutdown) {
                // 取出任务队列中的元素
                m_pool->m_queue.wait_dequeue(func);
                // 如果成功取出，执行工作函数
                if (func)
                    func();
            }
        }
    };

    bool m_shutdown; // 线程池是否关闭
    int m_max_requests;

    moodycamel::BlockingConcurrentQueue<std::function<void()>> m_queue; // 执行函数安全队列，即任务队列
    std::vector<std::thread> m_threads; // 工作线程队列
private:
    // Waits until threads finish their current task and shutdowns the pool
    void shutdown() {
        m_shutdown = true;
        for (int i = 0; i < m_threads.size(); ++i) {
            if (m_threads.at(i).joinable()) // 判断线程是否在等待
            {
                m_threads.at(i).join(); // 将线程加入到等待队列
            }
        }
    }

    // Inits thread pool
    void init() {
        for (int i = 0; i < m_threads.size(); ++i) {
            m_threads.at(i) = std::thread(ThreadWorker(this, i)); // 分配工作线程
        }
    }

public:
    // 线程池构造函数
    explicit ThreadPool(const int n_threads = 4, const int max_requests = 65535)
            : m_threads(std::vector<std::thread>(n_threads)), m_shutdown(false), m_max_requests(max_requests) {
        init();
    }

    ~ThreadPool() {
        shutdown();
    }

    ThreadPool(const ThreadPool &) = delete;

    ThreadPool(ThreadPool &&) = delete;

    ThreadPool &operator=(const ThreadPool &) = delete;

    ThreadPool &operator=(ThreadPool &&) = delete;


    // Submit a function to be executed asynchronously by the pool
    template<typename F, typename... Args>
    auto submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))> {
        //TODO 使用异常控制
//        if (m_queue.size_approx() > m_max_requests) {
//            return false;
//        }

        // Create a function with bounded parameter ready to execute
        std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f),
                                                               std::forward<Args>(args)...); // 连接函数和参数定义，特殊函数类型，避免左右值错误

        // Encapsulate it into a shared pointer in order to be able to copy construct
        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

        // Warp packaged task into void function
        std::function<void()> warpper_func = [task_ptr]() {
            (*task_ptr)();
        };
#ifndef NDEBUG
        //如果在debug模式
        //判断队列大小从而确定是生产者瓶颈还是消费者瓶颈
        static long long times = 0;
        times++;
        if (times % 10000 == 0) {
            printf("queue size(): %d  times: %lld\n", m_queue.size_approx(), times);
        }
#endif
        m_queue.enqueue(warpper_func);

        // 返回先前注册的任务指针
        return task_ptr->get_future();
    }
};


#endif //WEBSERVER_THREADPOOL_H
