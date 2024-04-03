#include "connection.hpp"

Connection::Connection(uint16_t port) : server_port(port), pool(BACKLOG) {
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

	server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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
	pool.~ThreadPool();
	close(server_fd);
}

void Connection::waitConnect() {
	std::cout << "[*] Wait: Server is listening for connections..." << std::endl;

	while (true) {
		int32_t client_fd;
		struct sockaddr_in client_addr;
		socklen_t addr_len = sizeof(client_addr);

		client_fd = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
		if (client_fd < 0) {
			std::cerr << "[-] Error: Failed to accept connection." << std::endl;
			continue;
		}
		std::cout << "[+] Success: Client connected: " << inet_ntoa(client_addr.sin_addr)
				<< ':' << client_addr.sin_port << '.' << std::endl;

		pool.enqueue([this, client_fd] { handleClient(client_fd); });
	}
}

void Connection::handleClient(int32_t client_fd) {
	std::string command;
	const std::string& command_list = commands_client[0];
	const std::string& command_get = commands_client[1] + ':';
	const std::string& command_exit = commands_client[2];

	updateStorage(client_fd);
	while (!(command = receive(client_fd)).empty()) {
		if (command.find(command_list) == 0) {
			sendFileList(client_fd);
		} else if (command.find(command_get) == 0) {
			std::vector<std::string> tokens;
			split(command, ':', tokens);
			std::string filename = tokens[1];
			sendFile(client_fd, filename);
		} else if (command.find(command_exit) == 0) {
			close(client_fd);
			std::cout << "[+] Success: Client disconnected." << std::endl;
			break;
		} else {
			std::cerr << "[-] Error: Invalid command." << std::endl;
		}
	}
}

std::string Connection::receive(int32_t client_fd) {
	std::byte buffer[BUFFER_SIZE];
	ssize_t bytes = recv(client_fd, buffer, BUFFER_SIZE, 0);
	if (bytes > 0) {
		return std::string(reinterpret_cast<const char*>(buffer), bytes);
	}
	return "";
}

void Connection::updateStorage(int32_t client_fd) {
	const std::string& command_list = commands_client[0];
	std::string response;
	size_t pos_start;

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

void Connection::sendToClient(int32_t client_fd, const std::string& msg) {
	ssize_t bytes = send(client_fd, msg.c_str(), msg.size(), 0);
	if (bytes == -1) {
		std::cerr << "[-] Error: Failed to send msg." << std::endl;
	}
}

void Connection::sendFileList(int32_t client_fd) {
	std::vector<std::string> files = getFilesList();
	std::string list = start_marker;
	for (const auto& file : files) {
		if (list.size() + file.size() > BUFFER_SIZE) {
			sendToClient(client_fd, list);
			list.clear();
		}
		list += file + ' ';
	}
	list += end_marker;
	sendToClient(client_fd, list);
}

void Connection::sendFile(int32_t client_fd, const std::string& filename) {

}

std::vector<std::string> Connection::getFilesList() {
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
