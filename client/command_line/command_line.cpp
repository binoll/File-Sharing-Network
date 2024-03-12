#include "command_line.hpp"

CommandLine::CommandLine(std::string& path) : connection(path) {
	std::string exit = "exit";

	std::cout << "Welcome to file sharing app!" << std::endl;

	while (true) {
		std::string user_path;

		std::cout << "Write path: ";
		std::cin >> user_path;

		if (user_path == exit) {
			this->exit();
		}

		if (this->update_dir(user_path) == -1) {
			std::cout << "Try again! [-]" << std::endl;
			continue;
		}

		std::cout << "Dir is correct! [+]" << std::endl;
		break;
	}

	std::cout << "The connection is established! [+]" << std::endl;
}

void CommandLine::run() {
	std::string command;
	uint8_t choice;

	while (true) {
		std::cout << "+----------------------------------------------------+" << std::endl;
		std::cout << "Command (for help - \"help\"): ";
		std::cin >> command;

		choice = processing_command(command);

		switch (choice) {
			default:
				std::cout << "This command does not exist! Try again! [-]" << std::endl;
				continue;
			case 0:
				this->exit();
				break;
			case 1:
				this->help();
				continue;
			case 2:
				this->connection.list();
				continue;
			case 3:
				std::string path;

				std::cout << "Write name of the file: ";
				std::cin >> path;

				if (connection.get(path)) {
					std::cout << "Try again! [-]" << std::endl;
				}
				continue;
		}
		break;
	}
}

int8_t CommandLine::update_dir(const std::string& path) {
	return this->connection.update_dir(path);
}

void CommandLine::exit() {
	std::cout << "Goodbye!" << std::endl;
}

void CommandLine::help() {
	std::cout << "Commands list: " << std::endl
			<< "\t1. exit" << std::endl
			<< "\t2. help" << std::endl
			<< "\t3. list" << std::endl
			<< "\t4. get [filename]" << std::endl;
}

int8_t CommandLine::processing_command(const std::string& command) {
	int8_t is_find = -1;

	if (command.empty()) { return -1; }

	for (auto& item : this->commands) {
		++is_find;

		if (command.find(item) == 0) {
			return is_find;
		}
	}

	return -1;
}