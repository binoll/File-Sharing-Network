cmake_minimum_required(VERSION 3.27)
project(server)

set(CMAKE_CXX_STANDARD 23)

add_executable(
        server
        server.cpp
        Connection/connection.cpp
        ThreadPool/threadPool.hpp
)
