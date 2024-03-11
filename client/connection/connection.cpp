#include "connection.hpp"

Connection::Connection() {
	std::cout << "";

	this->listen = std::thread(&Connection::listen_server, this);
}

Connection::~Connection() {
	try {
		listen.~thread();
	} catch (std::exception& err) {
		std::perror("Error");
		std::cout << std::endl;
	}

	if (close(server.fd) < 0) { throw std::system_error(); }

	if (close(client.fd) < 0) { throw std::system_error(); }
}

void Connection::listen_server() {

}
