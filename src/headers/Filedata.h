//
// Created by root on 6/8/23.
// 一个文件的内存对象

#ifndef WEBSERVER_FILEDATA_H
#define WEBSERVER_FILEDATA_H

#include <string>
#include <ostream>
#include <memory>

class Filedata {

    std::shared_ptr<char []> m_fileData;
    size_t m_len=0;
public:
    Filedata();
     Filedata(const std::string &fileName);
    ~Filedata();

    char *getFileData() const;
    size_t getFileLen() const;
    std::shared_ptr<char []> getSharedPtrFileData() const;
};

#endif //WEBSERVER_FILEDATA_H
