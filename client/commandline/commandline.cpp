#include "commandline.hpp"

CommandLine::CommandLine(std::string& path) : connection(path) {
	std::cout << "Welcome to file sharing app!" << std::endl;

	while (true) {
		std::string ip;
		int32_t port_listen;
		int32_t port_communicate;

		std::cout << "[*] Write the ip-address: ";
		std::cin >> ip;
		std::cout << "[*] Write the port for listening: ";
		std::cin >> port_listen;
		std::cout << "[*] Write the port for communicate: ";
		std::cin >> port_communicate;
		if (!connection.connectToServer(ip, port_listen, port_communicate)) {
			continue;
		}
		std::cout << "[+] The connection is established: " << ip << ':' << port_listen << ", " << ip << ':'
				<< port_communicate << '.' << std::endl;
		break;
	}
}

void CommandLine::run() {
	while (true) {
		std::string command;
		int8_t choice;

		if (!connection.isConnection()) {
			std::cout << "[-] Error: The server is not available." << std::endl;
			break;
		}

		std::cout << "[*] Write the command (for help - \"help\"): ";
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
				std::list<std::string> list = connection.getList();

				if (list.empty()) {
					std::cout << "[-] Error: No clients available. Try again!" << std::endl;
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
				std::string filename;

				std::cout << "[*] Write the name of the file: ";
				std::cin >> filename;
				if (filename.empty()) {
					std::cout << "[-] Error: The name of the file is empty. Try again!" << std::endl;
					continue;
				}
				if (connection.getFile(filename) < 0) {
					std::cout << "[-] Error: Failed to download the file. Try again!" << std::endl;
				}
				continue;
			}
			default: {
				std::cout << "[-] Error: This command does not exist. Try again!" << std::endl;
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
