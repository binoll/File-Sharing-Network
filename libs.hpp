#pragma once

#include <filesystem>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <string>
#include <thread>
#include <future>
#include <vector>
#include <regex>
#include <mutex>
#include <array>
#include <list>

#include <sys/poll.h>
#include <sys/socket.h>
#include <dirent.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

const std::vector<std::string> commands_client = {"list", "get", "exit"};
const std::vector<std::string> commands_server = {"list", "get", "part", "exit"};
const std::vector<std::string> marker = {"start:", ":end"};
