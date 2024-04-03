#pragma once

#include "../client.hpp"

/*
 * Commands from the Server:
 * 1. list - send storage of clients.
 * 2. get:size:filename - send the file.
 * 3. part:offset:size:filename - send part of the file.
 *
 *  Commands to Server:
 *  1. listFiles (filename:[hash]) - send storage of clients.
 *  2. get:filename - download the file.
 *  3. exit - close Connection with Server.
 */
class Connection {
 public:
	explicit Connection(std::string);

	~Connection();

	bool connectToServer(const std::string&, uint16_t);

	int8_t getFile(const std::string&);

	std::list<std::string> getList();

 private:
	std::list<std::string> listFiles();

	std::string receive();

	int8_t sendListToServer(const std::list<std::string>& list);

	int8_t sendFileToServer(const std::string&, size_t, size_t);

	int8_t sendToServer(const std::string& msg) const;

	std::string calculateFileHash(const std::string&);

	void handleServer();

	std::string dir;
	int32_t client_fd;
	uint16_t client_port;
	struct sockaddr_in client_addr;
	const std::string start_marker = marker[0];
	const std::string end_marker = marker[1];
};
