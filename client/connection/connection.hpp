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

	[[nodiscard]] bool isConnect() const;

 private:
	static int32_t findFreePort();

	[[nodiscard]] static int64_t sendToServer(int32_t, const std::string&) ;

	[[nodiscard]] static std::string receive(int32_t) ;

	std::list<std::string> listFiles();

	static bool checkConnection(int32_t);

	int64_t sendListToServer(int32_t);

	int64_t sendFileToServer(int32_t, const std::string&, uint64_t, uint64_t);

	std::string calculateFileHash(const std::string&);

	void handleServer();

	std::string dir;
	int32_t passive_fd = -1;
	int32_t active_fd = -1;
	int32_t receive_port = -1;
	int32_t send_port = -1;
	struct sockaddr_in send_addr { };
	struct sockaddr_in receive_addr { };
	std::thread thread;
	std::mutex mutex;
};
