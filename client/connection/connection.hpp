#pragma once

#include "../../libs.hpp"

// Data for the functioning of the client side
namespace data {
	struct client {
		int64_t sockfd = -1;
		struct sockaddr addr;
	};

	struct server {
		int64_t sockfd = -1;
		struct sockaddr_in addr;
	};
}

// The function connects to the server and returns the fd of server
void connect_server(struct data::server&, struct data::client&);

// The function closes the connection to the server
void connect_close(struct data::server&, struct data::client&);

// The function
void listen_server();
