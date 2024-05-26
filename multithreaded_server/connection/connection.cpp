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

void Connection::waitConnection(int32_t port_listen, int32_t port_communicate) {
	addr_listen.sin_port = htons(port_listen);
	addr_communicate.sin_port = htons(port_communicate);

	socket_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	socket_communicate = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (socket_listen < 0) {
		BOOST_LOG_TRIVIAL(error) << "Failed to create socket for listen" << std::endl;
		return;
	}

	if (socket_communicate < 0) {
		BOOST_LOG_TRIVIAL(error) << "Failed to create socket for communicate" << std::endl;
		return;
	}

	if (bind(socket_listen, reinterpret_cast<struct sockaddr*>(&addr_listen), sizeof(addr_listen)) < 0) {
		BOOST_LOG_TRIVIAL(error) << "Failed to bind the socket for listening" << std::endl;
		return;
	}

	if (bind(socket_communicate, reinterpret_cast<struct sockaddr*>(&addr_communicate), sizeof(addr_communicate)) < 0) {
		BOOST_LOG_TRIVIAL(error) << "Failed to bind the socket for communicating" << std::endl;
		return;
	}

	if (listen(socket_communicate, BACKLOG) < 0) {
		BOOST_LOG_TRIVIAL(error) << "Failed to listen for socket_communicate" << std::endl;
		return;
	}

	if (listen(socket_listen, BACKLOG) < 0) {
		BOOST_LOG_TRIVIAL(error) << "Failed to listen for socket_listen" << std::endl;
		return;
	}

	BOOST_LOG_TRIVIAL(info) << "Server is listening on port for listening: " << htons(addr_listen.sin_port)
				<< std::endl;
	BOOST_LOG_TRIVIAL(info) << "Server is listening on port for communicate: " << htons(addr_communicate.sin_port)
				<< std::endl;

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

		BOOST_LOG_TRIVIAL(info) << "Client connected: " << inet_ntoa(client_addr_listen.sin_addr) << ':'
					<< client_addr_listen.sin_port << ' ' << inet_ntoa(client_addr_communicate.sin_addr) << ':'
					<< client_addr_communicate.sin_port << std::endl;

		std::thread thread(&Connection::processingClients, this, client_socket_listen, client_socket_communicate);
		thread.detach();
	}
}

void Connection::closeConnection(int32_t client_socket_listen, int32_t client_socket_communicate) {
	std::pair pair = std::make_pair(client_socket_listen, client_socket_communicate);

	removeClients(pair);
	updateStorage(pair);

	close(client_socket_listen);
	close(client_socket_communicate);
}

void Connection::processingClients(int32_t client_socket_listen, int32_t client_socket_communicate) {
	int64_t bytes;
	std::pair pair = std::make_pair(client_socket_listen, client_socket_communicate);
	const std::string& command_list = commands[0];
	const std::string& command_get = commands[1];
	const std::string& command_error = commands[2];
	const std::string& command_exist = commands[3];
	const std::string& command_modify = commands[4];
	const std::string& command_add = commands[5];
	const std::string& command_delete = commands[6];

	if (!synchronization(client_socket_listen, client_socket_communicate)) {
		BOOST_LOG_TRIVIAL(error) << "The client can not connect. The storage could not be synchronized" << std::endl;
		closeConnection(client_socket_listen, client_socket_communicate);

		return;
	}

	while (true) {
		std::string command = "empty";

		receiveMessage(client_socket_listen, command, MSG_DONTWAIT | MSG_NOSIGNAL);

		if (command == command_list) {
			bytes = sendList(client_socket_listen);
			if (bytes < 0) {
				sendMessage(client_socket_listen, command_error, MSG_CONFIRM | MSG_NOSIGNAL);
				BOOST_LOG_TRIVIAL(error) << "Failed send the list of files" << std::endl;
			}
		} else if (command.compare(0, command_get.length(), command_get) == 0) {
			std::vector<std::string> tokens;
			std::string filename;

			split(command, ':', tokens);
			filename = tokens[1];

			bytes = sendFile(client_socket_listen, filename);
			if (bytes == -1) {
				sendMessage(client_socket_listen, command_error, MSG_CONFIRM | MSG_NOSIGNAL);
				BOOST_LOG_TRIVIAL(error) << "Failed send the file: \"" << filename << "\"" << std::endl;
			} else if (bytes == -2) {
				sendMessage(client_socket_listen, command_exist, MSG_CONFIRM | MSG_NOSIGNAL);
			} else {
				BOOST_LOG_TRIVIAL(info) << "File sent successfully: \"" << filename << "\"" << std::endl;
			}
		} else if (command.compare(0, command_add.length(), command_add) == 0 ||
				command.compare(0, command_delete.length(), command_delete) == 0 ||
				command.compare(0, command_modify.length(), command_modify) == 0) {
			std::vector<std::string> tokens;
			std::string filename;
			std::string hash;
			int64_t size;

			split(command, ':', tokens);

			if (tokens.size() == 4) {
				filename = tokens[1];
				hash = tokens[2];

				try {
					size = static_cast<int64_t>(std::stoull(tokens[3]));
				} catch (const std::exception& err) {
					BOOST_LOG_TRIVIAL(error) << err.what() << std::endl;
					continue;
				}
			} else if (tokens.size() == 2) {
				filename = tokens[1];
			} else {
				continue;
			}

			if (command.compare(0, command_add.length(), command_add) == 0) {
				addFileToStorage(pair, filename, size, hash);
			} else if (command.compare(0, command_modify.length(), command_modify) == 0) {
				modifyFileInStorage(pair, filename, size, hash);
			} else if (command.compare(0, command_delete.length(), command_delete) == 0) {
				deleteFileFromStorage(pair, filename);
			}

			updateStorage(pair);
		} else if (command.empty()) {
			break;
		}
	}

	BOOST_LOG_TRIVIAL(info) << "Client disconnected" << std::endl;
	closeConnection(client_socket_listen, client_socket_communicate);
}

