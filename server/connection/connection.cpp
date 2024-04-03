#include "connection.hpp"

Connection::Connection(uint16_t port) : server_port(port) {
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(server_port);

	server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if (server_fd < 0) {
		std::cerr << "[-] Error: Failed to create socket." << std::endl;
		return;
	}
	if (bind(server_fd,
	         reinterpret_cast<struct sockaddr*>(&server_addr),
	         sizeof(server_addr)) < 0) {
		std::cerr << "[-] Error: Failed to bind the socket." << std::endl;
		return;
	}
	if (listen(server_fd, BACKLOG) < 0) {
		std::cerr << "[-] Error: Failed to listen." << std::endl;
		return;
	}
}

Connection::~Connection() {
	close(server_fd);
}

void Connection::waitConnect() {
	int32_t client_fd;
	struct sockaddr_in client_addr;
	socklen_t addr_len = sizeof(client_addr);

	std::cout << "[*] Wait: server is listening for connections..." << std::endl;
	while (true) {
		client_fd = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
		if (client_fd < 0) {
			continue;
		}
		std::cout << "[+] Success: Client connected: " << inet_ntoa(client_addr.sin_addr)
				<< ':' << client_addr.sin_port << '.' << std::endl;
		updateStorage(client_fd);
		std::thread(&Connection::handleClient, this, client_fd).detach();
	}
}

void Connection::handleClient(int32_t client_fd) {
	std::string command;
	const std::string& command_list = commands_client[0];
	const std::string& command_get = commands_client[1] + ':';
	const std::string& command_exit = commands_client[2];

	while (!(command = receive(client_fd)).empty()) {
		if (command.find(command_list) == 0) {
			updateStorage(client_fd);
			sendFileList(client_fd);
		} else if (command.find(command_get) == 0) {
			std::vector<std::string> tokens;
			split(command, ':', tokens);
			std::string filename = tokens[1];
			sendFile(client_fd, filename);
		} else if (command.find(command_exit) == 0) {
			updateStorage(client_fd);
			close(client_fd);
			std::cout << "[+] Success: Client disconnected." << std::endl;
		} else {
			std::cerr << "[-] Error: Invalid command." << std::endl;
		}
	}
}

std::string Connection::receive(int32_t client_fd) {
	std::byte buffer[BUFFER_SIZE];
	ssize_t bytes;
	const std::string& command_err = commands_server[4];

	while (true) {
		bytes = recv(client_fd, buffer, BUFFER_SIZE, MSG_CONFIRM | MSG_DONTWAIT);
		if (bytes > 0) {
			return std::string(reinterpret_cast<char*>(buffer), bytes);
		} else if (bytes == 0) {
			break;
		}
	}
	return command_err;
}

void Connection::updateStorage(int32_t client_fd) {
	std::string response;
	size_t pos_start;
	const std::string& command_list = commands_client[0];

	sendToClient(client_fd, command_list);
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
				storeFile(client_fd, filename, hash);
			}
		}
		if (file_info == end_marker) {
			break;
		}
	} while (!(response = receive(client_fd)).empty());
	updateFilesWithSameHash();
}

ssize_t Connection::sendToClient(int32_t client_fd, const std::string& command) {
	ssize_t bytes = send(client_fd, command.c_str(), command.size(), MSG_CONFIRM | MSG_DONTWAIT);
	if (bytes == -1) {
		std::cerr << "[-] Error: Failed to send command." << std::endl;
		return -1;
	}
	return bytes;
}

void Connection::sendFileList(int32_t client_fd) {
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
	for (const auto& entry : storage) {
		list.push_back(entry.second.filename);
	}
	return list;
}

std::multimap<int32_t, FileInfo> Connection::findFilename(const std::string& filename) {
	std::multimap<int32_t, FileInfo> result;
	for (const auto& entry : storage) {
		if (entry.second.filename == filename) {
			result.insert(std::pair(entry.first, entry.second));
		}
	}
	return result;
}

void Connection::updateFilesWithSameHash() {
	for (auto first = storage.begin(); first != storage.end(); ++first) {
		for (auto second = std::next(first); second != storage.end(); ++second) {
			if (first->second.hash == second->second.hash) {
				first->second.filename = second->second.filename;
				break;
			}
		}
	}
}

void Connection::storeFile(int32_t client_fd, const std::string& filename, const std::string& hash) {
	FileInfo data {hash, filename};
	storage.insert(std::pair(client_fd, data));
	std::cout << "[+] Success: Stored the file: " << filename << '.' << std::endl;
}

void Connection::split(const std::string& str, char delim, std::vector<std::string>& tokens) {
	std::string token;
	std::istringstream tokenStream(str);
	while (std::getline(tokenStream, token, delim)) {
		tokens.push_back(token);
	}
}
