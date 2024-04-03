#pragma once

#include "../client.hpp"
#include "../Connection/connection.hpp"

class CommandLine {
 public:
	explicit CommandLine(std::string&);

	~CommandLine() = default;

	void run();

 private:
	static void exit();

	static void help();

	int8_t processing_command(const std::string&);

	Connection connection;
	std::vector<std::string> commands = {"exit", "help", "list", "get"};
};
