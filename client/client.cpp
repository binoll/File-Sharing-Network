#include "client.hpp"
#include "command_line/command_line.hpp"

int32_t main() {
	CommandLine command_line;

	command_line.run();

	return EXIT_SUCCESS;
}