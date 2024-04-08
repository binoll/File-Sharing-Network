#include "connection.hpp"

Connection::Connection(std::string dir) : dir(std::move(dir)) {
	addr_communicate.sin_family = AF_INET;
	addr_listen.sin_family = AF_INET;
}

Connection::~Connection() {
	if (thread.joinable()) {
		thread.join();
	}
	close(sockfd_listen);
	close(sockfd_communicate);
}

bool Connection::connectToServer(const std::string& server_ip, int32_t port_listen, int32_t port_communicate) {
	sockfd_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockfd_communicate = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd_communicate < 0 || sockfd_listen < 0) {
		std::cerr << "[-] Error: Failed to create socket." << std::endl;
		return false;
	}

	addr_listen.sin_port = htons(port_listen);
	addr_communicate.sin_port = htons(port_communicate);

	if (inet_pton(AF_INET, server_ip.c_str(), &addr_communicate.sin_addr) < 0) {
		std::cerr << "[-] Error: Invalid server address." << std::endl;
		return false;
	}
	if (connect(sockfd_communicate, reinterpret_cast<struct sockaddr*>(&addr_communicate), sizeof(addr_communicate)) <
			0) {
		std::cerr << "[-] Error: Failed connect to the server." << std::endl;
		return false;
	}

	if (inet_pton(AF_INET, server_ip.c_str(), &addr_listen.sin_addr) < 0) {
		std::cerr << "[-] Error: Invalid server address." << std::endl;
		return false;
	}
	if (connect(sockfd_listen, reinterpret_cast<struct sockaddr*>(&addr_listen),
	            sizeof(addr_listen)) < 0) {
		std::cerr << "[-] Error: Failed connect to the server." << std::endl;
		return false;
	}
	if (sendList(sockfd_communicate) < 0) {
		std::cerr << "[-] Error: Failed send the list of files." << std::endl;
		return false;
	}
	thread = std::thread(&Connection::handleServer, this);
	thread.detach();
	return true;
}

int64_t Connection::getFile(const std::string& filename) const {
	const std::string& command_error = commands_client[3];
	const std::string& command_get = commands_client[1] + ":" + filename;
	uint64_t pos_start;
	uint64_t pos_end;
	int64_t bytes;

	if (sendData(sockfd_listen, command_get, MSG_CONFIRM) < 0) {
		std::cerr << "[-] Error: Failed send the request." << std::endl;
		return -1;
	}

	std::string response = receiveData(sockfd_listen, MSG_WAITFORONE);
	if (response == command_error) {
		std::cerr << "[-] Error: The file does not exist." << std::endl;
		return -1;
	}
	pos_start = response.find(start_marker);
	if (pos_start == std::string::npos) {
		std::cerr << "[-] Error: Invalid response received from the server." << std::endl;
		return -1;
	}
	response.erase(pos_start, start_marker.length());

	std::ofstream file(filename, std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "[-] Error: Failed open the file for writing: " << filename << '.' << std::endl;
		sendData(sockfd_listen, command_error, MSG_CONFIRM);
		return -1;
	}
	do {
		pos_end = response.find(end_marker);
		if (pos_end == std::string::npos) {
			file.write(response.c_str(), static_cast<std::streamsize>(response.size()));
			continue;
		}
		response.erase(pos_end, end_marker.length());
		file.write(response.c_str(), static_cast<std::streamsize>(response.size()));
		break;
	} while (!(response = receiveData(sockfd_listen, MSG_WAITFORONE)).empty());
	bytes = file.tellp();
	file.close();
	return bytes;
}

