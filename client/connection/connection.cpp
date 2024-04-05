#include "connection.hpp"

Connection::Connection(std::string dir) : dir(std::move(dir)) {
	active_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	passive_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (passive_fd < 0 || active_fd < 0) {
		std::cerr << "[-] Error: Failed to create socket." << std::endl;
	}
}

Connection::~Connection() {
	if (thread.joinable()) {
		thread.join();
	}
	close(active_fd);
	close(passive_fd);
}

bool Connection::connectToServer(const std::string& server_ip, int32_t port) {
	active_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (active_fd < 0) {
		std::cerr << "[-] Error: Failed create send socket." << std::endl;
		return false;
	}

	send_port = port;
	send_addr.sin_family = AF_INET;
	send_addr.sin_port = htons(send_port);

	if (inet_pton(AF_INET, server_ip.c_str(), &send_addr.sin_addr) < 0) {
		std::cerr << "[-] Error: Invalid server address." << std::endl;
		return false;
	}
	if (connect(active_fd, reinterpret_cast<struct sockaddr*>(&send_addr), sizeof(send_addr)) < 0) {
		std::cerr << "[-] Error: Failed connect to the server." << std::endl;
		return false;
	}

	passive_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (passive_fd < 0) {
		std::cerr << "[-] Error: Failed create receive socket." << std::endl;
		return false;
	}

	receive_port = findFreePort();
	receive_addr.sin_family = AF_INET;
	receive_addr.sin_port = htons(receive_port);
	receive_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(passive_fd, reinterpret_cast<struct sockaddr*>(&receive_addr), sizeof(receive_addr)) < 0) {
		std::cerr << "[-] Error: Failed bind receive socket." << std::endl;
		return false;
	}
	if (connect(passive_fd, reinterpret_cast<struct sockaddr*>(&receive_addr), sizeof(receive_addr)) < 0) {
		std::cerr << "[-] Error: Failed connect to the server." << std::endl;
		return false;
	}
	sendListToServer(active_fd);
	thread = std::thread(&Connection::handleServer, this);
	thread.detach();
	return true;
}

int64_t Connection::getFile(const std::string& filename) {

}

std::list<std::string> Connection::getList() const {
	const std::string& command_list = commands_client[0];
	const std::string& start_marker = marker[0];
	const std::string& end_marker = marker[1];
	std::list<std::string> list;
	std::string response;
	uint64_t pos_start;

	if (sendToServer(active_fd, command_list) < 0) {
		return list;
	}
	response = receive(active_fd);
	pos_start = response.find(start_marker);
	if (pos_start == std::string::npos) {
		return list;
	}
	response.erase(pos_start, start_marker.length());

	do {
		std::istringstream iss(response);
		std::string filename;

		while (std::getline(iss, filename, ' ') && filename != end_marker) {
			list.emplace_back(filename);
		}
		if (filename == end_marker) {
			break;
		}
	} while (!(response = receive(active_fd)).empty());
	return list;
}

bool Connection::exit() const {
	const std::string& command_exit = commands_client[2];
	return sendToServer(active_fd, command_exit) > 0 && sendToServer(active_fd, command_exit) > 0;
}

bool Connection::isConnect() const {
	return checkConnection(passive_fd) && checkConnection(active_fd);
}

int32_t Connection::findFreePort() {
	sockaddr_in addr { };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = 0;

	int32_t fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		std::cerr << "Failed to create socket" << std::endl;
		return -1;
	}
	if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
		std::cerr << "[-] Error: Failed to bind socket" << std::endl;
		close(fd);
		return -1;
	}
	socklen_t addr_len = sizeof(addr);
	if (getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &addr_len) < 0) {
		std::cerr << "[-] Error: Failed to get socket name" << std::endl;
		close(fd);
		return -1;
	}
	int32_t port = ntohs(addr.sin_port);
	close(fd);
	return port;
}

int64_t Connection::sendToServer(int32_t fd, const std::string& command) {
	int64_t bytes;

	bytes = send(fd, command.c_str(), command.size(), MSG_CONFIRM);
	if (bytes < 0) {
		std::cerr << "[-] Error: Failed send to the server." << std::endl;
	}
	return bytes;
}

std::string Connection::receive(int32_t fd) {
	std::byte buffer[BUFFER_SIZE];
	std::string received_data;
	int64_t bytes;

	do {
		bytes = recv(fd, buffer, BUFFER_SIZE, MSG_WAITFORONE);
		if (bytes > 0) {
			received_data.append(reinterpret_cast<char*>(buffer), bytes);
		} else if (bytes < 0) {
			std::cerr << "[-] Error: Failed to receive data from server." << std::endl;
			return "";
		}
	} while (bytes == BUFFER_SIZE);
	return received_data;
}

