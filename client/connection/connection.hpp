#pragma once

#include "../client.hpp"
#include "../dir/dir.hpp"

struct data {
	int32_t fd;
	sockaddr_in addr;
};

class Connection {
 public:
	// Constructor.
	explicit Connection(const std::string&);

	// Destructor.
	~Connection();

	// Create connection with server.
	int8_t create_connection(const std::string&, uint64_t);

	// Push names of files available on the server in the list.
	int8_t list(std::list<std::filesystem::path>&);

	// Download file from server.
	int8_t get(const std::filesystem::path&);

	// Update work dir.
	int8_t update_dir(const std::string&);

 private:
	// Listen msg form server.
	void listen_server();

	// Send list of files to the server.
	int8_t send_list();

	// Send file to the server.
	int8_t send_file(const std::string&);

	// Send error msg to the server.
	int8_t send_err(const std::string&);

	// Send response to the server.
	void send_response(std::byte* buf, uint64_t size);

	Dir dir;
	std::thread listen;
	std::mutex mutex;
	struct data server;
};
