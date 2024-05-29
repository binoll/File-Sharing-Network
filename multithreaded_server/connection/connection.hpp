// Copyright 2024 binoll
#pragma once

#include "../server.hpp"

class Connection {
 public:
	explicit Connection();

	~Connection();

	void waitConnection(int32_t, int32_t);

 private:
	void closeConnection(int32_t, int32_t);

	void processingClients(int32_t, int32_t);

	static int64_t sendMessage(int32_t, const std::string&, int32_t);

	static int64_t receiveMessage(int32_t, std::string&, int32_t);

	static int64_t sendBytes(int32_t, const char*, int64_t, int32_t);

	static int64_t receiveBytes(int32_t, char*, int64_t, int32_t);

	static int64_t processResponse(std::string&);

	bool checkConnection(int32_t );

	bool synchronization(int32_t, int32_t);

	int64_t sendList(int32_t);

	int64_t sendFile(int32_t, std::string);

	std::vector<std::pair<int32_t, int32_t>> findSocket(const std::string&);

	std::vector<std::string> getListFiles();

	int64_t getSize(const std::string&);

	void indexFiles();

	void storeFiles(std::pair<int32_t, int32_t>, const std::string&, int64_t, const std::string&);

	void removeClients(std::pair<int32_t, int32_t>);

	static void split(const std::string&, char, std::vector<std::string>&);

	static std::string removeIndex(std::string);

	bool isFilenameModify(const std::string&);

	bool isFilenameChange(int32_t, const std::string&);

	bool isFileExistOnClient(int32_t, const std::string&);

	int32_t socket_listen { };
	int32_t socket_communicate { };
	struct sockaddr_in addr_listen { };
	struct sockaddr_in addr_communicate { };
	std::multimap<std::pair<int32_t, int32_t>, FileInfo> storage;
	std::vector<std::thread> threads;
	std::mutex mutex;
};
