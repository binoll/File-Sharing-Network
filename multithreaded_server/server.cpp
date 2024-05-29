// Copyright 2024 binoll
#include "server.hpp"
#include "connection/connection.hpp"

int32_t main() {
	Connection connection;
	int32_t port_listen;
	int32_t port_communicate;

	std::cout << "Enter port_listen: ";
	std::cin >> port_listen;
	std::cout << "Enter port_communicate: ";
	std::cin >> port_communicate;

	connection.waitConnection(port_listen, port_communicate);

	return EXIT_SUCCESS;
}
