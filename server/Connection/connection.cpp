#include "connection.hpp"

Connection::Connection(size_t port) : port(port) { }

Connection::~Connection() {
	close(this->socket_fd);
}

bool Connection::createConnection() {
	this->socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (this->socket_fd < 0) {
		std::cerr << "Error: Can not create socket" << std::endl;
		return false;
	}
	this->addr.sin_family = AF_INET;
	this->addr.sin_addr.s_addr = INADDR_ANY;
	this->addr.sin_port = htons(port);

	if (bind(this->socket_fd,
	         reinterpret_cast<struct sockaddr*>(&this->addr),
	         sizeof(this->addr)) < 0) {
		std::cerr << "[-] Error: Can not bind the socket" << std::endl;
		return false;
	}
	if (listen(this->socket_fd, 5) < 0) {
		std::cerr << "[-] Error: Listen failed" << std::endl;
		return false;
	}
	std::cout << "[+] Server listening on port: " << port << std::endl;
	return true;
}

void Connection::connectToClients() {
	std::cout << "Server is listening for connections..." << std::endl;

	while (true) {
		ssize_t fd;
		struct sockaddr_in addr;
		socklen_t addr_len = sizeof(addr);

		fd = accept(this->socket_fd, (struct sockaddr*) &addr, &addr_len);

		if (fd < 0) {
			std::cerr << "Error: Accept failed" << std::endl;
			continue;
		}
		std::cout << "[+] Client connected. Ip: " << inet_ntoa(addr.sin_addr)
				<< " port: " << addr.sin_port << std::endl;
		std::thread thread_client(&Connection::handleClient, this, fd);
		thread_client.detach();
	}
}

void Connection::handleClient(ssize_t fd) {
	std::string command;
	const std::string& command_list = commands_client[0];
	const std::string& command_get = commands_client[1];
	const std::string& command_exit = commands_client[2];

	while (!(command = receive(fd)).empty()) {
		if (command.find(command_list) == 0) {
			sendFileList(fd);
		} else if (command.find(command_get) == 0) {
			std::vector<std::string> tokens;

			split(command, ':', tokens);
			std::string filename = tokens[1];
			sendFile(fd, filename);
		} else if (command.find(command_exit)) {
			close(fd);
			std::cout << "[-] Client disconnected" << std::endl;
			break;
		} else {
			sendMsg(fd, "Invalid command");
		}
	}
}

std::string Connection::receive(ssize_t fd) {
	std::byte buffer[BUFFER_SIZE];
	ssize_t bytes_read = recv(fd, buffer, BUFFER_SIZE, 0);

	if (bytes_read > 0) {
		return std::string(reinterpret_cast<const char*>(buffer), bytes_read);
	}
	return "";
}

void Connection::sendMsg(ssize_t fd, const std::string& response) {
	send(fd, response.c_str(), response.size(), 0);
}

void Connection::sendFileList(ssize_t fd) {
	std::vector<std::string> files = getFileList();
	std::string list;

	for (const auto& file : files) {
		list += file + ' ';
	}
	sendMsg(fd, list);
}

void Connection::eraseFilesWithSameHash() {
	for (auto it1 = fileMap.begin(); it1 != fileMap.end(); ++it1) {
		for (auto it2 = std::next(it1); it2 != fileMap.end(); ++it2) {
			if (it1->second.hash == it2->second.hash) {
				fileMap.erase(it2->first);
				break;
			}
		}
	}
}

void Connection::sendFile(ssize_t fd, const std::string& filename) {
	std::ifstream file(filename, std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "Error: Unable to open file for reading: " << filename << std::endl;
		return;
	}

	file.seekg(0, std::ios::end);
	size_t size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(size);
	file.read(buffer.data(), size);

	ssize_t bytes_sent = send(fd, buffer.data(), size, 0);
	if (bytes_sent == -1) {
		std::cerr << "Error: Failed to send file: " << filename << std::endl;
		return;
	}

	std::cout << "File sent successfully: " << filename << std::endl;
}

std::vector<std::string> Connection::getFileList() {
	std::vector<std::string> files;

	for (const auto& entry : fileMap) {
		files.push_back(entry.second.filename);
	}
	return files;
}

void Connection::storeFile(ssize_t fd, const std::string& filename, const std::string& hash) {
	FileInfo data {hash, filename};
	fileMap[fd] = data;

	std::cout << "Stored filename: " << filename << " with hash: " << hash << " for user_id: " << fd << std::endl;
}

void Connection::split(const std::string& str, char delim, std::vector<std::string>& tokens) {
	std::string token;
	std::istringstream tokenStream(str);

	while (std::getline(tokenStream, token, delim)) {
		tokens.push_back(token);
	}
}
