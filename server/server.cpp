#include "server.hpp"
#include "connection/connection.hpp"

int32_t main() {
	uint64_t port;

	std::cout << "Write the server_port: ";
	std::cin >> port;

	Connection connection(port);
	connection.waitConnect();
	return EXIT_SUCCESS;
}
