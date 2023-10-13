# xd_webserver

自己实现的linux端 web静态资源服务器。

## 使用的技术

1. 线程池  互斥锁，信号量（互斥锁+条件变量实现），请求队列
2. 将线程池中请求队列变为 无锁队列
3. io多路复用  
4. cmake
5. 多线程缓冲 C++11读写锁
6. 单例模式 config 文件缓冲
7. 智能指针 
8. 状态机
9. 使用async重写线程调度
10. 



## 需要作的事情

1. 

## 已经完成的事情

1. 使用async重写线程调度
2. release模式段错误  编译器bug
3. config
4. 使用c++11带的多线程进行改写线程池
5. 使用linux底层的接口，写的线程池比较快
6. 尽量使用c++接口 和智能指针
7. 性能测试提高性能
8. 可以加速静态资源 进行对比  效果很明显
9. C++改写文件处理
10. 多线程访问单例模式，也就是容器。解决多线程访问容器的问题，读写锁。
11. 添加配置文件选项--不够完美
12. 压力测试
13. 修改水平触发为边缘触发  区别不大
14. 错误处理 提示日志
15. 定时检测过期连接并且关闭连接
16. new出来的用智能指针。
17. 连接过多Bad file descriptor
18. 



## 不好做的事情，先不去做





### 记录

#### 发现的结果

单例模式真好用

使用linux底层的接口，写的线程池比较快

#### 性能测试 10s：

Speed=1558026 pages/min, 18774140 bytes/sec.
Requests: 259671 susceed, 0 failed.

单生产者，单消费者

Speed=2074836 pages/min, 25001628 bytes/sec.
Requests: 345806 susceed, 0 failed.

##### 无锁队列+缓存 

Speed=3810942 pages/min, 45921848 bytes/sec.
Requests: 635157 susceed, 0 failed.

##### 真正无锁队列+缓存  1主线程 线程池2个消费者

Speed=3953652 pages/min, 47641576 bytes/sec.
Requests: 658942 susceed, 0 failed.

性能瓶颈可能在主线程上

#### Jmeter

Jmeter测试达到了nginx的水平

#### 记录格式

127.0.0.1 - - [07/Jun/2023 11:47:06] "GET /favicon.ico HTTP/1.1" 200 -