//
// Created by xd on 6/5/23.
//

#ifndef WEBSERVER_HTTPCONNECTION_H
#define WEBSERVER_HTTPCONNECTION_H

#include <netinet/in.h>
#include <sys/stat.h>
#include <memory>
#include "TaskInterface.h"


class util_timer;   // 前向声明
class HttpConnection : public TaskInterface {
public:
    static const int READ_BUFFER_SIZE = 1024;//读缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 1024;//写缓冲区的大小
    static const int FILENAME_LEN = 200;        // 文件名的最大长度
    // HTTP请求方法，这里只支持GET
    enum class METHOD {
        GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT
    };

    /*
        解析客户端请求时，主状态机的状态
        CHECK_STATE_REQUESTLINE:当前正在分析请求行
        CHECK_STATE_HEADER:当前正在分析头部字段
        CHECK_STATE_CONTENT:当前正在解析请求体
    */
    enum class CHECK_STATE {
        CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT
    };

    /*
        服务器处理HTTP请求的可能结果，报文解析的结果
        NO_REQUEST          :   请求不完整，需要继续读取客户数据
        GET_REQUEST         :   表示获得了一个完成的客户请求
        BAD_REQUEST         :   表示客户请求语法错误
        NO_RESOURCE         :   表示服务器没有资源
        FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
        FILE_REQUEST        :   文件请求,获取文件成功
        INTERNAL_ERROR      :   表示服务器内部错误
        CLOSED_CONNECTION   :   表示客户端已经关闭连接了
    */
    enum class HTTP_CODE {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    // 从状态机的三种可能状态，即行的读取状态，分别表示
    // 1.读取到一个完整的行 2.行出错 3.行数据尚且不完整
    enum class LINE_STATUS {
        LINE_OK = 0, LINE_BAD, LINE_OPEN
    };
    // 定时器
private:
    sockaddr_in m_address; //通信的socket地址
    char m_read_buff[READ_BUFFER_SIZE]; //读缓冲区
    char m_write_buff[READ_BUFFER_SIZE];//写缓冲区
    int m_read_idx; //标识读缓冲区中以及读取的客户端最后一个字节的下一个位置
    int m_write_idx;
    int m_checked_index;    //当前正在处理的字符在读缓冲区的位置
    int m_start_line;   //当前正在解析的行的起始位
    CHECK_STATE m_check_state;  //当前主状态机所处的状态

    //解析出来的数据
    char *m_url;   //请求目标文件的文件名
    char *m_version;   //协议版本 ,只支持HTTP1.1
    METHOD m_method;    //请求方法
    char *m_host; //主机名
    bool m_linger;  //http是否保持连接
    int m_content_length;   // HTTP请求的消息总长度
    int m_checked_idx;  // 写缓冲区中待发送的字节数
    char m_real_file[FILENAME_LEN];       // 客户请求的目标文件的完整路径，其内容等于 doc_root + m_url, doc_root是网站根目录
    struct stat m_file_stat; // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    struct iovec m_iv[2];                   // 我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量。
    int m_iv_count;
    std::shared_ptr<char []> m_file_address;                   // 客户请求的目标文件被mmap到内存中的起始位置

    int bytes_to_send;              // 将要发送的数据的字节数
    int bytes_have_send;            // 已经发送的字节数


    ////下面是方法区域
public:
    static int m_epollfd;//所有socket事件都被注册到一个epollfd中
    static int m_user_count; //统计用户的数量
    int m_sockfd; //当前http链接的socket
    //定时器
    util_timer* timer;


    HttpConnection();

    ~HttpConnection();


    void process() override; //处理客户端请求
    void init(int sockfd, const sockaddr_in &addr); //初始化接收新的请求
    void close_conn(bool delete_timer= false); //关闭连接
    bool read();    //非阻塞读数据
    bool write();    //非阻塞写数据
    void init();    //初始化连接其余的数据
private:
    HTTP_CODE process_read();    //解析http请求
    HTTP_CODE process_request_line(char *);    //解析请求首行
    HTTP_CODE process_headers(char *);    //解析请求头
    HTTP_CODE process_content(char *);    //解析请求体

    LINE_STATUS parse_line();//解析一行
    char *getline() { return m_read_buff + m_start_line; };

    HTTP_CODE do_request();

    void unmap();   // 对内存映射区执行munmap操作
    bool add_response(const char *format, ...);

    bool add_status_line(int status, const char *title);

    bool add_headers(int content_len);

    bool add_content_length(int content_len);

    bool add_linger();

    bool add_blank_line();

    bool add_content(const char *content);

    bool add_content_type();

    bool process_write(HTTP_CODE ret);

    static int get_status_code(HTTP_CODE ret);  //根据HTTP_CODE返回相应状态码
    static std::string get_time(); //获取系统时间
};

/**
 * 添加文件描述符到epoll中
 * @param epollfd
 * @param fd
 * @param one_shot 是否开启 EPOLLONESHOT模式
 * @param et 是否开启边缘触发模式
 */
void addfd(int epollfd, int fd, bool one_shot, bool et = false);

//从epoll中删除文件描述符
void removefd(int epollfd, int fd);

//epoll中修改文件描述符  EPOLLONESHOT被重置
void modifyfd(int epollfd, int fd, int event,bool et = false);

//设置非阻塞
int setnonblocking(int fd);
#endif //WEBSERVER_HTTPCONNECTION_H
