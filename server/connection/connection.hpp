#pragma once

#include "../server.hpp"

class Connection {
 public:
	explicit Connection(uint16_t);

	~Connection();

	void waitConnect();

 private:
	void handleClient(int32_t);

	std::string receive(int32_t);

	ssize_t sendToClient(int32_t, const std::string&);

	void synchronizationStorage(int32_t);

	bool checkConnection(int32_t);

	void sendFileList(int32_t);

	void sendFile(int32_t, const std::string&);

	std::vector<std::string> listFiles();

	std::multimap<int32_t, FileInfo> findFilename(const std::string&);

	void updateFilesWithSameHash();

	void storeClientFiles(int32_t, const std::string&, const std::string&);

	void removeClientFiles(int32_t);

	void split(const std::string&, char, std::vector<std::string>&);

	int32_t server_fd;
	uint16_t server_port;
	struct sockaddr_in server_addr;
	std::multimap<int32_t, FileInfo> storage;
	std::mutex mutex;
};
