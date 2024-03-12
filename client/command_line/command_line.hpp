#pragma once

#include "../client.hpp"
#include "../connection/connection.hpp"
#include "../dir/dir.hpp"

class CommandLine {
 public:
	explicit CommandLine(std::string&);

	~CommandLine() = default;

	void run();

	int8_t update_dir(const std::string&);

 private:
	void exit();

	void help();

	int8_t processing_command(const std::string&);

	Connection connection;
	std::vector<std::string> commands = {"exit", "help", "list", "get"};
};
