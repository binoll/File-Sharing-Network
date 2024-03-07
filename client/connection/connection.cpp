#include "connection.hpp"

void connect_server(struct data::server& connection_server,
                    struct data::client& connection_client) {
	struct sockaddr_in* sockaddr_server = nullptr;
	struct sockaddr* sockaddr_client = nullptr;
	int64_t sockfd_server;
	int64_t sockfd_client;
	uint64_t port_server;
	std::string ip_server;

	while (true) {
		std::cout << "Enter the IPv4-address of the server: ";
		std::cin >> ip_server;
		std::cout << std::endl;

		if (ip_server.size() > INET_ADDRSTRLEN) {
			std::cout << "Incorrect IPv4-address of the server! [-]" << std::endl;
			continue;
		}

		std::cout << "Enter the port of the server: ";
		std::cin >> port_server;
		std::cout << std::endl;

		if ((sockfd_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
			throw std::system_error();
		} else {
			break;
		}
	}

	sockaddr_server->sin_family = AF_INET;
	sockaddr_server->sin_port = htons(port_server);
	inet_pton(AF_INET, ip_server.c_str(), &sockaddr_server->sin_addr);

	connection_server.sockfd = sockfd_server;
	connection_server.addr = *sockaddr_server;

	if ((sockfd_client = connect(sockfd_server, sockaddr_client, sizeof(sockaddr_client))) == -1) {
		throw std::system_error();
	}

	connection_client.sockfd = sockfd_client;
	connection_client.addr = *sockaddr_client;
}

void connect_close(struct data::server&, struct data::client&) {

}

void listen_server() {

}