int64_t Connection::sendMessage(int32_t socket, const std::string& message, int32_t flags) {
	int64_t bytes;

	bytes = sendBytes(
			socket,
			message.data(),
			static_cast<int64_t>(message.size()),
			flags
	);

	return bytes;
}

int64_t Connection::receiveMessage(int32_t socket, std::string& message, int32_t flags) {
	int64_t bytes;
	char buffer[BUFFER_SIZE];

	bytes = receiveBytes(socket, buffer, BUFFER_SIZE, flags);
	if (bytes > 0) {
		message.assign(buffer, bytes);
	} else if (bytes == 0) {
		message.clear();
	}

	return bytes;
}

int64_t Connection::sendBytes(int32_t socket, const char* buffer, int64_t size, int32_t flags) {
	return send(socket, buffer, size, flags);
}

int64_t Connection::receiveBytes(int32_t socket, char* buffer, int64_t size, int32_t flags) {
	return recv(socket, buffer, size, flags);
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
	int64_t bytes;
	int64_t message_size;
	std::string message;
	const std::string& command_error = commands[2];
	std::pair pair = std::make_pair(client_socket_listen, client_socket_communicate);

	bytes = receiveMessage(client_socket_communicate, message, MSG_NOSIGNAL);
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
				addFileToStorage(pair, filename, file_size, file_hash);
				BOOST_LOG_TRIVIAL(info) << "Stored the file \"" << filename << '\"' << std::endl;
			} catch (const std::invalid_argument& err) {
				BOOST_LOG_TRIVIAL(error) << "Invalid argument: " << err.what() << std::endl;
				return false;
			} catch (const std::out_of_range& err) {
				BOOST_LOG_TRIVIAL(error) << "Out of range: " << err.what() << std::endl;
				return false;
			}
		}

		message_size -= bytes;

		if (message_size <= 0) {
			break;
		}

		bytes = receiveMessage(client_socket_communicate, message, MSG_NOSIGNAL);
		if (bytes < 0 || message == command_error) {
			return false;
		}
	}

	if (updateStorage(std::make_pair(client_socket_listen, client_socket_communicate)) < 0) {
		return false;
	}

	return true;
}

int64_t Connection::sendList(int32_t socket) {
	int64_t bytes;
	std::string message_size;
	std::string list;
	std::vector<std::string> files = getListFiles();

	list = std::accumulate(
			files.begin(), files.end(), std::string(),
			[](const std::string& a, const std::string& b) {
				return a + b + ' ';
			}
	);

	try {
		message_size = std::to_string(list.size()) + ':';
	} catch (const std::exception&) {
		return -1;
	}

	bytes = sendMessage(socket, message_size + list, MSG_CONFIRM | MSG_NOSIGNAL);
	if (bytes < 0) {
		return -1;
	}

	return bytes + static_cast<int64_t>(message_size.size());
}

