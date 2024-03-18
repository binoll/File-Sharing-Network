#include "server.hpp"
#include "Connection/connection.hpp"

int32_t main() {
	uint64_t port;

	std::cout << "Write the port: ";
	std::cin >> port;

	Connection connection(port);

	return EXIT_SUCCESS;
}
