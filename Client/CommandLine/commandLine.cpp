#include "commandLine.hpp"

CommandLine::CommandLine(std::string& path) : connection(path) {
	std::cout << "Welcome to file sharing app!" << std::endl;

	while (true) {
		std::string work_path;

		std::cout << "+------------------------------------------------+" << std::endl;
		std::cout << "Write the dir: ";
		std::cin >> work_path;

		std::cout << "[+] Dir is correct" << std::endl;
		break;
	}

	while (true) {
		std::string ip;
		uint64_t port;

		std::cout << "+------------------------------------------------+" << std::endl;
		std::cout << "Write the ip-address: ";
		std::cin >> ip;
		std::cout << "Write the client_port: ";
		std::cin >> port;

		if (!this->connection.connectToServer(ip, port)) {
			std::cout << "[-] Try again!" << std::endl;
			continue;
		}
		std::cout << "[+] The Connection is established" << std::endl;
		break;
	}
}

void CommandLine::run() {
	std::string command;
	int8_t choice;

	while (true) {
		std::cout << "+------------------------------------------------+" << std::endl;
		std::cout << "Write the command (for help - \"help\"): ";
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
					std::cout << "No clients available. Try again! [-]" << std::endl;
					continue;
				}

				std::cout << "List: " << std::endl;

				for (auto& item : list) {
					std::cout << "\t" << item << std::endl;
				}
				std::cout << std::endl;
				continue;
			}
			case 3: {
				std::string filename;

				std::cout << "Write the name of the file: ";
				std::cin >> filename;

				if (filename.empty()) {
					std::cout << "The name of the file is empty. Try again! [-]" << std::endl;
					continue;
				}

				if (this->connection.getFile(filename) == -1) {
					std::cout << "Try again! [-]" << std::endl;
				}
				continue;
			}
			default: {
				std::cout << "This command does not exist! Try again! [-]" << std::endl;
				continue;
			}
		}
		break;
	}
}

void CommandLine::exit() {
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

	for (auto& item : this->commands) {
		++index;

		if (command.find(item) == 0) {
			return index;
		}
	}

	return -1;
}