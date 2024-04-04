#include "connection.hpp"

Connection::Connection(uint16_t port) : server_port(port) {
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

		synchronizationStorage(client_fd);
		std::thread(&Connection::handleClient, this, client_fd).detach();
		break;
	}
}

void Connection::handleClient(int32_t client_fd) {
	std::string command;
	const std::string& command_list = commands_client[0];
	const std::string& command_get = commands_client[1] + ':';
	const std::string& command_exit = commands_client[2];

	while (checkConnection(client_fd) > 0) {
		command = receive(client_fd);

		if (command == command_list) {
			sendFileList(client_fd);
		} else if (command == command_get) {
			std::vector<std::string> tokens;
			split(command, ':', tokens);
			std::string filename = tokens[1];
			sendFile(client_fd, filename);
		} else if (command == command_exit) {
			close(client_fd);
			std::cout << "[+] Success: Client disconnected." << std::endl;
			break;
		}
	}
	removeClientFiles(client_fd);
}

std::string Connection::receive(int32_t client_fd) {
	const std::string& command_err = commands_server[4];
	std::byte buffer[BUFFER_SIZE];
	std::string receive_data;
	ssize_t bytes;

	bytes = recv(client_fd, buffer, BUFFER_SIZE, MSG_WAITFORONE);
	receive_data = std::string(reinterpret_cast<char*>(buffer), bytes);
	if (bytes > 0) {
		return receive_data;
	}
	return command_err;
}

ssize_t Connection::sendToClient(int32_t client_fd, const std::string& command) {
	ssize_t bytes;

	bytes = send(client_fd, command.c_str(), command.size(), MSG_CONFIRM);
	if (bytes == -1) {
		std::cerr << "[-] Error: Failed to send command." << std::endl;
	}
	return bytes;
}

void Connection::synchronizationStorage(int32_t client_fd) {
	const std::string& start_marker = marker[0];
	const std::string& end_marker = marker[1];
	std::string response;
	size_t pos_start;

	response = receive(client_fd);
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
				storeClientFiles(client_fd, filename, hash);
			}
		}
		if (file_info == end_marker) {
			break;
		}
	} while (!(response = receive(client_fd)).empty());
	updateFilesWithSameHash();
}

bool Connection::checkConnection(int32_t client_fd) {
	return getsockopt(client_fd, SOL_SOCKET, SO_ERROR, nullptr, nullptr);
}

void Connection::sendFileList(int32_t client_fd) {
	const std::string& start_marker = marker[0];
	const std::string& end_marker = marker[1];
	std::vector<std::string> files = listFiles();
	std::string list = start_marker;

	for (const auto& file : files) {
		if (list.size() + file.size() > BUFFER_SIZE) {
			sendToClient(client_fd, list);
			list.clear();
		}
		list += file + ' ';
	}
	sendToClient(client_fd, list);
	sendToClient(client_fd, end_marker);
}

void Connection::sendFile(int32_t client_fd, const std::string& filename) {

}

std::vector<std::string> Connection::listFiles() {
	std::vector<std::string> list;
	std::lock_guard<std::mutex> lock(mutex);

	for (const auto& entry : storage) {
		list.push_back(entry.second.filename);
	}
	return list;
}

std::multimap<int32_t, FileInfo> Connection::findFilename(const std::string& filename) {
	std::multimap<int32_t, FileInfo> result;
	std::lock_guard<std::mutex> lock(mutex);

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

void Connection::storeClientFiles(int32_t client_fd, const std::string& filename, const std::string& hash) {
	FileInfo data {hash, filename};
	std::lock_guard<std::mutex> lock(mutex);

	storage.insert(std::pair(client_fd, data));
	std::cout << "[+] Success: Stored the file: " << filename << '.' << std::endl;
}

void Connection::removeClientFiles(int32_t client_fd) {
	auto range = storage.equal_range(client_fd);
	std::lock_guard<std::mutex> lock(mutex);

	storage.erase(range.first, range.second);
	storage.erase(client_fd);
}

void Connection::split(const std::string& str, char delim, std::vector<std::string>& tokens) {
	std::string token;
	std::istringstream tokenStream(str);

	while (std::getline(tokenStream, token, delim)) {
		tokens.push_back(token);
	}
}
