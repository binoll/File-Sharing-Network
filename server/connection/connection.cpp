#include "connection.hpp"

Connection::Connection(int32_t port) : server_port(port) {
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(server_port);
}

Connection::~Connection() {
	close(server_fd);
}

void Connection::waitConnect() {
	server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_fd < 0) {
		std::cerr << "[-] Error: Failed to create socket." << std::endl;
		return;
	}
	if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
		std::cerr << "[-] Error: Failed to bind the socket." << std::endl;
		return;
	}
	if (listen(server_fd, BACKLOG) < 0) {
		std::cerr << "[-] Error: Failed to listen." << std::endl;
		return;
	}

	std::cout << "[*] Wait: server is listening for connections..." << std::endl;
	while (true) {
		int32_t client_fd;
		struct sockaddr_in client_addr { };
		socklen_t addr_len = sizeof(client_addr);

		client_fd = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
		if (client_fd < 0) { continue; }
		std::cout << "[+] Success: Client connected: " << inet_ntoa(client_addr.sin_addr)
				<< ':' << client_addr.sin_port << '.' << std::endl;
		std::thread(&Connection::handleClients, this, client_fd).detach();
	}
}

void Connection::handleClients(int32_t fd) {
	std::string command;
	const std::string& command_list = commands_client[0];
	const std::string& command_get = commands_client[1] + ':';
	const std::string& command_exit = commands_client[2];

	synchronizationStorage(fd);
	while (checkConnection(fd) > 0) {
		command = receive(fd);

		if (command == command_list) {
			sendFileList(fd);
		} else if (command == command_get) {
			std::vector<std::string> tokens;
			split(command, ':', tokens);
			std::string filename = tokens[1];
			sendFile(fd, filename);
		} else if (command == command_exit) {
			close(fd);
			std::cout << "[+] Success: Client disconnected." << std::endl;
			break;
		}
	}
	close(fd);
	removeClientFiles(fd);
}

std::string Connection::receive(int32_t fd) {
	const std::string& command_err = commands_server[4];
	std::byte buffer[BUFFER_SIZE];
	std::string receive_data;
	int64_t bytes;

	bytes = recv(fd, buffer, BUFFER_SIZE, MSG_WAITFORONE);
	receive_data = std::string(reinterpret_cast<char*>(buffer), bytes);
	if (bytes > 0) {
		return receive_data;
	}
	return command_err;
}

int64_t Connection::sendToClient(int32_t fd, const std::string& command) {
	int64_t bytes;

	bytes = send(fd, command.c_str(), command.size(), MSG_CONFIRM);
	if (bytes == -1) {
		std::cerr << "[-] Error: Failed to send command." << std::endl;
	}
	return bytes;
}

void Connection::synchronizationStorage(int32_t fd) {
	const std::string& start_marker = marker[0];
	const std::string& end_marker = marker[1];
	std::string response;
	uint64_t pos_start;

	response = receive(fd);
	pos_start = response.find(start_marker);

	if (pos_start == std::string::npos) {
		std::cerr << "[-] Error: Failed to update storage." << std::endl;
		return;
	}
	response.erase(pos_start, start_marker.length());

	do {
		std::istringstream iss(response);
		std::string file_info;
		std::string filename;
		std::string hash;

		while (std::getline(iss, file_info, ' ') && file_info != end_marker) {
			std::istringstream file_stream(file_info);
			std::getline(file_stream, filename, ':');
			std::getline(file_stream, hash);
			if (!filename.empty() && !hash.empty()) {
				storeClientFiles(fd, filename, hash);
			}
		}
		if (file_info == end_marker) {
			break;
		}
	} while (!(response = receive(fd)).empty());
	updateFilesWithSameHash();
}

bool Connection::checkConnection(int32_t fd) {
	return getsockopt(fd, SOL_SOCKET, SO_ERROR, nullptr, nullptr);
}

void Connection::sendFileList(int32_t fd) {
	const std::string& start_marker = marker[0];
	const std::string& end_marker = marker[1];
	std::vector<std::string> files = listFiles();
	std::string list = start_marker;

	for (const auto& file : files) {
		if (list.size() + file.size() > BUFFER_SIZE) {
			sendToClient(fd, list);
			list.clear();
		}
		list += file + ' ';
	}
	sendToClient(fd, list);
	sendToClient(fd, end_marker);
}

void Connection::sendFile(int32_t fd, const std::string& filename) {

}

std::vector<std::string> Connection::listFiles() {
	std::lock_guard<std::mutex> lock(mutex);
	std::vector<std::string> list;

	for (const auto& entry : storage) {
		list.push_back(entry.second.filename);
	}
	return list;
}

std::multimap<int32_t, FileInfo> Connection::findFilename(const std::string& filename) {
	std::lock_guard<std::mutex> lock(mutex);
	std::multimap<int32_t, FileInfo> result;

	for (const auto& entry : storage) {
		if (entry.second.filename == filename) {
			result.insert(std::pair(entry.first, entry.second));
		}
	}
	return result;
}

void Connection::updateFilesWithSameHash() {
	std::lock_guard<std::mutex> lock(mutex);

	for (auto first = storage.begin(); first != storage.end(); ++first) {
		for (auto second = std::next(first); second != storage.end(); ++second) {
			if (first->second.hash == second->second.hash) {
				first->second.filename = second->second.filename;
				break;
			}
		}
	}
}

void Connection::storeClientFiles(int32_t fd, const std::string& filename, const std::string& hash) {
	std::lock_guard<std::mutex> lock(mutex);
	FileInfo data {hash, filename};

	storage.insert(std::pair(fd, data));
	std::cout << "[+] Success: Stored the file: " << filename << '.' << std::endl;
}

void Connection::removeClientFiles(int32_t fd) {
	std::lock_guard<std::mutex> lock(mutex);
	auto range = storage.equal_range(fd);

	storage.erase(range.first, range.second);
	storage.erase(fd);
}

void Connection::split(const std::string& str, char delim, std::vector<std::string>& tokens) {
	std::string token;
	std::istringstream tokenStream(str);

	while (std::getline(tokenStream, token, delim)) {
		tokens.push_back(token);
	}
}
