#include "cmd.hpp"
#include "../connection/connection.hpp"

void cmd() {
	uint8_t ch;
	std::string command;

	std::cout << "Welcome to file sharing app!" << std::endl;

	while (true) {
		std::cout << "Write the command (write \"help\" for help): ";
		std::cin >> command;

		ch = processing_command(command);

		switch (ch) {
			case commands::list::exit:
				cmd_exit();
				break;
			case commands::list::help:
				cmd_help();
				continue;
			case commands::list::list:
				cmd_list();
				continue;
			case commands::list::get:
				cmd_get();
				continue;
			default:
				std::cout << "This item did not exist! Try again." << std::endl;
				continue;
		}
		break;
	}
}

int8_t processing_command(const std::string& command) {
	uint8_t size = 0;

	for (auto& pair : commands::commands_map) {
		for (auto& sym : pair.first) {
			auto check_equal = [&sym, &size](char c) {
				return (c == sym) ? ++size : size = 0;
			};

			std::for_each(command.begin(), command.end(), check_equal);
		}

		if ((pair.second != commands::list::get && size == command.size()) ||
				(pair.second == commands::list::get && pair.first.find(command) == 0)) {
			return pair.second;
		}
	}

	return -1;
}

void cmd_exit() {
	std::cout << "Goodbye!" << std::endl;
}

void cmd_help() {

}

void cmd_list() {

}

void cmd_get() {

}