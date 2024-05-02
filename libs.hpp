// Copyright 2024 binoll
#pragma once

#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <string>
#include <thread>
#include <numeric>
#include <vector>
#include <mutex>
#include <list>
#include <map>

#include <sys/socket.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ether.h>

#include <boost/log/trivial.hpp>
#include <boost/crc.hpp>

#include <pcap.h>

#define BUFFER_SIZE 1024

const std::vector<std::string> commands = {"list", "get", "exit", "error", "exist"};
