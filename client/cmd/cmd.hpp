#pragma once

#include "../../libs.hpp"
#include "../connection/connection.hpp"

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

void cmd(std::filesystem::path&);

int8_t processing_command(const std::string&);

void cmd_exit();

void cmd_help();

extern void cmd_list(std::filesystem::path&);

extern void cmd_get(std::filesystem::path&, std::string&);

extern bool update_dir(std::filesystem::path&);
