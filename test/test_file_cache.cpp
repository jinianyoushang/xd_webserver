//
// Created by root on 6/8/23.
//
#include <iostream>
#include <string>
#include <cstdlib>
#include "FileCache.h"

using namespace std;

int main() {
    for (int i = 0; i < 1000; ++i) {
        auto res = FileCache::getInstance().get("config.ini");
        cout << res << endl;

        res = FileCache::getInstance().get("README.md");
        cout << res << endl;

        res = FileCache::getInstance().get("config.ini");
        cout << res << endl;
    }
    return 0;
};