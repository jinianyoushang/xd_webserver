//
// Created by root on 6/5/23.
//

#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cerrno>
#include <fcntl.h>
#include <cstring>
#include <sys/stat.h>
#include <sys/uio.h>
#include <cstdarg>
#include <chrono>
#include "HttpConnection.h"
#include "Lst_timer.h"
#include "Config.h"
#include "FileCache.h"

int HttpConnection::m_epollfd = -1;
int HttpConnection::m_user_count = 0;
// 定义HTTP响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file from this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the requested file.\n";


extern sort_timer_lst timer_lst;    //清理超时请求

HttpConnection::HttpConnection() {

}

HttpConnection::~HttpConnection() {

}

//由线程池中的工作线程调用，处理http请求的入口
void HttpConnection::process() {
    //解析http请求
    HTTP_CODE read_ret = process_read();
    if (read_ret == HTTP_CODE::NO_REQUEST) {
        modifyfd(m_epollfd, m_sockfd, EPOLLIN, true);
        return;
    }
//    std::cout << "解析请求成功" << std::endl;
    //生成响应
    bool write_ret = process_write(read_ret);
    if (!write_ret) {
        close_conn();
    }
    //记录请求记录
//    std::cout << "生成响应成功" << std::endl;
    int status_code = get_status_code(read_ret);
    // TODO 打开日志
//    printf("%s -- [%s] \"%s %s %s\" %d -\n", m_host, get_time().c_str(), "GET", m_url, m_version, status_code);
    modifyfd(m_epollfd, m_sockfd, EPOLLOUT, true);
}

void HttpConnection::init(int sockfd, const sockaddr_in &addr) {
    m_sockfd = sockfd;
    m_address = addr;
    //设置端口复用
    int reuse = 1;
    int res = setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (-1 == res) {
        perror("setsockopt");
        exit(-1);
    }

    //添加到epoll对象中
    addfd(m_epollfd, m_sockfd, true, true);
    m_user_count++;
    init();
}

void HttpConnection::close_conn(bool delete_timer) {
//    printf("close_conn 关闭连接\n");
    if (m_sockfd != -1) {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
    if (delete_timer && timer) {
        timer_lst.del_timer(timer);
        timer = nullptr;
    }
}

//读取数据，在主线程中执行
//读到数据成功，其他失败
bool HttpConnection::read() {
    if (m_read_idx >= READ_BUFFER_SIZE) {
        return false;
    }
    int bytes_read = 0;
    while (true) {
        bytes_read = recv(m_sockfd, m_read_buff + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                //没有数据
                break;
            }
            return false;
        } else if (bytes_read == 0) {
            //对方关闭链接
            return false;
        }
        m_read_idx += bytes_read;
    }
    //    printf("读取到的数据：\n%s", m_read_buff);
    //调整定时器
    // 如果某个客户端上有数据可读，则我们要调整该连接对应的定时器，以延迟该连接被关闭的时间。
    if (timer) {
        time_t cur = time(nullptr);
        timer->expire = cur + 3 * Config::getInstance().TIMESLOT;
        timer_lst.adjust_timer(timer);
    }
    return true;
}

//发送数据，在主线程中执行
bool HttpConnection::write() {
    int temp = 0;

    if (bytes_to_send == 0) {
        // 将要发送的字节为0，这一次响应结束。
        modifyfd(m_epollfd, m_sockfd, EPOLLIN, true);
        init();
        return true;
    }

    while (true) {
        // 分散写
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp <= -1) {
            printf("TCP写缓冲区不够\n");
            // 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
            // 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性。
            if (errno == EAGAIN) {
                modifyfd(m_epollfd, m_sockfd, EPOLLOUT, true);
                return true;
            }
            return false;
        }
        //这样是为了解决写入的bug
        bytes_have_send += temp;
        bytes_to_send -= temp;

        if (bytes_have_send >= m_iv[0].iov_len) {
            //头已经发送完毕
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address.get() + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        } else {
            //如果没有发送完毕，还要修改下次写数据的位置：
            m_iv[0].iov_base = m_write_buff + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - temp;
        }

        if (bytes_to_send <= 0) {
            // 没有数据要发送了
            modifyfd(m_epollfd, m_sockfd, EPOLLIN, true);

            if (m_linger) {
                init();
                return true;
            } else {
                return false;
            }
        }

    }

}

