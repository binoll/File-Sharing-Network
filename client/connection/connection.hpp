#pragma once

#include "../client.hpp"

struct data {
	int64_t fd;
	sockaddr_in* addr;
};

class Connection {
 public:
	Connection();

	~Connection();

 private:
	void listen_server();

	std::thread listen;
	struct data client;
	struct data server;
};
