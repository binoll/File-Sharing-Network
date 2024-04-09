#pragma once

#include "../server.hpp"

class Connection {
 public:
	explicit Connection();

	~Connection();

	void waitConnection();

	static bool isConnect(int32_t, int32_t);

 private:
	static int32_t getPort();

	void handleClients(int32_t, int32_t);

	std::string receiveData(int32_t, int32_t);

	int64_t sendData(int32_t, const std::string&, int32_t);

	bool synchronizationStorage(int32_t, int32_t);

	static bool checkConnection(int32_t);

	int64_t sendListFiles(int32_t sockfd);

	int64_t sendFile(int32_t, const std::string&);

	int64_t getFile(int32_t, const std::string&, std::byte*, uint64_t, uint64_t);

	std::pair<int32_t, int32_t> findFd(const std::string&);

	std::vector<std::string> getListFiles();

	std::multimap<std::pair<int32_t, int32_t>, FileInfo> getFilename(const std::string&);

	int64_t getSize(const std::string&);

	void updateFilesWithSameHash();

	void storeFiles(std::pair<int32_t, int32_t>, const std::string&, int64_t, const std::string& hash);

	void removeClientFiles(std::pair<int32_t, int32_t>);

	static void split(const std::string&, char, std::vector<std::string>&);

	std::multimap<std::pair<int32_t, int32_t>, FileInfo> storage;
	std::mutex mutex;
	int32_t sockfd_listen { };
	int32_t sockfd_communicate { };
	struct sockaddr_in addr_listen { };
	struct sockaddr_in addr_communicate { };
	const std::string& start_marker = marker[0];
	const std::string& end_marker = marker[1];
};
