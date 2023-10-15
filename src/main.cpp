//
// Created by xd on 6/4/23.
//

#include <iostream>
#include <string>
#include <cstdlib>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cerrno>
#include <sys/epoll.h>
#include <csignal>
#include <cstring>
#include <memory>
#include <unistd.h>
#include <cassert>
#include "Threadpool.h"
#include "HttpConnection.h"
#include "Lst_timer.h"
#include "Config.h"

using namespace std;


sort_timer_lst timer_lst;    //清理超时请求
static int pipefd[2];   //超时时间使用的管道

//信号处理函数 添加信号捕捉
void addSignal(int sig, void (handler)(int)) {
    struct sigaction sa{};
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    int res = sigaction(sig, &sa, nullptr);
    if (res == -1) {
        perror("sigaction");
        exit(-1);
    }
}

void sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char *) &msg, 1, 0);
    errno = save_errno;
}


void timer_handler() {
    // 定时处理任务，实际上就是调用tick()函数
    timer_lst.tick();
    // 因为一次 alarm 调用只会引起一次SIGALARM 信号，所以我们要重新定时，以不断触发 SIGALARM信号。
    alarm(Config::getInstance().TIMESLOT);
}

// 定时器回调函数，它删除非活动连接socket上的注册事件，并关闭之。
void cb_func(HttpConnection *user_data) {
    assert(user_data);
    user_data->close_conn();
}

int main(int argc, char *argv[]) {
#ifdef NDEBUG
    // 非 Debug 模式下的代码
    cout<< "非 Debug 模式"<<endl;
#else
    // Debug 模式下的代码
    cout << "Debug 模式" << endl;
#endif

    //初始化配置文件
    std::cout << "doc_root " << Config::getInstance().doc_root.c_str() << std::endl;
    int port = 0;
    if (argc <= 1) {
        printf("可按照如下格式运行：%s port_number\n", basename(argv[0]));
        port = Config::getInstance().PORT;
    } else {
        port = atoi(argv[1]);
    }
    cout << "获得绑定的端口：" << port << endl;

    //对SIGPIPE信号进行处理 忽略
    addSignal(SIGPIPE, SIG_IGN);
    addSignal(SIGALRM, sig_handler);    //定时器的处理
    addSignal(SIGTERM, sig_handler);    //退出的处理
    bool stop_server = false; //停止服务的运行

    //初始化线程池
    unique_ptr<ThreadPool> pool;
    try {
        pool = make_unique<ThreadPool>(Config::getInstance().THREAD_NUMBER);
    } catch (...) {
        cout << "初始化线程池失败" << endl;
        exit(-1);
    }


    //初始化套接字
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("socket");
        exit(-1);
    }


    //设置端口复用
    int reuse = 1;
    int res = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (-1 == res) {
        perror("setsockopt");
        exit(-1);
    }

    //绑定
    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port); //一定要转换字节序
    res = bind(listenfd, (struct sockaddr *) &address, sizeof(address));
    if (-1 == res) {
        perror("bind");
        exit(-1);
    }

    //监听
    res = listen(listenfd, 16);
    if (-1 == res) {
        perror("listen");
        exit(-1);
    }

    //创建epoll对象，事件数组，添加
    epoll_event events[Config::getInstance().MAX_EVENT_NUMBER];
    int epollfd = epoll_create(10);
    //添加到epoll对象中
    addfd(epollfd, listenfd, false);
    cout << "epollfd:" << epollfd << endl;
    HttpConnection::m_epollfd = epollfd;

    // 创建管道 监听定时事件
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0], false);


    //创建一个数组用来保存所有的客户端信息
    vector<HttpConnection> users(Config::getInstance().MAX_FD);
    bool timeout = false;
    alarm(Config::getInstance().TIMESLOT);  // 定时,5秒后产生SIGALARM信号

    //循环监听事件
    while (!stop_server) {
        int num = epoll_wait(epollfd, events, Config::getInstance().MAX_EVENT_NUMBER, -1);
        if ((num < 0) && (errno != EINTR)) {
            cout << "epooll failure" << endl;
            break;
        }
        //循环遍历事件数组
        for (int i = 0; i < num; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) { //监听的事件
                //有客户端链接进来
                struct sockaddr_in client_address;
                socklen_t client_address_len = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *) &client_address, &client_address_len);
                //判断链接成功


                if (connfd < 0) {
                    perror("accept");
                    continue;
                }

                if (HttpConnection::m_user_count >= Config::getInstance().MAX_FD) {
                    printf("连接数达到上限\n");
                    close(connfd);
                    continue;
                }
                //将新的客户的数据初始化放到数组中
                users[connfd].init(connfd, client_address);
                // 创建定时器，设置其回调函数与超时时间，然后绑定定时器与用户数据，最后将定时器添加到链表timer_lst中
                auto *timer = new util_timer;
                timer->user_data = &users[connfd];
                timer->cb_func = cb_func;
                time_t cur = time(nullptr);
                timer->expire = cur + 3 * Config::getInstance().TIMESLOT;//15s超时
                users[connfd].timer = timer;
                timer_lst.add_timer(timer);
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //对方(异常)断开或者错误事件发生
//                printf("对方断开或者错误事件发生\n");
                users[sockfd].close_conn();
            } else if (sockfd == pipefd[0] && (events[i].events & EPOLLIN)) { // 处理信号
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1) {
                    continue;
                } else if (ret == 0) {
                    continue;
                } else {
                    for (int j = 0; j < ret; ++j) {
                        switch (signals[j]) {
                            case SIGALRM: {
                                // 用timeout变量标记有定时任务需要处理，但不立即处理定时任务
                                // 这是因为定时任务的优先级不是很高，我们优先处理其他更重要的任务。
                                timeout = true;
                                break;
                            }
                            case SIGTERM: {
                                printf("SIGTERM  stop_server\n");
                                stop_server = true;
                            }
                            default: {
                                printf("未处理的情况\n");
                                exit(-1);
                            }
                        }
                    }
                }
            } else if (events[i].events & EPOLLIN) {//检测读事件
                if (users[sockfd].read()) {
                    //一次性读取数据
                    auto func = [&]() {
                        users[sockfd].process();
                    };
                    pool->submit(func);//多线程处理数据
                } else {
//                    printf("read断开连接\n");
                    users[sockfd].close_conn(true);
                }
            } else if (events[i].events & EPOLLOUT) {//检测写事件
                if (!users[sockfd].write()) {
//                    printf("write断开连接\n");
                    users[sockfd].close_conn(true);
                }
            }
        }
        // 最后处理定时事件，因为I/O事件有更高的优先级。当然，这样做将导致定时任务不能精准的按照预定的时间执行。
        if (timeout) {
            timer_handler();
            timeout = false;
        }
    }

    close(epollfd);
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    return 0;
}
