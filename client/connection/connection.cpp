#include "connection.hpp"

Connection::Connection(std::string dir) : dir(std::move(dir)) {
	sockfd_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockfd_communicate = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd_listen < 0 || sockfd_communicate < 0) {
		std::cerr << "[-] Error: Failed to create socket." << std::endl;
	}
}

Connection::~Connection() {
	if (thread.joinable()) {
		thread.join();
	}
	close(sockfd_listen);
	close(sockfd_communicate);
}

bool Connection::connectToServer(const std::string& ip, int32_t port_listen, int32_t port_communicate) {
	addr_listen.sin_family = AF_INET;
	addr_communicate.sin_family = AF_INET;
	addr_listen.sin_port = htons(port_listen);
	addr_communicate.sin_port = htons(port_communicate);

	if (inet_pton(AF_INET, ip.c_str(), &addr_listen.sin_addr) < 0) {
		std::cerr << "[-] Error: Invalid server address." << std::endl;
		return false;
	}
	if (connect(sockfd_listen, reinterpret_cast<struct sockaddr*>(&addr_listen), sizeof(addr_listen)) < 0) {
		std::cerr << "[-] Error: Failed connect to the server." << std::endl;
		return false;
	}

	if (inet_pton(AF_INET, ip.c_str(), &addr_communicate.sin_addr) < 0) {
		std::cerr << "[-] Error: Invalid server address." << std::endl;
		return false;
	}
	if (connect(sockfd_communicate, reinterpret_cast<struct sockaddr*>(&addr_communicate),
	            sizeof(addr_communicate)) < 0) {
		std::cerr << "[-] Error: Failed connect to the server." << std::endl;
		return false;
	}

	if (sendList(sockfd_communicate) == -1) {
		std::cerr << "[-] Error: Failed send the list of files." << std::endl;
		return false;
	}
	thread = std::thread(&Connection::handleServer, this);
	thread.detach();
	return true;
}

int64_t Connection::getFile(const std::string& filename) {
	const std::string& command_error = commands_client[3];
	const std::string& command_get = commands_client[1] + ":" + filename;
	std::string response;
	uint64_t pos;
	int64_t send_bytes;
	int64_t bytes;

	send_bytes = sendData(sockfd_communicate, command_get, MSG_CONFIRM);
	if (send_bytes < 0) {
		return -1;
	}

	response = receiveData(sockfd_communicate, MSG_WAITFORONE);
	if (response == command_error) {
		return -2;
	}

	pos = response.find(start_marker);
	if (pos == std::string::npos) {
		return -1;
	}
	response.erase(pos, start_marker.length());

	std::ofstream file(filename, std::ios::binary);
	if (!file.is_open()) {
		sendData(sockfd_communicate, command_error, MSG_CONFIRM);
		return -3;
	}
	do {
		pos = response.find(end_marker);
		if (pos == std::string::npos) {
			file.write(response.c_str(), static_cast<std::streamsize>(response.size()));
			continue;
		}
		response.erase(pos, end_marker.length());
		file.write(response.c_str(), static_cast<std::streamsize>(response.size()));
		break;
	} while (!(response = receiveData(sockfd_communicate, MSG_WAITFORONE)).empty());
	bytes = file.tellp();
	file.close();
	return bytes;
}

std::list<std::string> Connection::getList() {
	const std::string& command_error = commands_client[3];
	const std::string& command_list = commands_client[0];
	std::list<std::string> list;
	std::string response;
	uint64_t pos;
	int64_t send_bytes;

	send_bytes = sendData(sockfd_communicate, command_list, MSG_CONFIRM);
	if (send_bytes < 0) {
		return list;
	}

	response = receiveData(sockfd_communicate, MSG_WAITFORONE);
	if (response == command_error) {
		return list;
	}

	pos = response.find(start_marker);
	if (pos == std::string::npos) {
		return list;
	}
	response.erase(pos, start_marker.length());

	do {
		std::istringstream iss(response);
		std::string filename;

		while (std::getline(iss, filename, ' ') && filename != end_marker) {
			list.emplace_back(filename);
		}
		if (filename == end_marker) {
			break;
		}
	} while (!(response = receiveData(sockfd_communicate, MSG_WAITFORONE)).empty());
	return list;
}

