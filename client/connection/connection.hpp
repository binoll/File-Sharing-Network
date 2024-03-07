#pragma once

#include "../../libs.hpp"

// Data for the functioning of the client side
namespace data {
	struct client {
		int64_t sockfd = -1;
	};

	struct server {
		int64_t sockfd = -1;
	};
}

// The function connects to the server and returns the fd of server
int8_t connect_server(struct data::server&, struct data::client&);

// The function closes the connection to the server
int8_t connect_close(struct data::server&, struct data::client&);

// The function
void listen_server();
