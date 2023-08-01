//
// Created by xd on 6/5/23.
// 线程同步机制封装类
//

#ifndef WEBSERVER_LOCKER_H
#define WEBSERVER_LOCKER_H
#include <pthread.h>

class Locker {
private:
    pthread_mutex_t m_mutex{};
public:
    Locker();
    ~Locker();
    bool lock();
    bool unlock();
    pthread_mutex_t *get();
};


#endif //WEBSERVER_LOCKER_H
