#include "client.hpp"
#include "CommandLine/commandLine.hpp"

int32_t main() {
	std::string current_path = ".";
	CommandLine command_line(current_path);

	command_line.run();

	return EXIT_SUCCESS;
}
