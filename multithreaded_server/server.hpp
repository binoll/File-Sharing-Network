// Copyright 2024 binoll
#pragma once

#include "../libs.hpp"

#define BACKLOG 100

struct FileInfo {
	int64_t size;
	std::string hash;
	std::string current_filename;
	std::string filename;
	bool is_filename_changed;
	bool is_filename_indexed;
};
