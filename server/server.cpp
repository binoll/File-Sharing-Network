#include "server.hpp"
#include "connection/connection.hpp"

int32_t main() {
	int32_t port;

	Connection connection;
	connection.waitConnection();
	return EXIT_SUCCESS;
}