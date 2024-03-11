#pragma once

#include "../client.hpp"

class Dir {
 public:
	explicit Dir(std::string& path);

	~Dir() = default;

	void list_files(std::list<std::filesystem::path>&);

	int64_t get_file(std::filesystem::path&, std::vector<char>&, uint64_t);

	void set_dir(std::filesystem::path);

 private:
	static int64_t copy(std::filesystem::path&, std::vector<char>&, uint64_t);

	std::filesystem::path dir;
};
