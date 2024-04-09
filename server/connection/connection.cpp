#include "connection.hpp"

Connection::Connection() {
	addr_listen.sin_family = AF_INET;
	addr_listen.sin_addr.s_addr = INADDR_ANY;

	addr_communicate.sin_family = AF_INET;
	addr_communicate.sin_addr.s_addr = INADDR_ANY;
}

Connection::~Connection() {
	close(sockfd_listen);
}

void Connection::waitConnection() {
	int32_t port_listen = getPort();
	int32_t port_communicate = getPort();

	addr_listen.sin_port = htons(port_listen);
	addr_communicate.sin_port = htons(port_communicate);

	sockfd_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockfd_communicate = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sockfd_listen < 0 || sockfd_communicate < 0) {
		std::cerr << "[-] Error: Failed to create socket." << std::endl;
		return;
	}

	if (bind(sockfd_listen, reinterpret_cast<struct sockaddr*>(&addr_listen), sizeof(addr_listen)) < 0) {
		std::cerr << "[-] Error: Failed to bind the socket for listening." << std::endl;
		return;
	}

	if (bind(sockfd_communicate, reinterpret_cast<struct sockaddr*>(&addr_communicate), sizeof(addr_communicate)) < 0) {
		std::cerr << "[-] Error: Failed to bind the socket for communicating." << std::endl;
		return;
	}

	if (listen(sockfd_listen, BACKLOG) < 0 || listen(sockfd_communicate, BACKLOG) < 0) {
		std::cerr << "[-] Error: Failed to listen." << std::endl;
		return;
	}

	std::cout << "[*] Wait: server is listening on port for communicate: " << htons(addr_communicate.sin_port)
			<< '.' << std::endl;
	std::cout << "[*] Wait: server is listening on port for listening: " << htons(addr_listen.sin_port)
			<< '.' << std::endl;

	while (true) {
		int32_t client_sockfd_listen;
		int32_t client_sockfd_communicate;
		struct sockaddr_in client_addr_listen { };
		struct sockaddr_in client_addr_communicate { };
		socklen_t addr_listen_len = sizeof(client_addr_listen);
		socklen_t addr_communicate_len = sizeof(client_addr_communicate);

		client_sockfd_communicate = accept(sockfd_communicate,
		                                   reinterpret_cast<struct sockaddr*>(&client_addr_communicate),
		                                   &addr_communicate_len);
		client_sockfd_listen = accept(sockfd_listen, reinterpret_cast<struct sockaddr*>(&client_addr_listen),
		                              &addr_listen_len);

		if (client_sockfd_listen < 0 || client_sockfd_communicate < 0) {
			continue;
		}

		std::cout << "[+] Success: Client connected: " << inet_ntoa(client_addr_listen.sin_addr) << ':'
				<< client_addr_listen.sin_port << ' ' << inet_ntoa(client_addr_communicate.sin_addr) << ':'
				<< client_addr_communicate.sin_port << '.' << std::endl;
		std::thread(&Connection::handleClients, this, client_sockfd_listen, client_sockfd_communicate).detach();
	}
}

bool Connection::isConnect(int32_t client_sockfd_listen, int32_t client_sockfd_communicate) {
	return checkConnection(client_sockfd_listen) && checkConnection(client_sockfd_communicate);
}

int32_t Connection::getPort() {
	sockaddr_in new_addr { };
	new_addr.sin_family = AF_INET;
	new_addr.sin_addr.s_addr = INADDR_ANY;
	new_addr.sin_port = 0;

	int32_t fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		return -1;
	}
	if (bind(fd, reinterpret_cast<sockaddr*>(&new_addr), sizeof(new_addr)) < 0) {
		close(fd);
		return -1;
	}
	socklen_t addr_len = sizeof(new_addr);
	if (getsockname(fd, reinterpret_cast<sockaddr*>(&new_addr), &addr_len) < 0) {
		close(fd);
		return -1;
	}
	int32_t port = ntohs(new_addr.sin_port);
	close(fd);
	return port;
}

