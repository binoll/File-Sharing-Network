cmake_minimum_required(VERSION 3.27)
project(P2P_network)

set(CMAKE_CXX_STANDARD 23)

add_executable(P2P_network_client client/client.cpp)
add_executable(P2P_network_server server/server.cpp)
