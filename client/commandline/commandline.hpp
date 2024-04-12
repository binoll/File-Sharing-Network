#pragma once

#include "../client.hpp"
#include "../connection/connection.hpp"

class CommandLine {
 public:
	explicit CommandLine(std::string&);

	~CommandLine() = default;

	void run();

 private:
	void exit();

	static void help();

	void getList();

	void getFile(std::string&);

	static std::string parseGetCommand(const std::string&);

	int8_t processingCommand(const std::string&);

	Connection connection;
	std::vector<std::string> commands = {"exit", "help", "list", "get"};
};
