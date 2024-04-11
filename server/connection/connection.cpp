#include "connection.hpp"

Connection::Connection() {
	addr_listen.sin_family = AF_INET;
	addr_listen.sin_addr.s_addr = INADDR_ANY;

	addr_communicate.sin_family = AF_INET;
	addr_communicate.sin_addr.s_addr = INADDR_ANY;
}

Connection::~Connection() {
	close(sockfd_listen);
	close(sockfd_communicate);
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

bool Connection::checkConnection(int32_t sockfd) {
	int32_t optval;
	socklen_t optlen = sizeof(optval);

	if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1 || optval != 0) {
		return false;
	}
	return true;
}

void Connection::handleClients(int32_t client_sockfd_listen, int32_t client_sockfd_communicate) {
	int64_t bytes;
	const std::string& command_list = commands_client[0];
	const std::string& command_get = commands_client[1] + ':';
	const std::string& command_exit = commands_client[2];

	if (!synchronization(client_sockfd_listen, client_sockfd_communicate)) {
		std::cerr << "[-] Error: The storage could not be synchronized." << std::endl;
		return;
	}

	while (isConnect(client_sockfd_listen, client_sockfd_communicate)) {
		std::string command;
		receiveMessage(client_sockfd_listen, command, MSG_DONTWAIT);

		if (command == command_list) {
			bytes = sendList(client_sockfd_listen);
			if (bytes < 0) {
				std::cerr << "[-] Error: Failed send the list of files." << std::endl;
			}
		} else if (command.find(command_get) != std::string::npos) {
			std::vector<std::string> tokens;
			split(command, ':', tokens);
			std::string filename = tokens[1];

			bytes = sendFile(client_sockfd_listen, filename);
			if (bytes < 0) {
				std::cerr << "[-] Error: Failed send the file." << std::endl;
			} else {
				std::cout << "[+] Success: File sent successfully: " << filename << '.' << std::endl;
			}
		} else if (command == command_exit) {
			std::cout << "[+] Success: Client disconnected." << std::endl;
			break;
		}
	}
	close(client_sockfd_listen);
	close(client_sockfd_communicate);
	removeFiles(std::pair(client_sockfd_listen, client_sockfd_communicate));
	updateStorage();
	std::cout << "[+] Success: Client disconnected." << std::endl;
}

int64_t Connection::sendMessage(int32_t sockfd, const std::string& message, int32_t flags) {
	int64_t bytes;
	std::byte buffer[message.size()];

	std::memcpy(buffer, message.data(), message.size());
	bytes = sendBytes(
			sockfd,
			buffer,
			static_cast<int64_t>(message.size()),
			flags
	);
	return bytes;
}

int64_t Connection::receiveMessage(int32_t sockfd, std::string& message, int32_t flags) {
	int64_t bytes;
	std::byte buffer[BUFFER_SIZE];

	bytes = receiveBytes(sockfd, buffer, BUFFER_SIZE, flags);
	if (bytes > 0) {
		message.append(reinterpret_cast<const char*>(buffer), bytes);
	}
	return bytes;
}

int64_t Connection::sendBytes(int32_t sockfd, const std::byte* buffer, int64_t size, int32_t flags) {
	int64_t bytes;
	std::lock_guard<std::mutex> lock(mutex);

	bytes = send(sockfd, buffer, size, flags);
	return bytes;
}

int64_t Connection::receiveBytes(int32_t sockfd, std::byte* buffer, int64_t size, int32_t flags) {
	int64_t bytes;
	std::lock_guard<std::mutex> lock(mutex);

	bytes = recv(sockfd, buffer, size, flags);
	return bytes;
}

int64_t Connection::processResponse(std::string& message) {
	int64_t size;
	size_t pos;

	pos = message.find(':');
	if (pos == std::string::npos) {
		return -1;
	}

	try {
		size = std::stoll(message.substr(0, pos));
	} catch (const std::exception& err) {
		return -1;
	}
	message = message.substr(pos + 1);
	return size;
}

