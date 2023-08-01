//
// Created by root on 6/8/23.
//
#include <cstring>
#include <iostream>
#include "Filedata.h"

Filedata::~Filedata() {
}

Filedata::Filedata(const std::string &fileName) {
    // 打开文件
    FILE *fp = std::fopen(fileName.c_str(), "r");
    if (!fp) {
        std::perror("fopen");
        exit(-1);
    }
    printf("open file %s\n",fileName.c_str());
    // 读取文件内容
    std::fseek(fp, 0, SEEK_END);
    long fileSize = std::ftell(fp);
    m_len = fileSize;
    std::rewind(fp);
    m_fileData = std::shared_ptr<char[]>(new char[fileSize]);
    std::fread(m_fileData.get(), 1, fileSize, fp);

//    // 输出文件内容
//    std::printf("%.*s", (int)fileSize, fileData);
    // 关闭文件
    std::fclose(fp);
}



char *Filedata::getFileData() const {
    return m_fileData.get();
}

size_t Filedata::getFileLen() const {
    return m_len;
}

std::shared_ptr<char[]> Filedata::getSharedPtrFileData() const {
    return m_fileData;
}

Filedata::Filedata() {
}
