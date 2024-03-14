#pragma once

#include "../client.hpp"
#include "../dir/dir.hpp"

/*
 * Commands from the server:
 * 1. list - send list of files.
 * 2. get:size:filename - send the file.
 * 3. part:offset:size:filename - send part of the file.
 *
 * Commands to server:
 *  1. list (filename:[hash]) - send list of files.
 *  2. get:filename - download the file.
 *  3. exit - close connection with server.
 */
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

	// Close connection with server.
	int8_t exit();

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

	// Send response to the server.
	void send_response(std::byte* buf, uint64_t bytes);

	Dir dir;
	std::thread listen;
	struct data server;
};
