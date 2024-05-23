// Copyright 2024 binoll
#pragma once

#include "../client.hpp"
#include "../connection/connection.hpp"

enum class ConsoleColor {
	Default = 0,
	Black = 30,
	Red = 31,
	Green = 32,
	Yellow = 33,
	Blue = 34,
	Magenta = 35,
	Cyan = 36,
	White = 37
};

class CommandLine {
 public:
	explicit CommandLine(std::string&);

	~CommandLine() = default;

	void run();

 private:
	static void setConsoleColor(const ConsoleColor&);

	static std::string getColorString(const ConsoleColor&);

	static void exit();

	static void help();

	void getList();

	void getFile(std::string&);

	int8_t processingCommand(const std::string&);

	Connection connection;
	std::vector<std::string> commands = {"exit", "help", "list", "get"};
};