//主状态机
HttpConnection::HTTP_CODE HttpConnection::process_read() {
    LINE_STATUS line_status = LINE_STATUS::LINE_OK;
    HTTP_CODE ret = HTTP_CODE::NO_REQUEST;
    char *text = nullptr;
    //解析到了一行完整的数据，或者解析到了请求体，也是完整的数据
    while ((m_check_state == CHECK_STATE::CHECK_STATE_CONTENT) && (line_status == LINE_STATUS::LINE_OK) ||
           (line_status = parse_line()) == LINE_STATUS::LINE_OK) {
        //获取一行数据
        text = getline();
        m_start_line = m_checked_index;
//        printf("got 1 http line:%s\n", text);
        switch (m_check_state) {
            case CHECK_STATE::CHECK_STATE_REQUESTLINE: {
                ret = process_request_line(text);
                switch (ret) {
                    case HTTP_CODE::NO_REQUEST:
                        break;
                    case HTTP_CODE::GET_REQUEST:
                        break;
                    case HTTP_CODE::BAD_REQUEST: {
                        return HTTP_CODE::BAD_REQUEST;
                    }
                        break;
                    case HTTP_CODE::NO_RESOURCE:
                        break;
                    case HTTP_CODE::FORBIDDEN_REQUEST:
                        break;
                    case HTTP_CODE::FILE_REQUEST:
                        break;
                    case HTTP_CODE::INTERNAL_ERROR:
                        break;
                    case HTTP_CODE::CLOSED_CONNECTION:
                        break;
                }
            }
                break;
            case CHECK_STATE::CHECK_STATE_HEADER: {
                ret = process_headers(text);
                switch (ret) {
                    case HTTP_CODE::NO_REQUEST:
                        break;
                    case HTTP_CODE::GET_REQUEST: {
                        return do_request();
                    }
                        break;
                    case HTTP_CODE::BAD_REQUEST: {
                        return HTTP_CODE::BAD_REQUEST;
                    }
                        break;
                    case HTTP_CODE::NO_RESOURCE:
                        break;
                    case HTTP_CODE::FORBIDDEN_REQUEST:
                        break;
                    case HTTP_CODE::FILE_REQUEST:
                        break;
                    case HTTP_CODE::INTERNAL_ERROR:
                        break;
                    case HTTP_CODE::CLOSED_CONNECTION:
                        break;
                }
            }
                break;
            case CHECK_STATE::CHECK_STATE_CONTENT: {
                ret = process_content(text);
                switch (ret) {
                    case HTTP_CODE::NO_REQUEST:
                        break;
                    case HTTP_CODE::GET_REQUEST: {
                        return do_request();
                    }
                        break;
                    case HTTP_CODE::BAD_REQUEST: {
                        return HTTP_CODE::BAD_REQUEST;
                    }
                        break;
                    case HTTP_CODE::NO_RESOURCE:
                        break;
                    case HTTP_CODE::FORBIDDEN_REQUEST:
                        break;
                    case HTTP_CODE::FILE_REQUEST:
                        break;
                    case HTTP_CODE::INTERNAL_ERROR:
                        break;
                    case HTTP_CODE::CLOSED_CONNECTION:
                        break;
                }
                line_status = LINE_STATUS::LINE_OPEN;
            }
                break;
            default: {
                return HTTP_CODE::INTERNAL_ERROR;
            }
                break;
        }
    }
    return HTTP_CODE::NO_REQUEST;
}

HttpConnection::HTTP_CODE HttpConnection::process_request_line(char *text) {
    //GET /index.html HTTP/1.1
    //获取请求方法
    m_url = strpbrk(text, " \t");
    if (!m_url) {
        return HTTP_CODE::BAD_REQUEST;
    }
    *m_url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0) {
        m_method = METHOD::GET;
    } else {
        return HTTP_CODE::BAD_REQUEST;
    }

    //获取请求协议
    // 检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。
    m_version = strpbrk(m_url, " \t");
    if (!m_version) {
        return HTTP_CODE::BAD_REQUEST;
    }
    *m_version++ = '\0';
    if (strcasecmp(m_version, "HTTP/1.1") != 0) {
        return HTTP_CODE::BAD_REQUEST;
    }

    //获取请求路径
    if (strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;
        m_url = strchr(m_url, '/');
    } else if (strncasecmp(m_url, "https://", 8) == 0) {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/') {
        return HTTP_CODE::BAD_REQUEST;
    }

    m_check_state = CHECK_STATE::CHECK_STATE_HEADER;//检查请求头
    return HTTP_CODE::NO_REQUEST;
}

