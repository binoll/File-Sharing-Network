#include "commandline.hpp"

CommandLine::CommandLine(std::string& path) : connection(path) {
	std::cout << "\033[1;34m" << "+=========================+" << std::endl;
	std::cout << "\033[1;34m" << "|   Welcome to the file   |" << std::endl;
	std::cout << "\033[1;34m" << "|      sharing app!       |" << std::endl;
	std::cout << "\033[1;34m" << "+=========================+" << std::endl;

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
				break;
			}
			case 2: {
				getList();
				break;
			}
			case 3: {
				getFile(command);
				break;
			}
			default: {
				std::cout << "[-] Error: This command does not exist." << std::endl;
				break;
			}
		}
	}
}

void CommandLine::exit() {
	if (!connection.exit()) {
		std::cout << "[-] Error: Failed to close the connection properly." << std::endl;
		return;
	}
	std::cout << "\033[1;32m" << "+========================+" << std::endl;
	std::cout << "\033[1;32m" << "|        Goodbye!        |" << std::endl;
	std::cout << "\033[1;32m" << "+========================+" << std::endl;
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

	std::cout << "[+] Success. List of files:" << std::endl;

	for (const auto& item : list) {
		std::cout << "\t" << item << std::endl;
	}
}

void CommandLine::getFile(std::string& command) {
	std::string filename = parseGetCommand(command);

	if (filename.empty()) {
		std::cout << "[-] Error: The name of the file is empty." << std::endl;
		return;
	}

	int64_t bytes = connection.getFile(filename);
	if (bytes > 0) {
		std::cout << "[+] Success: The " << '\"' << filename << '\"' << " has been downloaded." << std::endl;
	} else if (bytes == -1) {
		std::cout << "[-] Error: Failed to download the file." << std::endl;
	} else if (bytes == -2) {
		std::cout << "[-] Error: The file already exists in your directory." << std::endl;
	}
}

std::string CommandLine::parseGetCommand(const std::string& command) {
	size_t pos = command.find(' ');
	if (pos == std::string::npos || pos == command.size() - 1) {
		std::cout << "[-] Error: Invalid command format." << std::endl;
		return "";
	}
	return command.substr(pos + 1);
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