/*
 * 1. client (send size) -> server
 * 2. client (send list) -> server
 * 3. server (store files)
 */
bool Connection::synchronization(int32_t client_sockfd_listen, int32_t client_sockfd_communicate) {
	int64_t bytes;
	int64_t size_message;
	std::string data;
	std::string filename;
	std::string file_hash;
	std::string file_size;
	std::string buffer;

	bytes = receiveMessage(client_sockfd_listen, buffer, MSG_WAITFORONE);
	if (bytes < 0) {
		return false;
	}

	size_message = processResponse(buffer);

	while (true) {
		std::istringstream iss(buffer);

		while (std::getline(iss, data, ' ')) {
			std::istringstream file_stream(data);
			int64_t size;

			std::getline(file_stream, filename, ':');
			std::getline(file_stream, file_size, ':');
			std::getline(file_stream, file_hash);

			try {
				size = static_cast<int64_t>(std::stoull(file_size));
			} catch (const std::invalid_argument& err) {
				std::cerr << "[-] Error: Invalid argument: " << err.what() << std::endl;
				return false;
			} catch (const std::out_of_range& err) {
				std::cerr << "[-] Error: Out of range: " << err.what() << std::endl;
				return false;
			}

			if (!filename.empty() && !file_hash.empty()) {
				storeFiles(std::pair(client_sockfd_listen, client_sockfd_communicate), filename, size, file_hash);
			}
		}

		if ((size_message -= bytes) <= 0) {
			break;
		}

		bytes = receiveMessage(client_sockfd_listen, buffer, MSG_WAITFORONE);
		if (bytes < 0) {
			return false;
		}
	}
	updateStorage();
	return true;
}

int64_t Connection::sendList(int32_t sockfd) {
	int64_t bytes;
	int64_t total_bytes;
	std::string list;
	std::string message_size;
	std::vector<std::string> files;

	bytes = 0;
	total_bytes = 0;
	files = getListFiles();

	for (const auto& file : files) {
		list += file + ' ';
		total_bytes += bytes;
	}

	bytes = static_cast<int64_t>(list.size() + std::to_string(list.size() + ':').size());
	message_size = std::to_string(bytes) + ':';
	bytes = sendMessage(sockfd, message_size, MSG_CONFIRM);
	if (bytes < 0) {
		return -1;
	}

	bytes = sendMessage(sockfd, list, MSG_CONFIRM);
	if (bytes < 0) {
		return -1;
	}
	total_bytes += bytes;
	return total_bytes;
}

int64_t Connection::sendFile(int32_t sockfd, const std::string& filename) {
	int64_t size;
	int64_t bytes;
	int64_t total_bytes;
	int32_t client_sockfd_listen;
	int32_t client_sockfd_communicate;
	std::string message;
	std::string message_size;
	std::vector<std::pair<int32_t, int32_t>> sockfds;

	total_bytes = 0;
	size = getSize(filename);
	sockfds = findFd(filename);

	if (sockfds.size() > 1) {
		uint64_t size_vector = sockfds.size();
		uint64_t index = 0;

		while (true) {
			std::pair<int32_t, int32_t> pair = sockfds[index++ % size_vector];
			client_sockfd_listen = pair.first;
			client_sockfd_communicate = pair.second;

			if (client_sockfd_listen == -1 || client_sockfd_communicate == -1) {
				return -1;
			}

			if (size == 0) {
				bytes = sendMessage(sockfd, "", 0);
				if (bytes < 0) {
					return -1;
				}
				return 0;
			}

			for (int64_t offset = 0; offset < size; offset += bytes) {
				message = commands_server[1] + ':' + std::to_string(offset) + ':'
						+ std::to_string(size) + ':' + filename;
				std::byte buffer[BUFFER_SIZE];

				bytes = sendMessage(client_sockfd_communicate, message, MSG_CONFIRM);
				if (bytes < 0) {
					return -1;
				}

				bytes = receiveBytes(client_sockfd_listen, buffer, size, MSG_WAITFORONE);
				if (bytes < 0) {
					return -1;
				}

				bytes = sendBytes(sockfd, buffer, bytes, MSG_CONFIRM);
				if (bytes < 0) {
					return -1;
				}
				total_bytes += bytes;
			}
		}
	} else if (sockfds.size() == 1) {
		client_sockfd_listen = sockfds[0].first;
		client_sockfd_communicate = sockfds[0].second;

		if (client_sockfd_listen == -1 || client_sockfd_communicate == -1) {
			return -1;
		}

		if (size == 0) {
			bytes = sendMessage(sockfd, "", 0);
			if (bytes < 0) {
				return -1;
			}
			return 0;
		}

		for (int64_t offset = 0; offset < size; offset += bytes) {
			message = commands_server[1] + ':' + std::to_string(offset) + ':'
					+ std::to_string(size) + ':' + filename;
			std::byte buffer[BUFFER_SIZE];

			bytes = sendMessage(client_sockfd_communicate, message, MSG_CONFIRM);
			if (bytes < 0) {
				return -1;
			}

			bytes = receiveBytes(client_sockfd_listen, buffer, size, MSG_WAITFORONE);
			if (bytes < 0) {
				return -1;
			}

			bytes = sendBytes(sockfd, buffer, bytes, MSG_CONFIRM);
			if (bytes < 0) {
				return -1;
			}
			total_bytes += bytes;
		}
	} else {
		return -1;
	}
	return total_bytes;
}