HttpConnection::HTTP_CODE HttpConnection::process_headers(char *text) {
    // 遇到空行，表示头部字段解析完毕
    if (text[0] == '\0') {
        // 如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，
        // 状态机转移到CHECK_STATE_CONTENT状态
        if (m_content_length != 0) {
            m_check_state = CHECK_STATE::CHECK_STATE_CONTENT;
            return HTTP_CODE::NO_REQUEST;
        }
        // 否则说明我们已经得到了一个完整的HTTP请求
        return HTTP_CODE::GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        // 处理Connection 头部字段  Connection: keep-alive
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        }
    } else if (strncasecmp(text, "Content-Length:", 15) == 0) {
        // 处理Content-Length头部字段
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        // 处理Host头部字段
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    } else {
        //TODO 处理各种header
//        printf("oop! unknow header %s\n", text);
    }
    return HTTP_CODE::NO_REQUEST;
}

// 我们没有真正解析HTTP请求的消息体，只是判断它是否被完整的读入了
HttpConnection::HTTP_CODE HttpConnection::process_content(char *text) {
    if (m_read_idx >= (m_content_length + m_checked_idx)) {
        text[m_content_length] = '\0';
        return HTTP_CODE::GET_REQUEST;
    }
    return HTTP_CODE::NO_REQUEST;
}

//解析一行 一行的结束标志/r/n
HttpConnection::LINE_STATUS HttpConnection::parse_line() {
    char temp;
    for (; m_checked_index < m_read_idx; ++m_checked_index) {
        temp = m_read_buff[m_checked_index];
        if (temp == '\r') {
            if ((m_checked_index + 1) == m_read_idx) {
                return LINE_STATUS::LINE_OPEN;
            } else if (m_read_buff[m_checked_index + 1] == '\n') {
                m_read_buff[m_checked_index++] = '\0';
                m_read_buff[m_checked_index++] = '\0';
                return LINE_STATUS::LINE_OK;
            }
            return LINE_STATUS::LINE_BAD;
        } else if (temp == '\n') {
            if ((m_checked_index > 1) && (m_read_buff[m_checked_index - 1]) == '\r') {
                m_read_buff[m_checked_index - 1] = '\0';
                m_read_buff[m_checked_index++] = '\0';
                return LINE_STATUS::LINE_OK;
            }
            return LINE_STATUS::LINE_BAD;
        }
    }
    return LINE_STATUS::LINE_OPEN;
}

void HttpConnection::init() {
    bytes_to_send = 0;
    bytes_have_send = 0;

    m_check_state = CHECK_STATE::CHECK_STATE_REQUESTLINE;
    m_checked_index = 0;
    m_start_line = 0;
    m_read_idx = 0;
    m_method = METHOD::GET;
    m_url = nullptr;
    m_version = nullptr;
    bzero(m_read_buff, READ_BUFFER_SIZE);
    m_host = nullptr;
    m_linger = false;
    m_content_length = 0;
    m_checked_idx = 0;
    m_write_idx = 0;

    bzero(m_read_buff, READ_BUFFER_SIZE);
    bzero(m_write_buff, WRITE_BUFFER_SIZE);
    bzero(m_real_file, FILENAME_LEN);
}

// 当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性，
// 如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其
// 映射到内存地址m_file_address处，并告诉调用者获取文件成功
HttpConnection::HTTP_CODE HttpConnection::do_request() {
    // "/home/nowcoder/webserver/resources"
    strcpy(m_real_file, Config::getInstance().doc_root.c_str());
    int len = strlen(Config::getInstance().doc_root.c_str());
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    // 获取m_real_file文件的相关的状态信息，-1失败，0成功
    if (stat(m_real_file, &m_file_stat) < 0) {
        return HTTP_CODE::NO_RESOURCE;
    }

    // 判断访问权限
    if (!(m_file_stat.st_mode & S_IROTH)) {
        return HTTP_CODE::FORBIDDEN_REQUEST;
    }

    // 判断是否是目录
    if (S_ISDIR(m_file_stat.st_mode)) {
        return HTTP_CODE::BAD_REQUEST;
    }

    //获取文件指针
    m_file_address = FileCache::getInstance().get(m_real_file);
    return HTTP_CODE::FILE_REQUEST;
}


