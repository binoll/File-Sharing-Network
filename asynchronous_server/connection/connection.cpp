// Copyright 2024 binoll
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

	std::thread(&Connection::handleClients, this).detach();

	while (true) {
		int32_t client_socket_listen;
		int32_t client_socket_communicate;
		struct sockaddr_in client_addr_listen { };
		struct sockaddr_in client_addr_communicate { };
		socklen_t addr_listen_len = sizeof(client_addr_listen);
		socklen_t addr_communicate_len = sizeof(client_addr_communicate);
		std::pair<int32_t, int32_t> pair;

		client_socket_communicate = accept(socket_communicate,
		                                   reinterpret_cast<struct sockaddr*>(&client_addr_communicate),
		                                   &addr_communicate_len);
		client_socket_listen = accept(socket_listen, reinterpret_cast<struct sockaddr*>(&client_addr_listen),
		                              &addr_listen_len);

		if (client_socket_listen < 0 || client_socket_communicate < 0) {
			continue;
		}

		pair = {client_socket_listen, client_socket_communicate};
		std::cout << "[+] Success: Client connected: " << inet_ntoa(client_addr_listen.sin_addr) << ':'
				<< client_addr_listen.sin_port << ' ' << inet_ntoa(client_addr_communicate.sin_addr) << ':'
				<< client_addr_communicate.sin_port << '.' << std::endl;

		if (!synchronization(pair)) {
			std::cout << "[-] Error: The storage could not be synchronized." << std::endl;
			removeClients(pair);
			std::cout << "[+] Success: Client disconnected." << std::endl;
		}
	}
}

bool Connection::isConnect(std::pair<int32_t, int32_t> pair) {
	return checkConnection(pair.first) && checkConnection(pair.second);
}

void Connection::hasDataToRead(
		boost::coroutines::asymmetric_coroutine<std::pair<int32_t, int32_t>>::push_type& yield
) {
	fd_set fds;
	FD_ZERO(&fds);
	int32_t maxfd_listen = 0;

	while (true) {

		for (const auto& pair : sockets) {
			FD_SET(pair.first, &fds);
			maxfd_listen = std::max(maxfd_listen, pair.first);
		}

		struct timeval timeout { };
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		int32_t result = select(maxfd_listen + 1, &fds, nullptr, nullptr, &timeout);

		if (result < 0) {
			yield({-1, -1});
		} else if (result == 0) {
			yield({0, 0});
		} else {
			for (const auto& pair : sockets) {
				if (FD_ISSET(pair.first, &fds)) {
					yield(pair);
				}
			}
		}
	}
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

void Connection::handleClients() {
	std::pair<int32_t, int32_t> pair;
	std::pair<int32_t, int32_t> empty_pair = {0, 0};
	std::pair<int32_t, int32_t> invalid_pair = {-1, -1};

	while (true) {
		if (sockets.empty()) {
			continue;
		}

		boost::coroutines::asymmetric_coroutine<std::pair<int32_t, int32_t>>::pull_type coroutine_read(
				[&](boost::coroutines::asymmetric_coroutine<std::pair<int32_t, int32_t>>::push_type& yield) {
					hasDataToRead(yield);
				}
		);
		pair = coroutine_read.get();

		if (isConnect(pair) && pair == empty_pair) {
			continue;
		} else if (!isConnect(pair) && pair == invalid_pair) {
			removeClients(pair);
			std::cout << "[+] Success: Client disconnected." << std::endl;
			continue;
		}

		boost::coroutines::asymmetric_coroutine<void>::pull_type coroutine_processing(
				[&](boost::coroutines::asymmetric_coroutine<void>::push_type& yield) {
					processingClient(yield, pair);
				}
		);
		coroutine_processing();
	}
}

void Connection::processingClient(boost::coroutines::asymmetric_coroutine<void>::push_type& yield,
                                  std::pair<int32_t, int32_t> pair) {
	int64_t bytes;
	const std::string& command_list = commands[0];
	const std::string& command_get = commands[1] + ':';
	const std::string& command_exit = commands[2];
	const std::string& command_error = commands[3];
	std::string command;

	receiveMessage(pair.first, command, MSG_NOSIGNAL);

	if (command == command_list) {
		bytes = sendList(pair.first);

		if (bytes < 0) {
			sendMessage(pair.first, command_error, MSG_CONFIRM | MSG_NOSIGNAL);
			std::cout << "[-] Error: Failed send the list of files." << std::endl;
		} else {
			std::cout << "[+] Success: List sent successfully." << std::endl;
		}
	} else if (command.find(command_get) != std::string::npos) {
		std::vector<std::string> tokens;
		split(command, tokens);
		std::string filename = tokens[1];

		boost::coroutines::asymmetric_coroutine<int64_t>::pull_type coroutine_send_file(
				[&](boost::coroutines::asymmetric_coroutine<int64_t>::push_type& yield) {
					sendFile(yield, pair.first, filename);
				}
		);

		bytes = coroutine_send_file().get();
		if (bytes < 0) {
			sendMessage(pair.first, command_error, MSG_CONFIRM | MSG_NOSIGNAL);
			std::cout << "[-] Error: Failed send the file: \"" << filename << "\"." << std::endl;
		} else {
			std::cout << "[+] Success: File sent successfully: \"" << filename << "\"." << std::endl;
		}
	} else if (command == command_exit) {
		removeClients(pair);
	}
	yield();
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

	bytes = send(socket, buffer, size, flags);
	return bytes;
}

int64_t Connection::receiveBytes(int32_t socket, std::byte* buffer, int64_t size, int32_t flags) {
	int64_t bytes;

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

bool Connection::synchronization(std::pair<int32_t, int32_t>& pair) {
	int64_t message_size;
	int64_t bytes;
	std::string message;
	const std::string& command_error = commands[3];

	bytes = receiveMessage(pair.first, message, MSG_NOSIGNAL);
	if (bytes < 0 || message == command_error) {
		return false;
	}

	message_size = processResponse(message);
	if (message_size < 0) {
		return false;
	}

	while (message_size > 0) {
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
				storeFiles(pair, filename, file_size, file_hash);
			} catch (const std::invalid_argument& err) {
				std::cout << "[-] Error: Invalid argument: " << err.what() << '.' << std::endl;
				return false;
			} catch (const std::out_of_range& err) {
				std::cout << "[-] Error: Out of range: " << err.what() << '.' << std::endl;
				return false;
			}
		}

		message_size -= bytes;

		if (message_size <= 0) {
			break;
		}

		bytes = receiveMessage(pair.first, message, MSG_NOSIGNAL);
		if (bytes < 0 || message == command_error) {
			return false;
		}
	}
	sockets.push_back(pair);
	return true;
}

