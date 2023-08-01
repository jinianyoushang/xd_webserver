//
// Created by root on 6/7/23.
//

#ifndef WEBSERVER_CONFIG_H
#define WEBSERVER_CONFIG_H
#include "SimpleIni.h"


const std::string configFileName = "../config.ini";
class Config {
    CSimpleIniA ini;
public:
    int MAX_FD = 65535; //支持的最大的文件描述符的个数
    int MAX_EVENT_NUMBER = 10000; //监听事件的最大数量
    int TIMESLOT = 5;   //超时时间有关
    // 网站的根目录
    std::string doc_root = "../resources";
    //主机号
    std::string HOST = "0.0.0.0";
    //端口号
    int PORT = 9999;
    //线程数
    int THREAD_NUMBER = 8;
private:
    //设为禁止外部访问
    Config();

    Config(const Config &) = delete;

    Config &operator=(const Config &) = delete;

public:
    static Config& getInstance();
};

#endif //WEBSERVER_CONFIG_H
