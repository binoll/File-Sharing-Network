#pragma once

#include "../../libs.hpp"

namespace commands {
	enum list {
		exit = 0, help = 1, list = 2, get = 3
	};

	std::map<std::string, uint8_t> commands_map = {
			{"exit", list::exit},
			{"help", list::help},
			{"list", list::list},
			{"get",  list::get}
	};
}

void cmd();

int8_t processing_command(const std::string&);

void cmd_exit();

void cmd_help();

void cmd_list();

void cmd_get();