//设置非阻塞
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//添加文件描述符到epoll中
void addfd(int epollfd, int fd, bool one_shot, bool et) {
    epoll_event event{};
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;
    if (one_shot) {
        event.events = event.events | EPOLLONESHOT;
    }

    if (et) {
        event.events = event.events | EPOLLET;
    }
    int res = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    if (-1 == res) {
        perror("epoll_ctl add");
        exit(-1);
    }
    //设置文件描述符非阻塞
    setnonblocking(fd);
}


//从epoll中删除文件描述符
void removefd(int epollfd, int fd) {
    int res = epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
    if (-1 == res) {
        perror("epoll_ctl del");
        exit(-1);
    }
    close(fd);
}


void modifyfd(int epollfd, int fd, int ev, bool et) {
    if (fd == -1) {
        return;
    }
    epoll_event event{};
    event.data.fd = fd;
    event.events = ev | EPOLLRDHUP | EPOLLONESHOT;
    if (et) {
        event.events = event.events | EPOLLET;
    }
    int res = epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
    if (-1 == res) {
        perror("epoll_ctl mod");
//        exit(-1);
    }
}

// 往写缓冲中写入待发送的数据
bool HttpConnection::add_response(const char *format, ...) {
    if (m_write_idx >= WRITE_BUFFER_SIZE) {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buff + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    return true;
}

bool HttpConnection::add_status_line(int status, const char *title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpConnection::add_headers(int content_len) {
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
}

bool HttpConnection::add_content_length(int content_len) {
    return add_response("Content-Length: %d\r\n", content_len);
}

bool HttpConnection::add_linger() {
    return add_response("Connection: %s\r\n", m_linger ? "keep-alive" : "close");
}

bool HttpConnection::add_blank_line() {
    return add_response("%s", "\r\n");
}

bool HttpConnection::add_content(const char *content) {
    return add_response("%s", content);
}

bool HttpConnection::add_content_type() {
    return add_response("Content-Type:%s\r\n", "text/html");
}

// 根据服务器处理HTTP请求的结果，决定返回给客户端的内容
bool HttpConnection::process_write(HTTP_CODE ret) {
    switch (ret) {
        case HTTP_CODE::INTERNAL_ERROR:
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if (!add_content(error_500_form)) {
                return false;
            }
            break;
        case HTTP_CODE::BAD_REQUEST:
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if (!add_content(error_400_form)) {
                return false;
            }
            break;
        case HTTP_CODE::NO_RESOURCE:
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if (!add_content(error_404_form)) {
                return false;
            }
            break;
        case HTTP_CODE::FORBIDDEN_REQUEST:
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if (!add_content(error_403_form)) {
                return false;
            }
            break;
        case HTTP_CODE::FILE_REQUEST:
            add_status_line(200, ok_200_title);
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buff;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address.get();
            m_iv[1].iov_len = m_file_stat.st_size; //文件大小在这里获取
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        default:
            return false;
    }

    m_iv[0].iov_base = m_write_buff;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

int HttpConnection::get_status_code(HTTP_CODE ret) {
    switch (ret) {
        case HTTP_CODE::NO_REQUEST:
            return 204;
            break;
        case HTTP_CODE::GET_REQUEST:
            return 200;
            break;
        case HTTP_CODE::BAD_REQUEST:
            return 400;
            break;
        case HTTP_CODE::NO_RESOURCE:
            return 404;
            break;
        case HTTP_CODE::FORBIDDEN_REQUEST:
            return 403;
            break;
        case HTTP_CODE::FILE_REQUEST:
            return 200;
            break;
        case HTTP_CODE::INTERNAL_ERROR:
            return 500;
            break;
        default:
            return 200;
    }
    return -1;
}

std::string HttpConnection::get_time() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm *tm = std::localtime(&t);
    char buf[512];
    std::string res;
    //日期
    sprintf(buf, "%d-%02d-%02d ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    res += buf;
    //时间
    sprintf(buf, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
    res += buf;
    return res;
}
