#pragma once

#include <dirent.h>
#include "../client.hpp"

/*
 * Commands from the server:
 * 1. list - send storage of clients.
 * 2. get:size:filename - send the file.
 * 3. part:offset:size:filename - send part of the file.
 *
 *  Commands to server:
 *  1. list (filename:[hash]) - send storage of clients.
 *  2. get:filename - download the file.
 *  3. exit - close Connection with server.
 */
class Connection {
 public:
	Connection(const std::string&);

	~Connection();

	bool connectToServer(const std::string&, size_t);

	int8_t getFile(const std::string&);

	std::list<std::string> getList();

 private:
	std::list<std::string> filesList();

	std::string receive();

	int8_t sendListToServer(const std::list<std::string>& list);

	int8_t sendFileToServer(const std::string&, size_t, size_t);

	int8_t sendMsgToServer(const std::byte*, size_t);

	std::string calculateFileHash(const std::string&);

	void handleServer();

	ssize_t socket_fd;
	struct sockaddr_in server_address;
	std::mutex mutex;
	std::string dir;
	const std::string start_marker = marker[0];
	const std::string end_marker = marker[1];
};
