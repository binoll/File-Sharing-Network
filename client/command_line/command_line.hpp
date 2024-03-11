#pragma once

#include "../client.hpp"
#include "../connection/connection.hpp"
#include "../dir/dir.hpp"

class CommandLine {
 public:
	CommandLine();

	~CommandLine();

	void run();

 private:
	void exit();

	void help();

	int8_t processing_command(const std::string&);

	Connection connection;
	std::vector<std::string> commands = {"exit", "help", "list", "get_file"};
};
