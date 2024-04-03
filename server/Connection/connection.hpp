#pragma once

#include "../server.hpp"

struct FileInfo {
	std::string hash;
	std::string filename;
};

class Connection {
 public:
	explicit Connection(size_t);

	~Connection();

	void createConnection();

 private:
	void handleClient(ssize_t);

	std::string receive(ssize_t);

	void updateStorage(ssize_t);

	void sendToClient(ssize_t, const std::string&);

	void sendFile(ssize_t, const std::string&);

	std::vector<std::string> getFileList();

	std::unordered_map<ssize_t, FileInfo> findFilename(const std::string&);

	void eraseFilesWithSameHash();

	void storeFile(ssize_t, const std::string&, const std::string&);

	void sendFileList(ssize_t);

	void split(const std::string&, char, std::vector<std::string>&);

	ssize_t server_fd;
	struct sockaddr_in addr;
	size_t port;
	std::unordered_map<ssize_t, FileInfo> storage;
	const std::string start_marker = marker[0];
	const std::string end_marker = marker[1];
};
