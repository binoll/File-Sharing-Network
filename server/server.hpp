#pragma once

#include "../libs.hpp"

#define BACKLOG 100

struct FileInfo {
	uint64_t size;
	std::string hash;
	std::string filename;
};
