#include "client.hpp"
#include "command_line/command_line.hpp"

int32_t main() {
	std::string current_path = std::filesystem::current_path().string();
	CommandLine command_line(current_path);

	command_line.run();

	return EXIT_SUCCESS;
}