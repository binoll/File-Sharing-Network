#include "commandline.hpp"

CommandLine::CommandLine(std::string& path) : connection(path) {
	std::cout << "Welcome to file sharing app!" << std::endl;

	while (true) {
		std::string ip;
		int32_t port_listen;
		int32_t port_communicate;

		std::cout << "[*] Write the ip-address: ";
		std::cin >> ip;
		std::cout << "[*] Write the port for communicate: ";
		std::cin >> port_listen;
		std::cout << "[*] Write the port for listening: ";
		std::cin >> port_communicate;
		if (!connection.connectToServer(ip, port_listen, port_communicate)) {
			std::cout << "[-] Error: The connection was not established." << std::endl;
			continue;
		}
		break;
	}
}

void CommandLine::run() {
	while (true) {
		std::string command;
		int64_t bytes;
		int8_t choice;

		if (!connection.isConnection()) {
			std::cout << "[-] Error: The server is not available." << std::endl;
			break;
		}

		std::cout << "[*] Write the command: ";
		std::cin >> command;
		choice = processing_command(command);
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
				std::vector<std::string> list;
				connection.getList(list);

				if (list.empty()) {
					std::cerr << "[-] Error: The list of files could not be retrieved." << std::endl;
					continue;
				}
				std::cout << "[+] Success. List: " << std::endl;
				for (auto& item : list) {
					std::cout << "\t" << item << std::endl;
				}
				std::cout << std::endl;
				continue;
			}
			case 3: {
				std::string file_name;

				std::cout << "[*] Write the name of the file: ";
				std::cin >> file_name;

				if (file_name.empty()) {
					std::cout << "[-] Error: The name of the file is empty." << std::endl;
					continue;
				}

				bytes = connection.getFile(file_name);
				if (bytes == -1) {
					std::cout << "[-] Error: Failed to download the file." << std::endl;
				} else if (bytes == -2) {
					std::cout << "[-] Error: The file exists in your directory." << std::endl;
				}
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

void CommandLine::exit() {
	if (!connection.exit()) {
		std::cout << "[-] Error: Failed to correctly close connection." << std::endl;
		return;
	}
	std::cout << "Goodbye!" << std::endl;
}

void CommandLine::help() {
	std::cout << "Commands storage: " << std::endl
			<< "\t1. exit" << std::endl
			<< "\t2. help" << std::endl
			<< "\t3. storage" << std::endl
			<< "\t4. get" << std::endl;
}

int8_t CommandLine::processing_command(const std::string& command) {
	int8_t index = -1;

	if (command.empty()) { return -1; }
	for (const auto& item : commands) {
		++index;
		if (command.find(item) == 0) {
			return index;
		}
	}
	return -1;
}