void Connection::handleClients(int32_t client_sockfd_listen, int32_t client_sockfd_communicate) {
	int64_t bytes;
	std::string command;
	const std::string& command_list = commands_client[0];
	const std::string& command_get = commands_client[1] + ':';
	const std::string& command_exit = commands_client[2];

	if (!synchronizationStorage(client_sockfd_listen, client_sockfd_communicate)) {
		std::cerr << "[-] Error: The storage could not be synchronized." << std::endl;
	}

	while (isConnect(client_sockfd_listen, client_sockfd_communicate)) {
		command = receiveData(client_sockfd_listen, MSG_DONTWAIT);

		if (command == command_list) {
			if (sendListFiles(client_sockfd_listen) < 0) {
				std::cerr << "[-] Error: Failed send the list of files." << std::endl;
			}
		} else if (command.find(command_get) != std::string::npos) {
			std::vector<std::string> tokens;
			split(command, ':', tokens);
			std::string filename = tokens[1];

			bytes = sendFile(client_sockfd_listen, filename);
			if (bytes < 0) {
				std::cerr << "[-] Error: Failed send the file." << std::endl;
			}
			std::cout << "[+] Success: File sent successfully: " << filename << '.' << std::endl;
		} else if (command == command_exit) {
			std::cout << "[+] Success: Client disconnected." << std::endl;
			break;
		}
	}
	close(client_sockfd_listen);
	close(client_sockfd_communicate);
	removeClientFiles(std::pair(client_sockfd_listen, client_sockfd_communicate));
}

int64_t Connection::sendData(int32_t sockfd, const std::string& command, int32_t flags) {
	int64_t bytes;

	bytes = send(sockfd, command.c_str(), command.size(), flags);
	return bytes;
}

std::string Connection::receiveData(int32_t sockfd, int32_t flags) {
	std::byte buffer[BUFFER_SIZE];
	int64_t bytes;
	std::string receive_data;

	bytes = recv(sockfd, buffer, BUFFER_SIZE, flags);
	if (bytes > 0) {
		receive_data = std::string(reinterpret_cast<char*>(buffer), bytes);
	}
	return receive_data;
}

bool Connection::synchronizationStorage(int32_t client_sockfd_listen, int32_t client_sockfd_communicate) {
	std::string response;
	uint64_t pos;

	response = receiveData(client_sockfd_listen, MSG_WAITFORONE);
	pos = response.find(start_marker);

	if (pos == std::string::npos) {
		return false;
	}
	response.erase(pos, start_marker.length());

	do {
		std::istringstream iss(response);
		std::string data;
		std::string filename;
		std::string hash;
		int64_t size;

		while (std::getline(iss, data, ' ') && data != end_marker) {
			std::istringstream file_stream(data);
			std::getline(file_stream, filename, ':');
			std::string size_str;
			std::getline(file_stream, size_str, ':');
			size = static_cast<int64_t>(std::stoull(size_str));

			std::getline(file_stream, hash);
			if (!filename.empty() && !hash.empty()) {
				storeFiles(std::pair(client_sockfd_listen, client_sockfd_communicate), filename, size, hash);
			}
		}
		if (data == end_marker) {
			break;
		}
	} while (!(response = receiveData(client_sockfd_listen, MSG_WAITFORONE)).empty());
	updateFilesWithSameHash();
	return true;
}

bool Connection::checkConnection(int32_t sockfd) {
	int32_t optval;
	socklen_t optlen = sizeof(optval);

	if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1 || optval != 0) {
		return false;
	}
	return true;
}

int64_t Connection::sendListFiles(int32_t sockfd) {
	std::vector<std::string> files = getListFiles();
	std::string list;
	int64_t bytes = 0;
	int64_t send_bytes;

	send_bytes = sendData(sockfd, start_marker, MSG_CONFIRM);
	if (send_bytes < 0) {
		return -1;
	}

	for (const auto& file : files) {
		list += file + ' ';
		if (list.size() + file.size() > BUFFER_SIZE) {
			send_bytes = sendData(sockfd, list, MSG_CONFIRM);
			if (send_bytes < 0) {
				return -1;
			}
			bytes += send_bytes;
			list.clear();
		}
	}

	send_bytes = sendData(sockfd, list, MSG_CONFIRM);
	if (send_bytes < 0) {
		return -1;
	}
	bytes += send_bytes;

	send_bytes = sendData(sockfd, end_marker, MSG_CONFIRM);
	if (send_bytes < 0) {
		return -1;
	}
	return bytes;
}