int64_t Connection::sendFile(int32_t socket, std::string& filename) {
	int64_t bytes;
	int64_t total_bytes = 0;
	int64_t message_size = getSize(filename);
	std::string message;
	const std::string& command_error = commands[2];
	std::vector<std::pair<int32_t, int32_t>> sockets = findSocket(filename);

	if (isFileExistOnClient(socket, filename)) {
		return -2;
	}

	if (sockets.empty()) {
		return -1;
	}

	try {
		message = std::to_string(message_size) + ':';
	} catch (const std::exception&) {
		return -1;
	}

	bytes = sendMessage(socket, message, MSG_CONFIRM | MSG_NOSIGNAL);
	if (bytes < 0) {
		return -1;
	}

	for (int64_t i = 0, offset = 0; total_bytes < message_size; ++i, offset += bytes) {
		char buffer[BUFFER_SIZE];
		struct timeval timeout { };
		int64_t chunk_size = std::min<int64_t>(message_size - offset, static_cast<int64_t>(BUFFER_SIZE));
		int32_t client_socket_listen = sockets[i % sockets.size()].first;
		int32_t client_socket_communicate = sockets[i % sockets.size()].second;

		timeout.tv_sec = 5;
		message = commands[1] + ':' + std::to_string(offset) + ':' + std::to_string(chunk_size) + ':' + filename;

		if (sockets.empty()) {
			return -1;
		}

		bytes = sendMessage(client_socket_communicate, message, MSG_CONFIRM | MSG_NOSIGNAL);
		if (bytes < 0) {
			closeConnection(client_socket_listen, client_socket_communicate);
			continue;
		}

		bytes = setsockopt(client_socket_communicate, SOL_SOCKET, SO_RCVTIMEO,
		                   reinterpret_cast<char*>(&timeout), sizeof(timeout));
		if (bytes < 0) {
			closeConnection(client_socket_listen, client_socket_communicate);
			continue;
		}

		bytes = receiveBytes(client_socket_communicate, buffer, chunk_size, MSG_NOSIGNAL);
		if (bytes < 0) {
			closeConnection(client_socket_listen, client_socket_communicate);
			continue;
		}

		if (std::string(reinterpret_cast<const char*>(buffer), bytes) == command_error) {
			return -2;
		}

		bytes = sendBytes(socket, buffer, bytes, MSG_CONFIRM | MSG_NOSIGNAL);
		if (bytes < 0) {
			closeConnection(client_socket_listen, client_socket_communicate);
			continue;
		}

		total_bytes += bytes;
	}

	return total_bytes;
}

std::vector<std::pair<int32_t, int32_t>> Connection::findSocket(const std::string& filename) {
	std::vector<std::pair<int32_t, int32_t>> sockets;
	std::lock_guard<std::mutex> lock(mutex_storage);

	for (const auto& entry : storage) {
		std::string str;

		if (entry.second.filename == filename) {
			sockets.emplace_back(entry.first);
		}
	}

	return sockets;
}

std::vector<std::string> Connection::getListFiles() {
	std::set<std::string> unique_files;
	std::vector<std::string> list;
	std::lock_guard<std::mutex> lock(mutex_storage);

	for (auto& item : storage) {
		unique_files.insert(item.second.filename);
	}

	list = std::move(std::vector<std::string>(unique_files.begin(), unique_files.end()));

	return list;
}

int64_t Connection::getSize(const std::string& filename) {
	std::lock_guard<std::mutex> lock(mutex_storage);

	for (const auto& it : storage) {
		if (it.second.filename == filename) {
			return it.second.size;
		}
	}

	return -1;
}


void Connection::addFileToStorage(std::pair<int32_t, int32_t> pair, std::string& filename,
                                  int64_t size, const std::string& hash) {
	FileInfo data {size, hash, filename, false};
	std::lock_guard<std::mutex> lock(mutex_storage);

	storage.insert(std::make_pair(pair, data));
}

void Connection::modifyFileInStorage(std::pair<int32_t, int32_t> pair, std::string& filename,
                                     int64_t size, const std::string& hash) {
	std::lock_guard<std::mutex> lock(mutex_storage);

	for (auto& item : storage) {
		if (item.first.first == pair.first && item.first.second == pair.second) {
			item.second.filename = filename;
			item.second.hash = hash;
			item.second.size = size;
		}
	}
}