std::list<std::string> Connection::getList() const {
	const std::string& command_error = commands_client[3];
	const std::string& command_list = commands_client[0];
	std::list<std::string> list;
	std::string response;
	uint64_t pos_start;

	if (sendData(sockfd_listen, command_list, MSG_CONFIRM) < 0) {
		std::cerr << "[-] Error: Failed send the request." << std::endl;
		return list;
	}
	response = receiveData(sockfd_listen, MSG_WAITFORONE);
	if (response == command_error) {
		std::cerr << "[-] Error: The list of files could not be retrieved." << std::endl;
		return list;
	}
	pos_start = response.find(start_marker);
	if (pos_start == std::string::npos) {
		std::cerr << "[-] Error: Invalid response received from server." << std::endl;
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
	} while (!(response = receiveData(sockfd_listen, MSG_WAITFORONE)).empty());
	return list;
}

bool Connection::exit() const {
	const std::string& command_exit = commands_client[2];
	return sendData(sockfd_communicate, command_exit, MSG_CONFIRM) > 0 &&
			sendData(sockfd_listen, command_exit, MSG_CONFIRM) > 0;
}

bool Connection::isConnection() const {
	return checkConnection(sockfd_listen) && checkConnection(sockfd_communicate);
}

int32_t Connection::getPort() {
	sockaddr_in addr { };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = 0;

	int32_t fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		std::cerr << "[-] Error: Failed to create the socket." << std::endl;
		return -1;
	}
	if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
		std::cerr << "[-] Error: Failed bind socket." << std::endl;
		close(fd);
		return -1;
	}
	socklen_t addr_len = sizeof(addr);
	if (getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &addr_len) < 0) {
		std::cerr << "[-] Error: Failed get socket name." << std::endl;
		close(fd);
		return -1;
	}
	int32_t port = ntohs(addr.sin_port);
	close(fd);
	return port;
}

int64_t Connection::sendData(int32_t fd, const std::string& command, int32_t flags) {
	int64_t bytes;
	bytes = send(fd, command.c_str(), command.size(), flags);
	return bytes;
}

std::string Connection::receiveData(int32_t fd, int32_t flags) {
	std::byte buffer[BUFFER_SIZE];
	std::string received_data;
	int64_t bytes;

	bytes = recv(fd, buffer, BUFFER_SIZE, flags);
	if (bytes > 0) {
		received_data.append(reinterpret_cast<char*>(buffer), bytes);
	}
	return received_data;
}

std::list<std::string> Connection::getListFiles() {
	std::list<std::string> list;
	std::lock_guard<std::mutex> lock(mutex);

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

uint64_t Connection::getFileSize(const std::string& filename) {
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		return -1;
	}
	return file.tellg();
}

bool Connection::checkConnection(int32_t fd) {
	int32_t optval;
	socklen_t optlen = sizeof(optval);

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1 || optval != 0) {
		return false;
	}
	return true;
}

int64_t Connection::sendList(int32_t fd) {
	int64_t bytes = 0;
	int64_t send_bytes;
	std::string data;
	std::list<std::string> list = getListFiles();

	send_bytes = sendData(fd, start_marker, MSG_CONFIRM);
	if (send_bytes < 0) {
		return -1;
	}

	for (const auto& filename : list) {
		std::ostringstream oss;
		std::string hash = calculateFileHash(filename);
		uint64_t size = getFileSize(filename);

		oss << filename << ':' << size << ':' << hash << ' ';
		data += oss.str();
		if (data.size() + filename.size() > BUFFER_SIZE) {
			send_bytes = sendData(fd, data, MSG_CONFIRM);
			if (send_bytes < 0) {
				return -1;
			}
			bytes += send_bytes;
			data.clear();
		}
	}

	send_bytes = sendData(fd, data, MSG_CONFIRM);
	if (send_bytes < 0) {
		return -1;
	}
	bytes += send_bytes;

	send_bytes = sendData(fd, end_marker, MSG_CONFIRM);
	if (send_bytes < 0) {
		return -1;
	}
	return bytes;
}

