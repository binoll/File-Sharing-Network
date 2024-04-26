// Copyright 2024 binoll
#pragma once

#include "../client.hpp"

class Connection {
 public:
	explicit Connection(std::string);

	~Connection();

	bool connectToServer(const std::string&, int32_t, int32_t);

	int64_t getFile(const std::string&);

	int64_t getList(std::vector<std::string>&) const;

	[[nodiscard]] bool exit() const;

	[[nodiscard]] bool isConnection() const;

 private:
	void handleServer();

	static int64_t sendFile(int32_t, const std::string&, int64_t, int64_t);

	int64_t sendList(int32_t);

	static int64_t sendMessage(int32_t, const std::string&, int32_t);

	static int64_t receiveMessage(int32_t, std::string&, int32_t);

	static int64_t sendBytes(int32_t, const char*, int64_t, int32_t);

	static int64_t receiveBytes(int32_t, char*, int64_t, int32_t);

	static int64_t processResponse(std::string&);

	static bool checkConnection(int32_t);

	std::vector<std::string> getListFiles();

	static int64_t getFileSize(const std::string&);

	static std::string calculateFileHash(const std::string&);

	bool isFileExist(const std::string&);

	std::string dir;
	int32_t socket_listen;
	int32_t socket_communicate;
	struct sockaddr_in addr_listen { };
	struct sockaddr_in addr_communicate { };
	std::thread thread;
	std::mutex mutex_dir;
};
