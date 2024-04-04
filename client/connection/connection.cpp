#include "connection.hpp"

Connection::Connection(std::string dir) : dir(std::move(dir)) {
	client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if ((client_fd) < 0) {
		std::cerr << "[-] Error: Failed to create socket." << std::endl;
		return;
	}
}

Connection::~Connection() {
	close(client_fd);
}

bool Connection::connectToServer(const std::string& server_ip, uint16_t port) {
	client_port = port;
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(client_port);

	if (inet_pton(AF_INET, server_ip.c_str(), &client_addr.sin_addr) < 0) {
		std::cerr << "[-] Error: Invalid address or address not supported." << std::endl;
		return false;
	}
	if (connect(client_fd, reinterpret_cast<struct sockaddr*>(&client_addr), sizeof(client_addr)) < 0) {
		std::cerr << "[-] Error: Failed to connect to server with client_fd." << std::endl;
		return false;
	}
	sendListToServer();
	return true;
}


int8_t Connection::getFile(const std::string& filename) {

}

std::list<std::string> Connection::getList() {
	const std::string& command_list = commands_client[0];
	const std::string& start_marker = marker[0];
	const std::string& end_marker = marker[1];
	std::list<std::string> list;
	std::string response;
	size_t pos_start;

	if (sendToServer(command_list) < 0) {
		return list;
	}
	response = receive();
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
	} while (!(response = receive()).empty());
	return list;
}

int8_t Connection::exit() {
	const std::string& command_exit = commands_client[2];
	return sendToServer(command_exit);
}

void Connection::responseToServer() {
	struct timeval timeout { };
	fd_set readfds;

	FD_ZERO(&readfds);
	FD_SET(client_fd, &readfds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	int32_t ready = select(client_fd + 1, &readfds, nullptr, nullptr, &timeout);
	if (ready != -1 && ready != 0) {
		handleServer();
	}
}

std::list<std::string> Connection::listFiles() {
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

std::string Connection::receive() const {
	std::byte buffer[BUFFER_SIZE];
	std::string received_data;
	ssize_t bytes;

	do {
		bytes = recv(client_fd, buffer, BUFFER_SIZE, MSG_WAITFORONE);
		if (bytes > 0) {
			received_data.append(reinterpret_cast<char*>(buffer), bytes);
		} else if (bytes < 0) {
			std::cerr << "[-] Error: Failed to receive data from server." << std::endl;
			return "";
		}
	} while (bytes == BUFFER_SIZE);

	return received_data;
}

int8_t Connection::sendListToServer() {
	const std::string& start_marker = marker[0];
	const std::string& end_marker = marker[1];
	std::string file_info = start_marker;
	std::list<std::string> list = listFiles();

	for (const auto& filename : list) {
		if (file_info.size() + filename.size() > BUFFER_SIZE) {
			if (sendToServer(file_info) < 0) {
				return -1;
			}
		}
		std::ostringstream oss;
		std::string hash = calculateFileHash(filename);
		oss << filename << ':' << hash << ' ';
		file_info += oss.str();
	}
	if (sendToServer(file_info) < 0 || sendToServer(end_marker) < 0) {
		return -1;
	}
	return 0;
}

int8_t Connection::sendFileToServer(const std::string& filename, size_t size, size_t offset) {

}

int8_t Connection::sendToServer(const std::string& command) const {
	ssize_t bytes;

	bytes = send(client_fd, command.c_str(), command.size(), MSG_CONFIRM);
	if (bytes < 0) {
		std::cerr << "[-] Error: Failed to send command." << std::endl;
		return -1;
	}
	return 0;
}

bool Connection::checkConnection() const {
	return getsockopt(client_fd, SOL_SOCKET, SO_ERROR, nullptr, nullptr);
}

std::string Connection::calculateFileHash(const std::string& filename) {
	std::ifstream file(filename, std::ios::binary);
	std::hash<std::string> hash_fn;
	std::ostringstream oss;

	if (!file) {
		std::cerr << "[-] Error: Failed to open the file." << std::endl;
	}
	oss << file.rdbuf();
	std::string file_content = oss.str();
	size_t hash_result = hash_fn(file_content);
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

	while (checkConnection() == 0) {
		command = receive();

		if (command == command_list) {
			sendListToServer();
		} else if (command == command_get) {
			size_t colon1 = command.find(':', 4);
			size_t colon2 = command.find(':', colon1 + 1);
			size_t colon3 = command.find(':', colon2 + 1);

			if (colon1 != std::string::npos && colon2 != std::string::npos && colon3 != std::string::npos) {
				size_t size = std::stoull(command.substr(4, colon1 - 4));
				size_t offset = std::stoull(command.substr(colon1 + 1, colon2 - colon1 - 1));
				std::string filename = command.substr(colon3 + 1);

				sendFileToServer(filename, size, offset);
			} else {
				std::cerr << "[-] Error: Invalid command format." << std::endl;
			}
		} else if (command == command_part) {
			std::istringstream iss(command.substr(5));
			size_t colon1 = command.find(':', 5);
			size_t colon2 = command.find(':', colon1 + 1);
			size_t colon3 = command.find(':', colon2 + 1);

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
				sendFileToServer(filename, offset, size);
			}
		} else if (command == command_exit) {
			break;
		}
	}
}
