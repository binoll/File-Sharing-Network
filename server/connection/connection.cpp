#include "connection.hpp"

Connection::Connection() {
	addr_listen.sin_family = AF_INET;
	addr_listen.sin_addr.s_addr = INADDR_ANY;

	addr_communicate.sin_family = AF_INET;
	addr_communicate.sin_addr.s_addr = INADDR_ANY;
}

Connection::~Connection() {
	close(socket_listen);
	close(socket_communicate);
}

void Connection::waitConnection() {
	int32_t port_listen = getPort();
	int32_t port_communicate = getPort();

	addr_listen.sin_port = htons(port_listen);
	addr_communicate.sin_port = htons(port_communicate);

	socket_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	socket_communicate = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (socket_listen < 0 || socket_communicate < 0) {
		std::cout << "[-] Error: Failed to create socket." << std::endl;
		return;
	}

	if (bind(socket_listen, reinterpret_cast<struct sockaddr*>(&addr_listen), sizeof(addr_listen)) < 0) {
		std::cout << "[-] Error: Failed to bind the socket for listening." << std::endl;
		return;
	}

	if (bind(socket_communicate, reinterpret_cast<struct sockaddr*>(&addr_communicate), sizeof(addr_communicate)) < 0) {
		std::cout << "[-] Error: Failed to bind the socket for communicating." << std::endl;
		return;
	}

	if (listen(socket_listen, BACKLOG) < 0 || listen(socket_communicate, BACKLOG) < 0) {
		std::cout << "[-] Error: Failed to listen." << std::endl;
		return;
	}

	std::cout << "[*] Server is listening on port for communicate: " << htons(addr_communicate.sin_port)
			<< '.' << std::endl;
	std::cout << "[*] Server is listening on port for listening: " << htons(addr_listen.sin_port)
			<< '.' << std::endl;

	while (true) {
		int32_t client_socket_listen;
		int32_t client_socket_communicate;
		struct sockaddr_in client_addr_listen { };
		struct sockaddr_in client_addr_communicate { };
		socklen_t addr_listen_len = sizeof(client_addr_listen);
		socklen_t addr_communicate_len = sizeof(client_addr_communicate);

		client_socket_communicate = accept(socket_communicate,
		                                   reinterpret_cast<struct sockaddr*>(&client_addr_communicate),
		                                   &addr_communicate_len);
		client_socket_listen = accept(socket_listen, reinterpret_cast<struct sockaddr*>(&client_addr_listen),
		                              &addr_listen_len);

		if (client_socket_listen < 0 || client_socket_communicate < 0) {
			continue;
		}

		std::cout << "[+] Success: Client connected: " << inet_ntoa(client_addr_listen.sin_addr) << ':'
				<< client_addr_listen.sin_port << ' ' << inet_ntoa(client_addr_communicate.sin_addr) << ':'
				<< client_addr_communicate.sin_port << '.' << std::endl;
		std::thread(&Connection::handleClients, this, client_socket_listen, client_socket_communicate).detach();
	}
}

