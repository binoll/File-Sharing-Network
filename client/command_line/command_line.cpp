#include "command_line.hpp"

CommandLine::CommandLine() {
	std::filesystem::directory_entry entry;
	std::string exit = "exit";

	std::cout << "Welcome to file sharing app!" << std::endl;

	while (true) {
		std::string path;

		std::cout << "Write path to work dir (for exit\"exit\"): ";
		std::cin >> path;
		std::cout << std::endl;

		if (path.contains(exit)) {
			this->exit();
		}

		if (path.empty()) {
			std::perror("Error");
			std::cout << "Try again! [-]" << std::endl;
			continue;
		}

		entry = std::filesystem::directory_entry(path);

		if (!entry.exists()) {
			try {
				std::filesystem::create_directory(entry);
			} catch (std::exception& err) {
				std::perror("Error");
				std::cout << "Try again! [-]" << std::endl;
				continue;
			}
		}

		if (!entry.is_directory()) {
			std::perror("Error");
			std::cout << "Try again! [-]" << std::endl;
			continue;
		}

		dir.set_dir(entry.path());

		std::cout << "Dir is correct! [+]" << std::endl;
		break;
	}

	std::cout << "The connection is established! [+]" << std::endl;
}

CommandLine::~CommandLine() {

}

void CommandLine::run() {
	std::string command;
	uint8_t choice;

	std::cout << "Welcome to file sharing app!" << std::endl;

	while (true) {
		std::cout << "Write the command (write \"help\" for help): ";
		std::cin >> command;
		std::cout << std::endl;

		choice = processing_command(command);

		switch (choice) {
			case 0:
				this->exit();
				break;
			case 1:
				this->help();
				continue;
			case 2:
				std::list<std::filesystem::path> list_files;

				this->dir.list(list_files);
				std::cout << "Available files:" << std::endl;

				for (auto& item : list_files) {
					std::cout << "\t" << item << std::endl;
				}

				std::cout << std::endl;
				continue;
			case 3:

				dir.get_file();
				continue;
			default:
				std::cout << "This command does not exist! Try again! [-]" << std::endl;
				continue;
		}
		break;
	}
}

void CommandLine::exit() {
	std::cout << "Goodbye!" << std::endl;
	this->~CommandLine();
}

void CommandLine::help() {
	std::cout << "Commands list: " << std::endl
			<< "\t1. exit" << std::endl
			<< "\t2. help" << std::endl
			<< "\t3. list" << std::endl
			<< "\t4. get_file [filename]" << std::endl;
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