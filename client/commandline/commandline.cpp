// Copyright 2024 binoll
#include "commandline.hpp"

CommandLine::CommandLine(std::string& path) : connection(path) {
	std::cout << "+=========================+" << std::endl;
	std::cout << "|   Welcome to the file   |" << std::endl;
	std::cout << "|      sharing app!       |" << std::endl;
	std::cout << "+=========================+" << std::endl;

	while (true) {
		std::cout << "[*] List of console colors:" << std::endl;
		std::cout << "1. Black" << std::endl;
		std::cout << "2. Red" << std::endl;
		std::cout << "3. Green" << std::endl;
		std::cout << "4. Yellow" << std::endl;
		std::cout << "5. Blue" << std::endl;
		std::cout << "6. Magenta" << std::endl;
		std::cout << "7. Cyan" << std::endl;
		std::cout << "8. White" << std::endl;
		std::cout << "[*] Enter text color: ";

		int choice;
		std::cin >> choice;

		if (choice >= 1 && choice <= 8) {
			auto color = static_cast<ConsoleColor>(choice + static_cast<int>(ConsoleColor::Black) - 1);
			setConsoleColor(color);
			std::cout << "[+] Success: You chose color is " << getColorString(color) << '.' << std::endl;
			break;
		} else {
			std::cout << "[-] Error: Invalid color choice." << std::endl;
			continue;
		}
	}

	while (true) {
		std::string ip;
		int32_t port_listen;
		int32_t port_communicate;

		std::cout << "[*] Enter the IP address: ";
		std::cin >> ip;
		std::cout << "[*] Enter the port for communication: ";
		std::cin >> port_listen;
		std::cout << "[*] Enter the port for listening: ";
		std::cin >> port_communicate;

		if (!connection.connectToServer(ip, port_listen, port_communicate)) {
			std::cout << "[-] Error: The connection could not be established." << std::endl;
			continue;
		}
		break;
	}
}

void CommandLine::run() {
	while (true) {
		std::string command;

		if (!connection.isConnection()) {
			std::cout << "[-] Error: The server is not available." << std::endl;
			break;
		}

		std::cout << "[*] Enter the command: ";
		std::cin >> command;

		int8_t choice = processingCommand(command);
		switch (choice) {
			case 0: {
				exit();
				break;
			}
			case 1: {
				help();
				continue;
			}
			case 2: {
				getList();
				continue;
			}
			case 3: {
				std::string filename;

				std::cin >> filename;
				getFile(filename);
				continue;
			}
			default: {
				std::cout << "[-] Error: This command does not exist." << std::endl;
				continue;
			}
		}
		break;
	}
}

void CommandLine::setConsoleColor(const ConsoleColor& color) {
	std::cout << "\033[" << static_cast<int>(color) << "m";
}

std::string CommandLine::getColorString(const ConsoleColor& color) {
	std::map<ConsoleColor, std::string> colorMap = {
			{ConsoleColor::Default, "Default"},
			{ConsoleColor::Black,   "Black"},
			{ConsoleColor::Red,     "Red"},
			{ConsoleColor::Green,   "Green"},
			{ConsoleColor::Yellow,  "Yellow"},
			{ConsoleColor::Blue,    "Blue"},
			{ConsoleColor::Magenta, "Magenta"},
			{ConsoleColor::Cyan,    "Cyan"},
			{ConsoleColor::White,   "White"}
	};
	return colorMap[color];
}

void CommandLine::exit() {
	if (!connection.exit()) {
		std::cout << "[-] Error: Failed to close the connection properly." << std::endl;
		return;
	}
	std::cout << "+=========================+" << std::endl;
	std::cout << "|        Goodbye!         |" << std::endl;
	std::cout << "+=========================+" << std::endl;
}

void CommandLine::help() {
	std::cout << "[*] Available commands:" << std::endl
			<< "1. exit" << std::endl
			<< "2. help" << std::endl
			<< "3. list" << std::endl
			<< "4. get filename" << std::endl;
}

void CommandLine::getList() {
	std::vector<std::string> list;
	connection.getList(list);

	std::cout << "[*] Wait: The server is processing the request..." << std::endl;
	std::cout << "[+] Success. List of files:" << std::endl;

	for (const auto& item : list) {
		std::cout << "\t" << item << std::endl;
	}
}

void CommandLine::getFile(std::string& filename) {
	if (filename.empty()) {
		std::cout << "[-] Error: The name of the file is empty." << std::endl;
		return;
	}

	std::cout << "[*] Wait: The server is processing the request..." << std::endl;

	int64_t bytes = connection.getFile(filename);
	if (bytes >= 0) {
		std::cout << "[+] Success: The " << '\"' << filename << '\"' << " has been downloaded." << std::endl;
	} else if (bytes == -1) {
		std::cout << "[-] Error: Failed to download the file." << std::endl;
	} else if (bytes == -2) {
		std::cout << "[-] Error: The file already exists in your directory." << std::endl;
	}
}

int8_t CommandLine::processingCommand(const std::string& command) {
	if (command.empty()) {
		return -1;
	}
	for (size_t i = 0; i < commands.size(); ++i) {
		if (command.find(commands[i]) == 0) {
			return static_cast<int8_t>(i);
		}
	}
	return -1;
}
