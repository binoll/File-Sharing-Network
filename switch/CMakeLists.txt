cmake_minimum_required(VERSION 3.20)
project(switch)

set(CMAKE_C_STANDARD 23)

add_executable(
        switch
        switch.c
)

target_link_libraries(switch pcap)
