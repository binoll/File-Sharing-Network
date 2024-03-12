#pragma once

#include "../client.hpp"

class Dir {
 public:
	// Constructor.
	explicit Dir(const std::string&);

	// Destructor.
	~Dir() = default;

	// Push in list files at current dir.
	int8_t list_files(std::list<std::filesystem::path>&);

	/*
	 * Push in buf part of file.
	 *
	 * Return -2 if EOF.
	 *
	 */
	int64_t get_file(const std::filesystem::path&, std::byte*, int64_t, int64_t);

	// Create file.
	int64_t set_file(const std::filesystem::path&, std::byte*, int64_t);

	// Set work path.
	int8_t set_work_path(const std::string&);

	// Checks if there is a file in the current dir.
	bool is_exist(const std::filesystem::path&);

	// Return current path.
	std::filesystem::path get_work_path();

 private:
	std::filesystem::path work_path;
};