bool Connection::isConnect(int32_t client_socket_listen, int32_t client_socket_communicate) {
	return checkConnection(client_socket_listen) && checkConnection(client_socket_communicate);
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

bool Connection::checkConnection(int32_t socket) {
	int32_t optval;
	socklen_t optlen = sizeof(optval);

	if (getsockopt(socket, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1 || optval != 0) {
		return false;
	}
	return true;
}

void Connection::handleClients(int32_t client_socket_listen, int32_t client_socket_communicate) {
	int64_t bytes;
	const std::string& command_list = commands_client[0];
	const std::string& command_get = commands_client[1] + ':';
	const std::string& command_exit = commands_client[2];

	if (!synchronization(client_socket_listen, client_socket_communicate)) {
		std::cout << "[-] Error: The storage could not be synchronized." << std::endl;
		return;
	}

	while (isConnect(client_socket_listen, client_socket_communicate)) {
		std::string command;
		receiveMessage(client_socket_listen, command, MSG_DONTWAIT);

		if (command == command_list) {
			bytes = sendList(client_socket_listen);
			if (bytes < 0) {
				std::cout << "[-] Error: Failed send the list of files." << std::endl;
			}
		} else if (command.find(command_get) != std::string::npos) {
			std::vector<std::string> tokens;
			split(command, ':', tokens);
			std::string filename = tokens[1];

			bytes = sendFile(client_socket_listen, filename);
			if (bytes < 0) {
				std::cout << "[-] Error: Failed send the file." << std::endl;
			} else {
				std::cout << "[+] Success: File sent successfully: " << filename << '.' << std::endl;
			}
		} else if (command == command_exit) {
			std::cout << "[+] Success: Client disconnected." << std::endl;
			break;
		}
	}
	close(client_socket_listen);
	close(client_socket_communicate);
	removeFiles(std::pair(client_socket_listen, client_socket_communicate));
	updateStorage();
	std::cout << "[+] Success: Client disconnected." << std::endl;
}

int64_t Connection::sendMessage(int32_t socket, const std::string& message, int32_t flags) {
	int64_t bytes;
	std::byte buffer[message.size()];

	std::memcpy(buffer, message.data(), message.size());
	bytes = sendBytes(
			socket,
			buffer,
			static_cast<int64_t>(message.size()),
			flags
	);
	return bytes;
}

int64_t Connection::receiveMessage(int32_t socket, std::string& message, int32_t flags) {
	int64_t bytes;
	std::byte buffer[BUFFER_SIZE];

	bytes = receiveBytes(socket, buffer, BUFFER_SIZE, flags);
	if (bytes > 0) {
		message.append(reinterpret_cast<const char*>(buffer), bytes);
	}
	return bytes;
}

int64_t Connection::sendBytes(int32_t socket, const std::byte* buffer, int64_t size, int32_t flags) {
	int64_t bytes;
	std::lock_guard<std::mutex> lock(mutex);

	bytes = send(socket, buffer, size, flags);
	return bytes;
}

int64_t Connection::receiveBytes(int32_t socket, std::byte* buffer, int64_t size, int32_t flags) {
	int64_t bytes;
	std::lock_guard<std::mutex> lock(mutex);

	bytes = recv(socket, buffer, size, flags);
	return bytes;
}

int64_t Connection::processResponse(std::string& message) {
	int64_t size;
	uint64_t pos;

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

bool Connection::synchronization(int32_t client_socket_listen, int32_t client_socket_communicate) {
	int64_t size_message;
	int64_t bytes;
	std::string message;

	bytes = receiveMessage(client_socket_listen, message, MSG_WAITFORONE);
	if (bytes < 0) {
		return false;
	}

	size_message = processResponse(message);

	while (size_message > 0) {
		std::istringstream iss(message);

		while (iss >> message) {
			std::istringstream file_stream(message);
			std::string filename;
			std::string file_size_str;
			std::string file_hash;

			std::getline(file_stream, filename, ':');
			std::getline(file_stream, file_size_str, ':');
			std::getline(file_stream, file_hash);

			try {
				int64_t file_size = std::stoll(file_size_str);
				storeFiles(std::make_pair(client_socket_listen, client_socket_communicate),
				           filename, file_size, file_hash);
			} catch (const std::invalid_argument& err) {
				std::cout << "[-] Error: Invalid argument: " << err.what() << '.' << std::endl;
				return false;
			} catch (const std::out_of_range& err) {
				std::cout << "[-] Error: Out of range: " << err.what() << '.' << std::endl;
				return false;
			}
		}

		size_message -= bytes;
		if (size_message <= 0) {
			break;
		}

		bytes = receiveMessage(client_socket_listen, message, MSG_WAITFORONE);
		if (bytes < 0) {
			return false;
		}
	}
	updateStorage();
	return true;
}

int64_t Connection::sendList(int32_t socket) {
	int64_t bytes;
	std::string message_size;
	std::vector<std::string> files = getListFiles();
	std::string list;

	list = std::accumulate(files.begin(), files.end(), std::string(),
	                       [](const std::string& a, const std::string& b) {
		                       return a + b + ' ';
	                       });

	message_size = std::to_string(list.size() + 1) + ':';
	bytes = sendMessage(socket, message_size, MSG_CONFIRM);
	if (bytes < 0) {
		return -1;
	}

	bytes = sendMessage(socket, list, MSG_CONFIRM);
	if (bytes < 0) {
		return -1;
	}

	return bytes + static_cast<int64_t>(message_size.size());
}

int64_t Connection::sendFile(int32_t socket, const std::string& filename) {
	int64_t size = getSize(filename);
	int64_t total_bytes = 0;
	std::vector<std::pair<int32_t, int32_t>> sockets = findFd(filename);

	if (size == 0) {
		int64_t bytes = sendMessage(socket, "", 0);
		return (bytes < 0) ? -1 : 0;
	}

	if (sockets.empty()) {
		return -1;
	}

	for (const auto& socket_pair : sockets) {
		int32_t client_socket_listen = socket_pair.first;
		int32_t client_socket_communicate = socket_pair.second;

		if (client_socket_listen == -1 || client_socket_communicate == -1) {
			return -1;
		}

		for (int64_t offset = 0; offset < size; offset += BUFFER_SIZE) {
			int64_t chunk_size = std::min(size - offset, static_cast<int64_t>(BUFFER_SIZE));
			std::string message = commands_server[1] + ':' + std::to_string(offset) + ':' +
					std::to_string(size) + ':' + filename;
			std::byte buffer[BUFFER_SIZE];

			int64_t bytes = sendMessage(client_socket_communicate, message, MSG_CONFIRM);
			if (bytes < 0) {
				return -1;
			}

			bytes = receiveBytes(client_socket_listen, buffer, chunk_size, MSG_DONTWAIT);
			if (bytes < 0) {
				return -1;
			}

			bytes = sendBytes(socket, buffer, bytes, MSG_CONFIRM);
			if (bytes < 0) {
				return -1;
			}

			total_bytes += bytes;
		}
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
	std::unordered_map<std::string, int> filename_counts;
	std::vector<std::string> unique_filenames;
	std::vector<std::string> duplicate_filenames;
	std::lock_guard<std::mutex> lock(mutex);

	for (const auto& entry : storage) {
		filename_counts[entry.second.filename]++;
	}

	for (const auto& entry : filename_counts) {
		if (entry.second == 1) {
			unique_filenames.push_back(entry.first);
		} else {
			duplicate_filenames.push_back(entry.first);
		}
	}

	for (const auto& filename : duplicate_filenames) {
		unique_filenames.push_back(filename);
	}
	return unique_filenames;
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
					first->second.is_filename_changed = true;
					second->second.is_filename_changed = true;
					--file_count[first->second.filename];
				}
			}
		}
	}
}

