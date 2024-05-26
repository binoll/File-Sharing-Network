// Copyright 2024 binoll
#pragma once

#include "../client.hpp"

class Connection {
 public:
	explicit Connection(std::string);

	~Connection();

	bool connectToServer(const std::string&, int32_t, int32_t);

	[[nodiscard]] int64_t getFile(const std::string&) const;

	int64_t getList(std::vector<std::string>&) const;

 private:
	void processingServer();

	int64_t sendFile(int32_t, const std::string&, int64_t, int64_t);

	int64_t sendList(int32_t);

	static int64_t sendMessage(int32_t, const std::string&, int32_t);

	static int64_t receiveMessage(int32_t, std::string&, int32_t);

	static int64_t sendBytes(int32_t, const char*, int64_t, int32_t);

	static int64_t receiveBytes(int32_t, char*, int64_t, int32_t);

	static int64_t processResponse(std::string&);

	std::vector<std::string> getListFiles();

	int64_t getFileSize(const std::string&);

	static std::string calculateFileHash(const std::string&);

    std::vector<std::string> updateDir();

	static int64_t sendUpdatedChanges(int32_t, const std::string&);

	bool renameFile(const std::string&, const std::string&);

	static void split(const std::string&, char, std::vector<std::string>&);

	std::string dir;
	int32_t socket_listen;
	int32_t socket_communicate;
	struct sockaddr_in addr_listen { };
	struct sockaddr_in addr_communicate { };
	std::thread thread;
	std::mutex mutex;
	std::unordered_map<std::string, std::filesystem::file_time_type> storage;
};
