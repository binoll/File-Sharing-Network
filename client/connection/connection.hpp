#pragma once

#include "../client.hpp"

/*
 * Commands from the server:
 * 1. list - send storage of clients.
 * 2. part:offset:size:file_name - send part of the file.
 *
 *  Commands to server:
 *  1. getListFiles (file_name:[hash]) - send storage of clients.
 *  2. get:file_name - download the file.
 *  3. exit - close connection with server.
 */
class Connection {
 public:
	explicit Connection(std::string);

	~Connection();

	bool connectToServer(const std::string&, int32_t, int32_t);

	int64_t getFile(const std::string&);

	int64_t getList(std::vector<std::string>&);

	bool exit();

	[[nodiscard]] bool isConnection() const;

 private:
	void handleServer();

	int64_t sendFile(int32_t, const std::string&, int64_t, int64_t);

	int64_t sendList(int32_t sockfd);

	int64_t sendMessage(int32_t, const std::string&, int32_t);

	int64_t receiveMessage(int32_t, std::string&, int32_t flags);

	int64_t sendBytes(int32_t, const std::byte*, int64_t, int32_t);

	int64_t receiveBytes(int32_t, std::byte*, int64_t, int32_t);

	static int64_t processResponse(std::string&);

	static bool checkConnection(int32_t);

	std::vector<std::string> getListFiles();

	static uint64_t getFileSize(const std::string&);

	std::string calculateFileHash(const std::string&);

	bool isFileExist(const std::string&);

	std::string dir;
	int32_t sockfd_listen;
	int32_t sockfd_communicate;
	struct sockaddr_in addr_listen { };
	struct sockaddr_in addr_communicate { };
	std::thread thread;
	std::mutex mutex;
};
