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
#include_next "concurrentqueue.h"

//线程池类，定义成模板类是为了代码的复用，模板参数T是任务类
//目前是单生产者多消费者
template<typename T>
class Threadpool {
private:
    //线程的数量
    int m_thread_number;
    //线程池数组,
    std::vector<pthread_t> m_threads;
    //请求队列中最多允许的，等待处理的请求数量
    int m_max_requests;
    //请求队列  修改为无锁队列
//    moodycamel::BlockingConcurrentQueue<T *> m_workqueue; //有锁队列
    moodycamel::ConcurrentQueue<T *> m_workqueue;
    //是否结束线程
    bool m_stop{false};

    static void *worker(void *arg);

    void run();

public:
    explicit Threadpool(int mThreadNumber = 8, int mMaxRequests = 10000);

    ~Threadpool();

    //添加任务
    bool append(T *request);
};

template<typename T>
void Threadpool<T>::run() {
    while (!m_stop) {
        T *request;
        //使用无锁队列
        while (!m_workqueue.try_dequeue(request)) {
            sched_yield();//让出时间片
        }
        //开始处理任务
        if (!request) {
            continue;
        }
        request->process();//每个任务都要定义process
    }
}

template<typename T>
Threadpool<T>::Threadpool(int mThreadNumber, int mMaxRequests) : m_thread_number(mThreadNumber),
                                                                 m_max_requests(mMaxRequests) {
    if (mThreadNumber <= 0 || mMaxRequests <= 0) {
        throw std::exception();
    }
    m_threads.resize(mThreadNumber);
    for (int i = 0; i < m_thread_number; ++i) {
        std::cout << "create the " << i << "th thread" << std::endl;
        if (pthread_create(&m_threads[i], nullptr, worker, this) != 0) {
            throw std::exception();
        }
        if (pthread_detach(m_threads[i])) {
            throw std::exception();
        }
    }

}

template<typename T>
Threadpool<T>::~Threadpool() {
    m_stop = true;
}

template<typename T>
void *Threadpool<T>::worker(void *arg) {
    auto pool = static_cast<Threadpool *>(arg);
    pool->run();
    return pool;
}

template<typename T>
bool Threadpool<T>::append(T *request) {
    if (m_workqueue.size_approx() > m_max_requests) {
        return false;
    }

#ifndef NDEBUG
    //如果在debug模式
    //判断队列大小从而确定是生产者瓶颈还是消费者瓶颈
    static long long times = 0;
    times++;
    if (times % 5000 == 0) {
        printf("queue size(): %d  times: %d\n", m_workqueue.size_approx(), times);
    }
#endif

    m_workqueue.enqueue(request);
    return true;
}

#endif //WEBSERVER_THREADPOOL_H
