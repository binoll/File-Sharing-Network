cmake_minimum_required(VERSION 3.20)
project(P2P_network)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(multithreaded_server)
add_subdirectory(client)
