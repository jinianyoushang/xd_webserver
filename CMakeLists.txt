cmake_minimum_required(VERSION 3.10)
project(webserver)

set(CMAKE_CXX_STANDARD 17)
#add_compile_options(-O3)

include_directories(src/
        src/headers
        ext/simpleini-4.19
        ext/concurrentqueue-master
        )


add_executable(webserver
        src/main.cpp
        src/main.cpp
        src/locker.cpp
        src/headers/locker.h
        src/Threadpool.cpp src/headers/Threadpool.h src/HttpConnection.cpp src/headers/HttpConnection.h src/headers/TaskInterface.h src/headers/Lst_timer.h src/headers/Config.h src/FileCache.cpp src/headers/FileCache.h src/Filedata.cpp src/headers/Filedata.h src/Config.cpp)



target_link_libraries(webserver pthread)


add_executable(test_simp_ini  test/test_simp_ini.cpp)
add_executable(testFileCache test/test_file_cache.cpp src/FileCache.cpp src/Filedata.cpp)