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

	bool synchronization(int32_t, int32_t);

	int64_t sendList(int32_t);

	int64_t sendFile(int32_t, std::string&);

	std::vector<std::pair<int32_t, int32_t>> findSocket(const std::string&);

	std::vector<std::string> getListFiles();

	int64_t getSize(const std::string&);

	void addFileToStorage(std::pair<int32_t, int32_t>, std::string&, int64_t, const std::string&);

	void modifyFileInStorage(std::pair<int32_t, int32_t> pair, std::string& filename, int64_t size, const std::string& hash);

	void deleteFileFromStorage(std::pair<int32_t, int32_t> pair, std::string& filename, int64_t size, const std::string& hash);

	int64_t updateStorage(std::pair<int32_t, int32_t>);

	void removeClients(std::pair<int32_t, int32_t>);

	static std::string removeIndex(std::string);

	static void split(const std::string&, char, std::vector<std::string>&);

	bool isFileExistOnClient(int32_t socket, const std::string& filename);

	int32_t socket_listen { };
	int32_t socket_communicate { };
	struct sockaddr_in addr_listen { };
	struct sockaddr_in addr_communicate { };
	std::multimap<std::pair<int32_t, int32_t>, FileInfo> storage;
	std::vector<std::thread> threads;
	std::mutex mutex_storage;
};
