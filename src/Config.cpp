//
// Created by root on 6/11/23.
//
#include "Config.h"
Config::Config() {
    ini.SetUnicode();
    SI_Error rc = ini.LoadFile(configFileName.c_str());
    if (rc < 0) {
        /* handle error */
        printf("ini.LoadFile error\n");
        exit(-1);
    }
    const char *host = ini.GetValue("config", "HOST", "");
    if (!std::string(host).empty()) {
        HOST = host;
    }
    const char *port = ini.GetValue("config", "PORT", "");
    if (!std::string(port).empty()) {
        PORT = atoi(port);
    }
    const char *max_fd = ini.GetValue("config", "MAX_FD", "");
    if (!std::string(max_fd).empty()) {
        MAX_FD = atoi(max_fd);
    }
    const char *max_event_number = ini.GetValue("config", "MAX_EVENT_NUMBER", "");
    if (!std::string(max_event_number).empty()) {
        MAX_EVENT_NUMBER = atoi(max_event_number);
    }
    const char *timeslot = ini.GetValue("config", "TIMESLOT", "");
    if (!std::string(timeslot).empty()) {
        TIMESLOT = atoi(timeslot);
    }
    const char *root = ini.GetValue("config", "DOC_ROOT", "");
    if (!std::string(root).empty()) {
        doc_root = root;
        printf("docroot %s\n", doc_root.c_str());
    }
    const char *thread_num = ini.GetValue("config", "THREAD_NUMBER", "");
    if (!std::string(thread_num).empty()) {
        THREAD_NUMBER = atoi(thread_num);
    }
}

Config &Config::getInstance() {
    static Config instance;
    return instance;
}
