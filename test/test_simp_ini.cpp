//
// Created by root on 6/8/23.
//
#include <iostream>
#include <string>
#include <cstdlib>
#include <SimpleIni.h>

int main() {
    CSimpleIniA ini;
    ini.SetUnicode();
    SI_Error rc=ini.LoadFile("config.ini");
    if (rc < 0) { /* handle error */ };

    const char *host = ini.GetValue("database", "host", "");


    if (std::string(host).empty() ) {
        std::cerr << "config file error: database configuration is not complete" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "host: " << host << std::endl;
    return EXIT_SUCCESS;
}