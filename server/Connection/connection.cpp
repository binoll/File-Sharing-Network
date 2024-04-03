#include "connection.hpp"

Connection::Connection(size_t port) : port(port) { }

Connection::~Connection() {
	close(server_fd);
}

void Connection::createConnection() {
	server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (server_fd < 0) {
		std::cerr << "[-] Error: Failed to create socket." << std::endl;
		return;
	}
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(server_fd,
	         reinterpret_cast<struct sockaddr*>(&addr),
	         sizeof(addr)) < 0) {
		std::cerr << "[-] Error: Failed to bind the socket." << std::endl;
		return;
	}
	if (listen(server_fd, 5) < 0) {
		std::cerr << "[-] Error: Failed to listen." << std::endl;
		return;
	}
	std::cout << "[*] Wait: Server is listening for connections..." << std::endl;

	while (true) {
		ssize_t client_fd;
		struct sockaddr_in addr;
		socklen_t addr_len = sizeof(addr);

		client_fd = accept(server_fd, reinterpret_cast<struct sockaddr*>(&addr), &addr_len);
		if (client_fd < 0) {
			std::cerr << "[-] Error: Failed to accept connection." << std::endl;
			continue;
		}
		std::cout << "[+] Success: Client connected. Ip: " << inet_ntoa(addr.sin_addr)
				<< " port: " << addr.sin_port << '.' << std::endl;

		switch (fork()) {
			case 0: {
				handleClient(client_fd);
				continue;
			}
			default: {
				std::cerr << "[-] Error: Failed to create child process..." << std::endl;
				close(client_fd);
				break;
			}
		}
		break;
	}
}

void Connection::handleClient(ssize_t client_fd) {
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

std::string Connection::receive(ssize_t client_fd) {
	std::byte buffer[BUFFER_SIZE];
	ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
	if (bytes_read > 0) {
		return std::string(reinterpret_cast<const char*>(buffer), bytes_read);
	}
	return "";
}

void Connection::updateStorage(ssize_t client_fd) {
	const std::string& command_list = commands_client[0];
	const std::string& command_err = commands_server[4];
	std::string response;
	size_t pos_start;

	sendToClient(client_fd, command_list);
	response = receive(client_fd);
	pos_start = response.find(start_marker);

	if (pos_start == std::string::npos) {
		std::cerr << "[-] Error: Failed to update storage." << std::endl;
		sendToClient(client_fd, command_err);
		return;
	}
	do {
		if (response.empty()) {
			continue;
		}
		response.erase(pos_start, start_marker.length());
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
	eraseFilesWithSameHash();
}

void Connection::sendToClient(ssize_t client_fd, const std::string& msg) {
	send(client_fd, msg.c_str(), msg.size(), 0);
}

void Connection::sendFileList(ssize_t client_fd) {
	std::vector<std::string> files = getFileList();
	std::string list;

	for (const auto& file : files) {
		list += file + ' ';
	}
	sendToClient(client_fd, list);
}

void Connection::sendFile(ssize_t client_fd, const std::string& filename) {
	const std::string& error = commands_server[4];
	std::unordered_map<ssize_t, FileInfo> files = findFilename(filename);

	if (files.empty()) {
		sendToClient(client_fd, error);
	}
	if (files.size() > 1) {

	}

	if (fd < 0) {
		std::cerr << "[-] Error: Failed to send file: " << filename << '.' << std::endl;
	}
	ssize_t bytes = sendfile(client_fd,);

	if (bytes == -1) {
		std::cerr << "[-] Error: Failed to send file: " << filename << '.' << std::endl;
		return;
	}
	std::cout << "[+] Success: File sent successfully: " << filename << '.' << std::endl;
}

std::vector<std::string> Connection::getFileList() {
	std::vector<std::string> files;

	for (const auto& entry : storage) {
		files.push_back(entry.second.filename);
	}
	return files;
}

std::unordered_map<ssize_t, FileInfo> Connection::findFilename(const std::string& filename) {
	std::unordered_map<ssize_t, FileInfo> result;

	for (const auto& entry : storage) {
		if (entry.second.filename == filename) {
			result[entry.first] = entry.second;
		}
	}
	return result;
}

void Connection::eraseFilesWithSameHash() {
	for (auto first = storage.begin(); first != storage.end(); ++first) {
		for (auto second = std::next(first); second != storage.end(); ++second) {
			if (first->second.hash == second->second.hash) {
				storage.erase(second->first);
				break;
			}
		}
	}
}

void Connection::storeFile(ssize_t client_fd, const std::string& filename, const std::string& hash) {
	FileInfo data {hash, filename};
	storage[client_fd] = data;

	std::cout << "[+] Success: Stored the file: " << filename << '.' << std::endl;
}

void Connection::split(const std::string& str, char delim, std::vector<std::string>& tokens) {
	std::string token;
	std::istringstream tokenStream(str);
	while (std::getline(tokenStream, token, delim)) {
		tokens.push_back(token);
	}
}