int64_t Connection::sendList(int32_t socket) {
	int64_t bytes;
	std::string message_size;
	std::string list;
	std::vector<std::string> files = getListFiles();

	list = std::accumulate(files.begin(), files.end(), std::string(),
	                       [](const std::string& a, const std::string& b) {
		                       return a + b + ' ';
	                       });

	try {
		message_size = std::to_string(list.size()) + ':';
	} catch (const std::exception& err) {
		return -1;
	}

	bytes = sendMessage(socket, message_size + list, MSG_CONFIRM | MSG_NOSIGNAL);
	if (bytes < 0) {
		return -1;
	}
	return bytes + static_cast<int64_t>(message_size.size());
}

int64_t Connection::sendFile(boost::coroutines::asymmetric_coroutine<int64_t>::push_type& yield,
                             int32_t socket, const std::string& filename) {
	int64_t bytes;
	int64_t total_bytes = 0;
	int64_t message_size = getSize(filename);
	std::string real_filename;
	std::string message;
	std::vector<std::pair<int32_t, int32_t>> sockets_filename = findSocket(filename);
	const std::string& command_error = commands[3];

	if (isFilenameChanged(filename)) {
		real_filename = removeIndex(filename);
	} else {
		real_filename = filename;
	}

	try {
		message = std::to_string(message_size) + ':';
	} catch (const std::exception& err) {
		return -1;
	}

	bytes = sendMessage(socket, message, MSG_CONFIRM | MSG_NOSIGNAL);
	if (bytes < 0) {
		return -1;
	}

	std::remove_if(
			sockets_filename.begin(),
			sockets_filename.end(),
			[&](const std::pair<int32_t, int32_t>& pair) {
				return pair.first == socket || pair.second == socket;
			}
	);

	if (sockets_filename.empty()) {
		return -1;
	}

	for (int64_t i = 0, offset = 0; total_bytes < message_size; ++i, offset += bytes) {
		std::byte buffer[BUFFER_SIZE];
		int64_t chunk_size = std::min<int64_t>(message_size - offset, static_cast<int64_t>(BUFFER_SIZE));
		int32_t client_socket_communicate = sockets_filename[i % sockets_filename.size()].second;
		struct timeval timeout { };
		timeout.tv_sec = 5;
		message = commands[1] + ':' + std::to_string(offset) + ':' + std::to_string(chunk_size) + ':' + real_filename;

		std::remove_if(
				sockets_filename.begin(),
				sockets_filename.end(),
				[&](const std::pair<int32_t, int32_t>& pair) {
					return !isConnect(pair);
				}
		);

		if (sockets_filename.empty()) {
			return -1;
		}

		bytes = sendMessage(client_socket_communicate, message, MSG_CONFIRM | MSG_NOSIGNAL);
		if (bytes < 0) {
			continue;
		}

		bytes = setsockopt(client_socket_communicate, SOL_SOCKET, SO_RCVTIMEO,
		                   reinterpret_cast<char*>(&timeout), sizeof(timeout));
		if (bytes < 0) {
			continue;
		}

		bytes = receiveBytes(client_socket_communicate, buffer, chunk_size, MSG_NOSIGNAL);
		if (bytes < 0) {
			continue;
		}

		if (std::string(reinterpret_cast<const char*>(buffer), bytes) == command_error) {
			return -2;
		}

		bytes = sendBytes(socket, buffer, bytes, MSG_CONFIRM | MSG_NOSIGNAL);
		if (bytes < 0) {
			continue;
		}
		total_bytes += bytes;
		yield(bytes);
	}
	return total_bytes;
}

