#pragma once

#include "../client.hpp"
#include "../dir/dir.hpp"

// Struct for data
struct data {
	int32_t fd;
	sockaddr_in addr;
};

class Connection {
 public:
	explicit Connection(const std::string&);

	~Connection();

	int8_t create_connection(const std::string&, uint64_t);

	int8_t list();

	int8_t get(const std::filesystem::path&);

	int8_t update_dir(const std::string&);

 private:
	void listen_server();

	int8_t send_list();

	int8_t send_file(const std::string&);

	int8_t  send_err(const std::string&);

	void send_response(std::byte* buf, uint64_t size);

	Dir dir;
	std::thread listen;
	std::mutex mutex;
	struct data server;
};
