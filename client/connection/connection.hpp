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

	bool connectToServer(const std::string&, int32_t);

	int64_t getFile(const std::string&);

	[[nodiscard]] std::list<std::string> getList() const;

	[[nodiscard]] bool exit() const;

 private:
	static int32_t findFreePort();

	static int64_t sendToServer(int32_t, const std::string&);

	static std::string receive(int32_t);

	std::list<std::string> listFiles();

	static bool checkConnection(int32_t);

	int64_t sendListToServer(int32_t);

	int64_t sendFileToServer(const std::string&, uint64_t, uint64_t);

	std::string calculateFileHash(const std::string&);

	void handleServer();

	std::string dir;
	int32_t listen_fd = -1;
	int32_t client_fd = -1;
	int32_t listen_port = -1;
	int32_t client_port = -1;
	struct sockaddr_in client_addr { };
	struct sockaddr_in listen_addr { };
	std::thread listen_server;
	std::mutex mutex;
};
