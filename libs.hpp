#pragma once

#include <filesystem>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <regex>
#include <mutex>
#include <array>
#include <list>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define ONE_KB 1024

const std::string START_MSG = "start";
const std::string END_MSG = "end";
struct data {
	int32_t fd;
	sockaddr_in addr;
};
