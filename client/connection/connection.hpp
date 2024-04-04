#pragma once

#include "../client.hpp"

/*
 * Commands from the server:
 * 1. list - send storage of clients.
 * 2. get:size:filename - send the file.
 * 3. part:offset:size:filename - send part of the file.
 *
 *  Commands to server:
 *  1. listFiles (filename:[hash]) - send storage of clients.
 *  2. get:filename - download the file.
 *  3. exit - close connection with server.
 */
class Connection {
 public:
	explicit Connection(std::string);

	~Connection();

	bool connectToServer(const std::string&, uint16_t);

	int8_t getFile(const std::string&);

	std::list<std::string> getList();

	int8_t exit();

	void responseToServer();
	
 private:
	std::list<std::string> listFiles();

	std::string receive() const;

	int8_t sendListToServer();

	int8_t sendFileToServer(const std::string&, size_t, size_t);

	int8_t sendToServer(const std::string&);

	bool checkConnection();

	std::string calculateFileHash(const std::string&);

	void handleServer();

	std::string dir;
	int32_t client_fd;
	uint16_t client_port { };
	struct sockaddr_in client_addr { };
};
