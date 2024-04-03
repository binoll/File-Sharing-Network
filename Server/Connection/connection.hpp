#pragma once

#include "../server.hpp"
#include "../ThreadPool/threadpool.hpp"

class Connection {
 public:
	explicit Connection(uint16_t);

	~Connection();

	void waitConnect();

 private:
	void handleClient(int32_t);

	std::string receive(int32_t);

	void updateStorage(int32_t);

	void sendToClient(int32_t, const std::string&);

	void sendFile(int32_t, const std::string&);

	std::vector<std::string> getFilesList();

	std::multimap<int32_t, FileInfo> findFilename(const std::string&);

	void updateFilesWithSameHash();

	void storeFile(int32_t, const std::string&, const std::string&);

	void sendFileList(int32_t);

	void split(const std::string&, char, std::vector<std::string>&);

	int32_t server_fd;
	uint16_t server_port;
	struct sockaddr_in server_addr;
	ThreadPool pool;
	std::multimap<ssize_t, FileInfo> storage;
	const std::string start_marker = marker[0];
	const std::string end_marker = marker[1];
};
