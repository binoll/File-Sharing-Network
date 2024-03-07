#include "connection.hpp"

int8_t connect_server(struct data::server& connection_server,
                      struct data::client& connection_client) {
	struct sockaddr_in* addr = nullptr;
	int64_t sfd_server;
	int64_t sfd_client;
	uint16_t port_server;
	std::string ip_server;
	std::string exit = "exit";

	while (true) {
		std::cout << "Enter the IPv4-address of the server (for exit - \"exit\"): ";
		std::cin >> ip_server;
		std::cout << std::endl;

		if (ip_server.compare(exit)) {
			return -1;
		}

		if (ip_server.size() > INET_ADDRSTRLEN) {
			std::cout << "Incorrect IPv4-address of the server! Try again! [-]" << std::endl;
			continue;
		}

		std::cout << "Enter the port of the server: ";
		std::cin >> port_server;
		std::cout << std::endl;

		if ((sfd_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0) {
			std::cout << "Can not create socket! Try again! [-]" << std::endl;
			continue;
		} else {
			break;
		}
	}

	addr->sin_family = AF_INET;
	addr->sin_port = htons(port_server);

	if (inet_pton(AF_INET, ip_server.c_str(), &addr->sin_addr) <= 0) {
		throw std::system_error();
	}

	if ((sfd_client = connect(sfd_server,
	                          reinterpret_cast<const sockaddr*>(&addr->sin_addr),
	                          sizeof(*addr))) <= 0) {
		throw std::system_error();
	}

	connection_server.sockfd = sfd_server;
	connection_client.sockfd = sfd_client;

	return 0;
}

int8_t connect_close(struct data::server& connection_server, struct data::client& connection_client) {
	if (close(connection_server.sockfd) <= 0) {
		throw std::system_error();
	}

	if (close(connection_client.sockfd) <= 0) {
		throw std::system_error();
	}

	return 0;
}

void listen_server() {
	while (true) {

	}
}