cmake_minimum_required(VERSION 3.27)
project(client)

set(CMAKE_CXX_STANDARD 23)

add_executable(
        client
        client.cpp
        CommandLine/commandLine.cpp
        Connection/connection.cpp
)