std::vector<std::pair<int32_t, int32_t>> Connection::findSocket(const std::string& filename) {
	std::vector<std::pair<int32_t, int32_t>> vector;

	for (const auto& entry : storage) {
		std::string str;

		if (entry.second.filename == filename) {
			vector.emplace_back(entry.first);
		}
	}
	return vector;
}

std::vector<std::string> Connection::getListFiles() {
	std::unordered_map<std::string, int32_t> filename_counts;
	std::vector<std::string> unique_filenames;
	std::vector<std::string> duplicate_filenames;

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
	for (const auto& it : storage) {
		if (it.second.filename == filename) {
			return it.second.size;
		}
	}
	return -1;
}

void Connection::updateStorage() {
	std::unordered_map<std::string, int64_t> file_count;

	for (auto& entry : storage) {
		++file_count[entry.second.hash];
	}

	for (auto first = storage.begin(); first != storage.end(); ++first) {
		for (auto second = std::next(first); second != storage.end(); ++second) {
			if (first->second.hash != second->second.hash &&
					first->second.filename == second->second.filename) {
				int64_t file_occurrences = file_count[first->second.hash];

				if (file_occurrences > 1) {
					first->second.filename += '(' + std::to_string(file_occurrences - 1) + ')';
					second->second.filename += '(' + std::to_string(file_occurrences) + ')';
					first->second.is_filename_changed = true;
					second->second.is_filename_changed = true;
					--file_count[first->second.hash];
					--file_count[second->second.hash];
				}
			}
		}
	}
}

void Connection::storeFiles(std::pair<int32_t, int32_t> pair, const std::string& filename, int64_t size,
                            const std::string& hash) {
	FileInfo data {size, hash, filename, false};

	storage.insert(std::pair(pair, data));
	std::cout << "[+] Success: Stored the file: " << filename << '.' << std::endl;
}

void Connection::removeClients(std::pair<int32_t, int32_t> pair) {
	auto range = storage.equal_range(pair);

	for (auto it = range.first; it != range.second; ++it) {
		std::string filename = removeIndex(it->second.filename);

		for (auto& entry : storage) {
			if (entry.second.filename.find(filename) == 0) {
				entry.second.filename = removeIndex(entry.second.filename);
			}
		}
	}
	storage.erase(range.first, range.second);
	close(pair.first);
	close(pair.second);
	updateStorage();
}

void Connection::split(const std::string& str, std::vector<std::string>& tokens) {
	std::string token;
	std::istringstream tokenStream(str);

	while (std::getline(tokenStream, token, ':')) {
		tokens.push_back(token);
	}
}

std::string Connection::removeIndex(std::string filename) {
	uint64_t pos = filename.rfind('(');

	if (pos != std::string::npos) {
		filename.erase(pos);
	}
	return filename;
}

bool Connection::isFilenameChanged(const std::string& filename) {
	for (auto& entry : storage) {
		if (entry.second.filename == filename) {
			return entry.second.is_filename_changed;
		}
	}
	return false;
}
