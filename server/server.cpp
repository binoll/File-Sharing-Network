#include "server.hpp"
#include "connection/connection.hpp"

int32_t main() {
	int32_t port;

	std::cout << "Write the port: ";
	std::cin >> port;

	Connection connection(port);
	connection.waitConnect();
	return EXIT_SUCCESS;
}
