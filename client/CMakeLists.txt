cmake_minimum_required(VERSION 3.27)
project(P2P_network_client)

set(CMAKE_CXX_STANDARD 23)

add_executable(P2P_network_client
        client.cpp
        command_line/command_line.cpp
        connection/connection.cpp
        dir/dir.cpp
)
