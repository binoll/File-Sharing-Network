#pragma once

#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <list>
#include <map>

#include <sys/poll.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <dirent.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

const std::vector<std::string> commands_client = {"list", "get", "exit", "error"};
const std::vector<std::string> commands_server = {"list", "get", "part", "exit", "error"};
const std::vector<std::string> marker = {"start: ", ":end"};
