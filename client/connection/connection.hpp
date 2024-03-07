#pragma once

#include "../../libs.hpp"

// Data for the functioning of the client side
namespace data {
	struct client {
		int64_t sockfd;
		struct sockaddr_in addr;
	};

	struct server {
		int64_t sockfd;
		struct sockaddr_in addr;
	};
}

// The function connects to the server and returns the fd of server
int64_t connect_server(std::string&);