int64_t Connection::sendFile(int32_t sockfd, const std::string& filename) {
	std::multimap<std::pair<int32_t, int32_t>, FileInfo> files = getFilename(filename);
	std::pair<int32_t, int32_t> pair = findFd(filename);
	std::byte buffer[BUFFER_SIZE];
	uint64_t size = getSize(filename);
	int64_t send_bytes;
	int64_t read_bytes;
	int64_t bytes = 0;

	if (files.empty()) {
		return -1;
	}

	send_bytes = sendData(sockfd, start_marker, MSG_CONFIRM);
	if (send_bytes < 0) {
		return -1;
	}

	for (uint64_t offset = 0; bytes < size; offset += read_bytes) {
		read_bytes = getFile(pair.first, filename, buffer, offset, BUFFER_SIZE);
		if (read_bytes > 0) {
			std::string data(reinterpret_cast<char*>(buffer), read_bytes);
			send_bytes = sendData(sockfd, data, MSG_CONFIRM);
			if (send_bytes != read_bytes) {
				return -1;
			}
			bytes += send_bytes;
			continue;
		}
		break;
	}

	send_bytes = sendData(sockfd, end_marker, MSG_CONFIRM);
	if (send_bytes < 0) {
		return -1;
	}
	return bytes;
}

int64_t Connection::getFile(int32_t sockfd, const std::string& filename, std::byte* buffer,
                            uint64_t offset, uint64_t max_size) {
	const std::string& command_part = commands_server[2] + ':' + std::to_string(offset) + ':' +
			std::to_string(max_size) + ':' + filename;
	std::string response;
	int64_t bytes = 0;
	int64_t send_bytes;
	uint64_t pos;

	send_bytes = sendData(sockfd, command_part, MSG_CONFIRM);
	if (send_bytes < 0) {
		return -1;
	}

	response = receiveData(sockfd, MSG_WAITFORONE);
	pos = response.find(start_marker);
	if (pos != std::string::npos) {
		response.erase(pos, start_marker.length());
	}

	do {
		pos = response.find(end_marker);
		if (pos == std::string::npos) {
			std::memcpy(buffer, response.data(), std::min(response.size(), static_cast<std::size_t>(BUFFER_SIZE)));
			bytes += static_cast<int64_t>(response.size());
			continue;
		}
		response.erase(pos, end_marker.length());
		std::memcpy(buffer, response.data(), std::min(response.size(), static_cast<std::size_t>(BUFFER_SIZE)));
		bytes += static_cast<int64_t>(response.size());
	} while (!(response = receiveData(sockfd, MSG_WAITFORONE)).empty());
	return bytes;
}

std::pair<int32_t, int32_t> Connection::findFd(const std::string& filename) {
	for (const auto& it : storage) {
		if (it.second.filename == filename) {
			return it.first;
		}
	}
	return std::make_pair(-1, -1);
}

std::vector<std::string> Connection::getListFiles() {
	std::vector<std::string> list;
	std::lock_guard<std::mutex> lock(mutex);

	for (const auto& entry : storage) {
		list.push_back(entry.second.filename);
	}
	return list;
}

std::multimap<std::pair<int32_t, int32_t>, FileInfo> Connection::getFilename(const std::string& filename) {
	std::multimap<std::pair<int32_t, int32_t>, FileInfo> result;
	std::lock_guard<std::mutex> lock(mutex);

	for (const auto& entry : storage) {
		if (entry.second.filename == filename) {
			result.insert(std::make_pair(entry.first, entry.second));
		}
	}
	return result;
}

int64_t Connection::getSize(const std::string& filename) {
	std::lock_guard<std::mutex> lock(mutex);

	for (const auto& it : storage) {
		if (it.second.hash == filename) {
			return it.second.size;
		}
	}
	return -1;
}

void Connection::updateFilesWithSameHash() {
	std::lock_guard<std::mutex> lock(mutex);

	for (auto first = storage.begin(); first != storage.end(); ++first) {
		for (auto second = std::next(first); second != storage.end(); ++second) {
			if (first->second.hash == second->second.hash) {
				second->second.filename = first->second.filename;
				break;
			}
		}
	}
}

void Connection::storeFiles(std::pair<int32_t, int32_t> pair, const std::string& filename, int64_t size,
                            const std::string& hash) {
	std::lock_guard<std::mutex> lock(mutex);
	FileInfo data {size, hash, filename};

	storage.insert(std::pair(pair, data));
	std::cout << "[+] Success: Stored the file: " << filename << '.' << std::endl;
}

void Connection::removeClientFiles(std::pair<int32_t, int32_t> pair) {
	std::lock_guard<std::mutex> lock(mutex);
	auto range = storage.equal_range(pair);

	storage.erase(range.first, range.second);
}

void Connection::split(const std::string& str, char delim, std::vector<std::string>& tokens) {
	std::string token;
	std::istringstream tokenStream(str);

	while (std::getline(tokenStream, token, delim)) {
		tokens.push_back(token);
	}
}