void Connection::deleteFileFromStorage(std::pair<int32_t, int32_t> pair, std::string& filename) {
	std::lock_guard<std::mutex> lock(mutex_storage);

	for (auto it = storage.begin(); it != storage.end();) {
		if (it->first.first == pair.first && it->first.second == pair.second &&
				it->second.filename == filename) {
			it = storage.erase(it);
		} else {
			++it;
		}
	}
}

int64_t Connection::updateStorage(std::pair<int32_t, int32_t> pair) {
	std::unordered_map<std::string, int64_t> hash_count;
	std::lock_guard<std::mutex> lock(mutex_storage);

	for (auto& entry : storage) {
		++hash_count[entry.second.hash];
	}

	for (auto first = storage.begin(); first != storage.end(); ++first) {
		for (auto second = std::next(first); second != storage.end(); ++second) {
			int64_t file_occurrences = hash_count[first->second.hash];

			if (first->second.hash != second->second.hash &&
					first->second.filename == second->second.filename) {
				if (file_occurrences > 1) {
					std::string command_change = commands[4] + ':';
					std::string new_first_filename;
					std::string new_second_filename;

					if (first->second.is_filename_modify) {
						first->second.filename = removeIndex(first->second.filename);
					}

					if (second->second.is_filename_modify) {
						second->second.filename = removeIndex(second->second.filename);
					}

					new_first_filename = first->second.filename + ':' + first->second.filename + '(' +
							std::to_string(file_occurrences - 1) + ')';

					if (sendMessage(pair.second, command_change + new_first_filename,
					                MSG_CONFIRM | MSG_NOSIGNAL) < 0) {
						return -1;
					}

					first->second.filename += '(' + std::to_string(file_occurrences - 1) + ')';

					new_second_filename = second->second.filename + ':' + second->second.filename + '(' +
							std::to_string(file_occurrences) + ')';

					if (sendMessage(pair.second, command_change + new_second_filename,
					                MSG_CONFIRM | MSG_NOSIGNAL) < 0) {
						return -1;
					}

					second->second.filename += '(' + std::to_string(file_occurrences) + ')';

					first->second.is_filename_modify = true;
					second->second.is_filename_modify = true;
					--hash_count[first->second.hash];
					--hash_count[second->second.hash];
				} else if (file_occurrences == 1 && first->second.is_filename_modify) {
					std::string command_change = commands[4] + ':';
					std::string new_filename;

					new_filename = first->second.filename + ':' + removeIndex(first->second.filename);

					if (sendMessage(pair.second, command_change + new_filename,
					                MSG_CONFIRM | MSG_NOSIGNAL) < 0) {
						return -1;
					}

					first->second.filename = removeIndex(first->second.filename);
					first->second.is_filename_modify = false;
				}
			} else if (first->second.hash == second->second.hash &&
					first->second.filename != second->second.filename) {
				std::string command_change = commands[4] + ':';
				std::string new_filename;

				new_filename = second->second.filename + ':' + first->second.filename;

				if (sendMessage(pair.second, command_change + new_filename,
				                MSG_CONFIRM | MSG_NOSIGNAL) < 0) {
					return -1;
				}

				second->second.filename = first->second.filename;
			}
		}
	}

	return 0;
}

void Connection::removeClients(std::pair<int32_t, int32_t> pair) {
	std::lock_guard<std::mutex> lock(mutex_storage);
	auto range = storage.equal_range(pair);

	storage.erase(range.first, range.second);
}

std::string Connection::removeIndex(std::string filename) {
	uint64_t pos = filename.rfind('(');

	if (pos != std::string::npos) {
		filename.erase(pos);
	}

	return filename;
}

void Connection::split(const std::string& str, char delim, std::vector<std::string>& tokens) {
	std::string token;
	std::istringstream tokenStream(str);

	while (std::getline(tokenStream, token, delim)) {
		tokens.push_back(token);
	}
}

bool Connection::isFileExistOnClient(int32_t socket, const std::string& filename) {
	std::lock_guard<std::mutex> lock(mutex_storage);

	return std::ranges::any_of(storage, [&](const auto& item) {
		return item.second.filename == filename &&
				(item.first.first == socket || item.first.second == socket);
	});
}