bool Connection::exit() {
	const std::string& command_exit = commands_client[2];
	return sendData(sockfd_communicate, command_exit, MSG_CONFIRM) > 0;
}

bool Connection::isConnection() {
	return checkConnection(sockfd_communicate) && checkConnection(sockfd_listen);
}

int64_t Connection::sendData(int32_t fd, const std::string& command, int32_t flags) {
	int64_t bytes;
	std::lock_guard<std::mutex> lock(mutex);

	bytes = send(fd, command.c_str(), command.size(), flags);
	return bytes;
}

std::string Connection::receiveData(int32_t fd, int32_t flags) {
	std::byte buffer[BUFFER_SIZE];
	int64_t bytes;
	std::string receive_data;
	std::lock_guard<std::mutex> lock(mutex);

	bytes = recv(fd, buffer, BUFFER_SIZE, flags);
	if (bytes > 0) {
		receive_data = std::string(reinterpret_cast<char*>(buffer), bytes);
	}
	return receive_data;
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
	std::list<std::string> list = getListFiles();
	std::string data;
	int64_t bytes = 0;
	int64_t send_bytes;

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

int64_t Connection::sendFile(int32_t fd, const std::string& filename, int64_t offset, int64_t size) {
	std::byte buffer[BUFFER_SIZE];
	std::ifstream file(filename, std::ios::binary);
	int64_t bytes = 0;
	int64_t send_bytes;

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
	std::ostringstream oss;
	std::hash<std::string> hash_fn;
	std::lock_guard<std::mutex> lock(mutex);

	if (!file) {
		std::cerr << "[-] Error: Failed to open the file." << std::endl;
		return "";
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
	std::string command;
	const std::string& command_list = commands_server[0];
	const std::string& command_get = commands_server[1] + ':';
	const std::string& command_part = commands_server[2] + ':';
	const std::string& command_exit = commands_server[3];
	const std::string& command_error = commands_server[4];


	while (isConnection()) {
		command = receiveData(sockfd_listen, MSG_DONTWAIT);

		if (command == command_list) {
			bytes = sendList(sockfd_listen);
			if (bytes < 0) {
				continue;
			}
		} else if (command.substr(0, 4) == command_get) {
			std::string filename = command.substr(4);
			std::ifstream file(filename, std::ios::binary);

			if (!file.is_open()) {
				std::cerr << "[-] Error: Failed open the file for reading: " << filename << '.' << std::endl;
				continue;
			}
			file.seekg(0, std::ios::end);
			std::streamsize size = file.tellg();
			file.seekg(0, std::ios::beg);

			std::byte buffer[size];
			file.read(reinterpret_cast<char*>(buffer), size);

			bytes = sendFile(sockfd_listen, filename, std::ios::beg, size);
			if (bytes == -1) {
				std::cerr << "[-] Error: Failed send the file: " << filename << '.' << std::endl;
			} else if (bytes == -2) {
				std::cerr << "[-] Error: Failed open the file: " << filename << '.' << std::endl;
			}
			continue;
		} else if (command.substr(0, 5) == command_part) {
			std::istringstream iss(command.substr(5));
			std::vector<std::string> tokens;
			std::string token;

			while (std::getline(iss, token, ':')) {
				tokens.push_back(token);
			}

			if (tokens.size() > 3) {
				sendData(sockfd_listen, command_error, MSG_CONFIRM);
				return;
			}
			auto offset = static_cast<off_t>(std::stoull(tokens[0]));
			auto size = static_cast<std::streamsize>(std::stoull(tokens[1]));

			std::string filename = tokens[2];
			for (size_t i = 3; i < tokens.size(); ++i) {
				filename += ":" + tokens[i];
			}

			bytes = sendFile(sockfd_listen, filename, offset, size);
			if (bytes == -1) {
				std::cerr << "[-] Error: Failed send the file: " << filename << '.' << std::endl;
			} else if (bytes == -2) {
				std::cerr << "[-] Error: Failed open the file: " << filename << '.' << std::endl;
			}
			continue;
		} else if (command == command_exit) {
			break;
		}
	}
}
