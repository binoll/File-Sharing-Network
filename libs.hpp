// Copyright 2024 binoll
#pragma once

#include <unordered_map>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <string>
#include <thread>
#include <numeric>
#include <vector>
#include <mutex>
#include <list>
#include <map>

#include <sys/poll.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <dirent.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <boost/crc.hpp>
#include <boost/coroutine/all.hpp>

#define BUFFER_SIZE 1024

const std::vector<std::string> commands = {"list", "get", "exit", "error"};
