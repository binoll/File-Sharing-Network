#include "server.hpp"
#include "Connection/connection.hpp"

int32_t main() {
	uint64_t port;

	std::cout << "Write the port: ";
	std::cin >> port;

	Connection connection(port);

	if (!connection.createConnection()) {
		return EXIT_FAILURE;
	}
	connection.connectToClients();
	return EXIT_SUCCESS;
}
