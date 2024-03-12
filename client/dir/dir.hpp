#pragma once

#include "../client.hpp"

class Dir {
 public:
	explicit Dir(const std::string&);

	~Dir() = default;

	int64_t get_file(const std::filesystem::path&, std::byte*, int64_t, int64_t);

	int8_t list_files(std::list<std::filesystem::path>&);

	int8_t set_work_path(const std::string&);

	std::filesystem::path get_path();

 private:
	std::filesystem::path work_path;
};