int64_t Connection::sendFile(int32_t fd, const std::string& filename, uint64_t offset, uint64_t size) {
	std::byte buffer[BUFFER_SIZE];
	int64_t bytes = 0;
	int64_t send_bytes;
	std::ifstream file(filename, std::ios::binary);

	if (!file.is_open()) {
		return -2;
	}
	file.seekg(static_cast<off_t>(offset), std::ios::beg);

	send_bytes = sendData(fd, start_marker, MSG_CONFIRM);
	if (send_bytes < 0) {
		return -1;
	}

	while (bytes < size && !file.eof()) {
		uint64_t read_bytes = (size - bytes) > BUFFER_SIZE ? BUFFER_SIZE : size - bytes;

		if (!file.is_open()) {
			return -2;
		}
		file.read(reinterpret_cast<char*>(buffer), static_cast<off_t>(read_bytes));

		send_bytes = sendData(fd, reinterpret_cast<char*>(buffer), MSG_CONFIRM);
		if (send_bytes < 0) {
			return -1;
		}
		bytes += send_bytes;
	}

	send_bytes = sendData(fd, end_marker, MSG_CONFIRM);
	if (send_bytes < 0) {
		return -1;
	}
	return bytes;
}

std::string Connection::calculateFileHash(const std::string& filename) {
	std::ifstream file(filename, std::ios::binary);
	std::hash<std::string> hash_fn;
	std::ostringstream oss;
	std::lock_guard<std::mutex> lock(mutex);

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
	int64_t bytes;
	const std::string& command_list = commands_server[0];
	const std::string& command_get = commands_server[1] + ':';
	const std::string& command_part = commands_server[2] + ':';
	const std::string& command_exit = commands_server[3];

	while (isConnection()) {
		std::string command = receiveData(sockfd_listen, MSG_DONTWAIT);

		if (command == command_list) {
			bytes = sendList(sockfd_listen);
			if (bytes < 0) {
				std::cerr << "[-] Error: Failed send the list of files." << std::endl;
			}
			continue;
		} else if (command.substr(0, 4) == command_get) {
			std::string filename = command.substr(4);
			std::ifstream file(filename, std::ios::binary);

			if (!file.is_open()) {
				std::cerr << "[-] Error: Failed to open the file for reading: " << filename << '.' << std::endl;
				continue;
			}
			file.seekg(0, std::ios::end);
			std::streamsize size = file.tellg();
			file.seekg(0, std::ios::beg);

			std::byte buffer[size];
			file.read(reinterpret_cast<char*>(buffer), size);

			bytes = sendFile(sockfd_listen, filename, std::ios::beg, size);
			if (bytes == -1) {
				std::cerr << "[-] Error: Failed to send the file: " << filename << '.' << std::endl;
			} else if (bytes == -2) {
				std::cerr << "[-] Error: Failed to read from the file: " << filename << '.' << std::endl;
			}
			continue;
		} else if (command.substr(0, 5) == command_part) {
			std::istringstream iss(command.substr(5));
			uint64_t colon1 = command.find(':', 5);
			uint64_t colon2 = command.find(':', colon1 + 1);
			uint64_t colon3 = command.find(':', colon2 + 1);

			if (colon1 != std::string::npos && colon2 != std::string::npos && colon3 != std::string::npos) {
				off_t offset = static_cast<off_t>(std::stoull(command.substr(5, colon1 - 5)));
				std::streamsize size = static_cast<std::streamsize>(
						std::stoull(command.substr(colon1 + 1, colon2 - colon1 - 1))
				);
				std::string filename = command.substr(colon3 + 1);
				std::ifstream file(filename, std::ios::binary);
				std::byte buffer[size];

				if (!file.is_open()) {
					std::cerr << "[-] Error: Failed to open the file for reading: " << filename << '.' << std::endl;
					continue;
				}
				file.seekg(offset, std::ios::beg);
				file.read(reinterpret_cast<char*>(buffer), size);

				bytes = sendFile(sockfd_listen, filename, offset, size);
				if (bytes == -1) {
					std::cerr << "[-] Error: Failed send the file: " << filename << '.' << std::endl;
				} else if (bytes == -2) {
					std::cerr << "[-] Error: Failed open the file: " << filename << '.' << std::endl;
				}
				continue;
			}
		} else if (command == command_exit) {
			break;
		}
	}
}
