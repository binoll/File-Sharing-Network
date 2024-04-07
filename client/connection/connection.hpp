#pragma once

#include "../client.hpp"

/*
 * Commands from the server:
 * 1. list - send storage of clients.
 * 2. get:filename - send the file.
 * 3. part:offset:size:filename - send part of the file.
 *
 *  Commands to server:
 *  1. getListFiles (filename:[hash]) - send storage of clients.
 *  2. get:filename - download the file.
 *  3. exit - close connection with server.
 */
class Connection {
 public:
	explicit Connection(std::string);

	~Connection();

	bool connectToServer(const std::string&, int32_t);

	[[nodiscard]] int64_t getFile(const std::string&) const;

	[[nodiscard]] std::list<std::string> getList() const;

	[[nodiscard]] bool exit() const;

	[[nodiscard]] bool isConnection() const;

 private:
	static int32_t getPort();

	static int64_t sendData(int32_t, const std::string&);

	[[nodiscard]] static std::string receiveData(int32_t);

	std::list<std::string> getListFiles();

	static uint64_t getFileSize(const std::string&);

	static bool checkConnection(int32_t);

	int64_t sendList(int32_t);

	static int64_t sendFile(int32_t, const std::string&, uint64_t, uint64_t);

	std::string calculateFileHash(const std::string&);

	void handleServer();

	std::string dir;
	int32_t server_fd = 0;
	int32_t client_fd = 0;
	int32_t server_port = 0;
	int32_t client_port = 0;
	struct sockaddr_in client_addr { };
	struct sockaddr_in server_addr { };
	std::thread thread;
	std::mutex mutex;
};
