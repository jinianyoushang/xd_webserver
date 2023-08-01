//
// Created by root on 6/5/23.
//

#ifndef WEBSERVER_TASKINTERFACE_H
#define WEBSERVER_TASKINTERFACE_H
// 用到线程池的类要实现里面的方法
class TaskInterface {
public:
    virtual void process() = 0;
};

#endif //WEBSERVER_TASKINTERFACE_H
