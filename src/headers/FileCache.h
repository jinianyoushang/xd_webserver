//
// Created by root on 6/8/23.
//  用来cache已经访问的文件，防止重复读取硬盘
//  使用读写锁处理多线程访问的问题

#ifndef WEBSERVER_FILECACHE_H
#define WEBSERVER_FILECACHE_H

#include <unordered_map>
#include <shared_mutex>
#include "Filedata.h"

//使用单例模型去实现
class FileCache {
private:
    const size_t m_maxsize = 100;   //容器最大值
    std::unordered_map<std::string,Filedata> m_umap{m_maxsize}; //保存数据的容器
    std::shared_mutex m_mutex;
    //设为禁止外部访问
    FileCache();
    FileCache(const FileCache&)=delete;
    FileCache& operator=(const FileCache&)=delete;
    void writeMap(const std::string &fileName);   //加上读写锁写入map
    int countMap(const std::string &fileName);    //加上读写锁countmap
    std::shared_ptr<char []> readMap(const std::string &fileName);   //加上读写锁readmap

public:
    static FileCache& getInstance();
    std::shared_ptr<char []> get(const std::string& fileName);
};


#endif //WEBSERVER_FILECACHE_H
