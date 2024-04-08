#include "server.hpp"
#include "connection/connection.hpp"

int32_t main() {
	Connection connection;

	connection.waitConnection();
	return EXIT_SUCCESS;
}