void Connection::storeFiles(std::pair<int32_t, int32_t> pair, const std::string& filename, int64_t size,
                            const std::string& hash) {
	FileInfo data {size, hash, filename, false};
	std::lock_guard<std::mutex> lock(mutex);

	storage.insert(std::pair(pair, data));
	std::cout << "[+] Success: Stored the file: " << filename << '.' << std::endl;
}

void Connection::removeFiles(std::pair<int32_t, int32_t> pair) {
	std::string filename;
	std::unordered_map<std::string, int64_t> file_count;
	std::lock_guard<std::mutex> lock(mutex);
	auto range = storage.equal_range(pair);

	if (range.first != storage.end()) {
		filename = range.first->second.filename;
	}

	storage.erase(range.first, range.second);

	for (auto& entry : storage) {
		file_count[entry.second.filename]++;
	}

	for (auto& it : storage) {
		if (it.second.is_filename_changed && it.second.filename.find(filename) == 0) {
			uint64_t pos_start = it.second.filename.rfind('(');

			if (pos_start != std::string::npos) {
				uint64_t pos_end = it.second.filename.rfind(')');

				if (pos_end != std::string::npos) {
					int64_t file_occurrences = file_count[it.second.filename];
					std::string number = it.second.filename.substr(pos_start + 1, pos_end - pos_start - 1);
					it.second.filename.replace(pos_start + 1, number.length(), std::to_string(file_occurrences));
					--file_count[it.second.filename];
				}

				if (file_count[it.second.filename] == 1) {
					it.second.filename.erase(pos_start);
				}
			}
		}
	}
}

void Connection::split(const std::string& str, char delim, std::vector<std::string>& tokens) {
	std::string token;
	std::istringstream tokenStream(str);

	while (std::getline(tokenStream, token, delim)) {
		tokens.push_back(token);
	}
}
