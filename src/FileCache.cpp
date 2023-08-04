//
// Created by xd on 6/8/23.
//

#include <random>
#include <iostream>
#include "FileCache.h"

FileCache::FileCache() {
}

std::shared_ptr<char []> FileCache::get(const std::string &fileName) {
    if (countMap(fileName)) {
//        std::cout<<"大小："<<m_umap.size()<<std::endl;
        return readMap(fileName);
    } else {
        //如果不存在则插入新的数据，然后返回
        if (m_umap.size() < m_maxsize) {
        } else {
            //满了的情况下
            printf("删除一个元素\n");
            // 生成一个随机数，表示要删除的元素的下标
            std::random_device rd;
            std::mt19937 gen(rd());
            int s=m_umap.size();
            std::uniform_int_distribution<> dis(0, m_umap.size()-1);
            int randomIndex = dis(gen);
            // 将迭代器指向要删除的元素
            auto it = std::next(m_umap.begin(), randomIndex);
            m_umap.erase(it);
        }
        //这里插入元素
        writeMap(fileName);
        return readMap(fileName);
    }
    return nullptr;
}

FileCache &FileCache::getInstance() {
    static FileCache instance;
    return instance;
}

void FileCache::writeMap(const std::string &fileName) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_umap.insert({fileName, Filedata(fileName)});
}

int FileCache::countMap(const std::string &fileName) {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    int res = m_umap.count(fileName);
    return res;
}

std::shared_ptr<char []> FileCache::readMap(const std::string &fileName) {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto res = m_umap.at(fileName).getSharedPtrFileData();
    return res;
}
