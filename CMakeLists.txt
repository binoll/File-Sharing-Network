cmake_minimum_required(VERSION 3.20)
project(P2P_network)

set(CMAKE_CXX_STANDARD 23)

find_package(OpenSSL REQUIRED)

add_subdirectory(multithreaded_server)
add_subdirectory(client)
add_subdirectory(switch)

find_package(Boost REQUIRED COMPONENTS log)
target_link_libraries(multithreaded_server PRIVATE Boost::log OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(client PRIVATE Boost::log OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(switch pcap)
