//
// Created by xd on 6/8/23.
//

#include <random>
#include <iostream>
#include "FileCache.h"


FileCache::FileCache() {
}

std::shared_ptr<char []> FileCache::get(const std::string &fileName) {
    try {
        return readMap(fileName);
    }catch (const char *msg) {
        std::cerr << "ERROR:" << msg << std::endl;
        //这里插入元素
        writeMap(fileName);
    }
    return readMap(fileName);
}

FileCache &FileCache::getInstance() {
    static FileCache instance;
    return instance;
}

void FileCache::writeMap(const std::string &fileName) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_lruCache.put(fileName, Filedata(fileName));
}

std::shared_ptr<char []> FileCache::readMap(const std::string &fileName) {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto res = m_lruCache.get(fileName);
    return res.getSharedPtrFileData();
}
