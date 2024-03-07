#include "cmd.hpp"

void cmd(std::filesystem::path& dir) {
	std::string command;
	uint8_t ch;

	std::cout << "Welcome to file sharing app!" << std::endl;

	while (true) {
		std::cout << "Write the command (write \"help\" for help): ";
		std::cin >> command;
		std::cout << std::endl;

		ch = processing_command(command);

		switch (ch) {
			case commands::list::exit:
				cmd_exit();
				break;
			case commands::list::help:
				cmd_help();
				continue;
			case commands::list::list:
				update_dir(dir);
				cmd_list(dir);
				continue;
			case commands::list::get:
				update_dir(dir);

				try {
					cmd_get(dir, command);
				} catch (std::exception e) {
					std::cout << "The file does not exist! Try again!" << std::endl;
				}
				continue;
			default:
				std::cout << "This number does not exist! Try again!" << std::endl;
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
	std::cout << "Commands list: " << std::endl
			<< "\t1. exit" << std::endl
			<< "\t2. help" << std::endl
			<< "\t3. list" << std::endl
			<< "\t4. get [filename]" << std::endl;
}