std::list<std::string> Connection::listFiles() {
	std::lock_guard<std::mutex> lock(mutex);
	std::list<std::string> list;

	try {
		std::filesystem::directory_iterator iterator(dir);
		for (const auto& entry : iterator) {
			if (std::filesystem::is_regular_file(entry)) {
				list.push_back(entry.path().filename().string());
			}
		}
	} catch (const std::exception& err) {
		std::cerr << "[-] Error: " << err.what() << std::endl;
	}
	return list;
}

bool Connection::checkConnection(int32_t fd) {
	return getsockopt(fd, SOL_SOCKET, SO_ERROR, nullptr, nullptr);
}

int64_t Connection::sendListToServer(int32_t fd) {
	const std::string& start_marker = marker[0];
	const std::string& end_marker = marker[1];
	std::string file_info = start_marker;
	std::list<std::string> list = listFiles();
	int64_t send_bytes = 0;
	int64_t bytes = 0;

	for (const auto& filename : list) {
		if (file_info.size() + filename.size() > BUFFER_SIZE) {
			if ((send_bytes = sendToServer(fd, file_info)) < 0) {
				return -1;
			}
			bytes += send_bytes;
		}
		std::ostringstream oss;
		std::string hash = calculateFileHash(filename);
		oss << filename << ':' << hash << ' ';
		file_info += oss.str();
	}
	if ((send_bytes = sendToServer(fd, file_info)) < 0) {
		return -1;
	}
	bytes += send_bytes;
	if ((send_bytes = sendToServer(fd, end_marker)) < 0) {
		return -1;
	}
	bytes += send_bytes;
	return bytes;
}

int64_t Connection::sendFileToServer(int32_t fd, const std::string& filename, uint64_t size, uint64_t offset) {

}

std::string Connection::calculateFileHash(const std::string& filename) {
	std::lock_guard<std::mutex> lock(mutex);
	std::ifstream file(filename, std::ios::binary);
	std::hash<std::string> hash_fn;
	std::ostringstream oss;

	if (!file) {
		std::cerr << "[-] Error: Failed to open the file." << std::endl;
	}
	oss << file.rdbuf();
	std::string file_content = oss.str();
	uint64_t hash_result = hash_fn(file_content);
	std::stringstream ss;
	ss << std::hex << hash_result;
	return ss.str();
}

void Connection::handleServer() {
	std::string command;
	const std::string& command_list = commands_server[0];
	const std::string& command_get = commands_server[1] + ':';
	const std::string& command_part = commands_server[2] + ':';
	const std::string& command_exit = commands_server[3];

	while (isConnect()) {
		command = receive(passive_fd);

		if (command == command_list) {
			sendListToServer(passive_fd);
		} else if (command == command_get) {
			uint64_t colon1 = command.find(':', 4);
			uint64_t colon2 = command.find(':', colon1 + 1);
			uint64_t colon3 = command.find(':', colon2 + 1);

			if (colon1 != std::string::npos && colon2 != std::string::npos && colon3 != std::string::npos) {
				uint64_t size = std::stoull(command.substr(4, colon1 - 4));
				uint64_t offset = std::stoull(command.substr(colon1 + 1, colon2 - colon1 - 1));
				std::string filename = command.substr(colon3 + 1);

				sendFileToServer(passive_fd, filename, size, offset);
			} else {
				std::cerr << "[-] Error: Invalid command format." << std::endl;
			}
		} else if (command == command_part) {
			std::istringstream iss(command.substr(5));
			uint64_t colon1 = command.find(':', 5);
			uint64_t colon2 = command.find(':', colon1 + 1);
			uint64_t colon3 = command.find(':', colon2 + 1);

			if (colon1 != std::string::npos && colon2 != std::string::npos && colon3 != std::string::npos) {
				off_t offset = static_cast<off_t>(std::stoull(command.substr(5, colon1 - 5)));
				std::streamsize size = static_cast<std::streamsize>(std::stoull(
						command.substr(colon1 + 1, colon2 - colon1 - 1)));
				std::string filename = command.substr(colon3 + 1);
				std::ifstream file(filename, std::ios::binary);
				std::vector<std::byte> buffer(size);

				if (!file) {
					std::cerr << "[-] Error: Can not open the file for reading: " << filename << '.' << std::endl;
					continue;
				}
				file.seekg(offset);
				file.read(reinterpret_cast<char*>(buffer.data()), size);
				sendFileToServer(passive_fd, filename, offset, size);
			}
		} else if (command == command_exit) {
			break;
		}
	}
}
