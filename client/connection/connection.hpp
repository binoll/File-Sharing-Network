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
	int8_t list(std::list<std::string>&);

	// Download file from server.
	int8_t get(const std::string&);

	// Update work dir.
	int8_t update_dir(const std::string&);

 private:
	// Listen msg form server.
	void listen_server();

	// Send list of files to the server.
	int8_t send_list();

	// Send part of the file to the server.
	int8_t send_file(const std::string&, int64_t, int64_t);

	// Send file to the server.
	int8_t send_file(const std::string& path, int64_t);

	// Send msg to the server.
	int8_t send_msg(const std::string&);

	/*
	 * Send response to the server.
	 *
	 * Commands from server:
	 * 1. list - send list of files.
	 * 2. get:[size]:[filename] - send file.
	 * 3. part:[offset]:[bytes]:[filename] - send part of file.
	 *
	 */
	void send_response(std::byte* buf, uint64_t bytes);

	Dir dir;
	std::thread listen;
	std::mutex mutex;
	struct data server;
};