std::vector<std::pair<int32_t, int32_t>> Connection::findFd(const std::string& filename) {
	std::vector<std::pair<int32_t, int32_t>> vector;
	std::lock_guard<std::mutex> lock(mutex);

	for (const auto& item : storage) {
		if (item.second.filename == filename) {
			vector.emplace_back(item.first);
		}
	}
	return vector;
}

std::vector<std::string> Connection::getListFiles() {
	std::vector<std::string> vector;
	std::lock_guard<std::mutex> lock(mutex);
	std::unordered_map<std::string, int> filename_counts;

	for (const auto& entry : storage) {
		filename_counts[entry.second.filename]++;
	}

	for (const auto& entry : storage) {
		if (filename_counts[entry.second.filename] == 1) {
			vector.push_back(entry.second.filename);
		}
	}

	for (const auto& pair : filename_counts) {
		if (pair.second > 1) {
			vector.push_back(pair.first);
		}
	}
	return vector;
}

int64_t Connection::getSize(const std::string& filename) {
	std::lock_guard<std::mutex> lock(mutex);

	for (const auto& it : storage) {
		if (it.second.filename == filename) {
			return it.second.size;
		}
	}
	return -1;
}

void Connection::updateStorage() {
	std::unordered_map<std::string, int64_t> file_count;
	std::lock_guard<std::mutex> lock(mutex);

	for (auto& entry : storage) {
		file_count[entry.second.filename]++;
	}

	for (auto first = storage.begin(); first != storage.end(); ++first) {
		for (auto second = std::next(first); second != storage.end(); ++second) {
			if (first->second.hash == second->second.hash &&
					first->second.filename != second->second.filename) {
				second->second.filename = first->second.filename;
			}

			if (first->second.hash != second->second.hash &&
					first->second.filename == second->second.filename) {
				int64_t file_occurrences = file_count[first->second.filename];

				if (file_occurrences > 1) {
					first->second.filename += '(' + std::to_string(file_occurrences) + ')';
					second->second.filename += '(' + std::to_string(file_occurrences - 1) + ')';
					file_count[first->second.filename]--;
				}
			}
		}
	}
}

void Connection::storeFiles(std::pair<int32_t, int32_t> pair, const std::string& filename, int64_t size,
                            const std::string& hash) {
	FileInfo data {size, hash, filename};
	std::lock_guard<std::mutex> lock(mutex);

	storage.insert(std::pair(pair, data));
	std::cout << "[+] Success: Stored the file: " << filename << '.' << std::endl;
}

void Connection::removeFiles(std::pair<int32_t, int32_t> pair) {
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
