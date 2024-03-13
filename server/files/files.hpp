#pragma once

#include "../server.hpp"
#include "files.hpp"

class Files {
 public:
	Files(const uint64_t, const std::string&, const std::string&);

	void add(const uint64_t, const std::string&, const std::string&);

	std::vector<std::pair<uint64_t, std::string>> find_by_hash(const std::string&);

	std::vector<std::pair<uint64_t, std::string>> find_by_id(const uint64_t);

	void remove_by_id(const uint64_t);

	void remove_by_filename(const std::string&);

 private:
	std::unordered_map<std::string, std::vector<std::pair<uint64_t, std::string>>> list;
};
