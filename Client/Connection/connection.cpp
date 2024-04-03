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
	client_addr.sin_port = htons(port);

	if (inet_pton(AF_INET, server_ip.c_str(), &client_addr.sin_addr) <= 0) {
		std::cerr << "[-] Error: Invalid address or address not supported." << std::endl;
		return false;
	}
	if (connect(client_fd, reinterpret_cast<struct sockaddr*>(&client_addr), sizeof(client_addr)) < 0) {
		std::cerr << "[-] Error: Failed connect to server." << std::endl;
		return false;
	}
	std::thread(&Connection::handleServer, this).detach();
	return true;
}

int8_t Connection::getFile(const std::string& filename) {

}

std::list<std::string> Connection::getList() {
	const std::string& command_list = commands_client[0];
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
		std::string hash;

		while (std::getline(iss, filename, ' ') && filename != end_marker) {
			list.emplace_back(filename);
		}
		if (filename == end_marker) {
			break;
		}
	} while (!(response = receive()).empty());
	return list;
}

std::list<std::string> Connection::listFiles() {
	std::list<std::string> list;

	try {
		for (const auto& filename : std::filesystem::directory_iterator(dir)) {
			if (std::filesystem::is_regular_file(filename)) {
				list.push_back(filename.path().filename());
			}
		}
	} catch (const std::exception& err) {
		std::cerr << "[-] Error: " << err.what() << std::endl;
		return list;
	}
	return list;
}

std::string Connection::receive() {
	std::byte buffer[BUFFER_SIZE];
	ssize_t bytes = recv(client_fd, buffer, BUFFER_SIZE, 0);
	if (bytes > 0) {
		return std::string(reinterpret_cast<const char*>(buffer), bytes);
	}
	return "";
}

int8_t Connection::sendListToServer(const std::list<std::string>& list) {
	std::string file_info = start_marker;

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
	if (sendToServer(file_info) < 0) {
		return -1;
	}
	if (sendToServer(end_marker) < 0) {
		return -1;
	}
	return 0;
}

int8_t Connection::sendFileToServer(const std::string& filename, size_t size, size_t offset) {

}

int8_t Connection::sendToServer(const std::string& msg) const {
	if (send(client_fd, msg.c_str(), msg.size(), 0) < 0) {
		std::cerr << "[-] Error: Failed to send msg." << std::endl;
		return -1;
	}
	return 0;
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

	while (!(command = receive()).empty()) {
		if (command == command_list) {
			std::list<std::string> file_list = listFiles();
			sendListToServer(file_list);
		} else if (command.find(command_get) == 0) {
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
		} else if (command.find(command_part) == 0) {
			std::istringstream iss(command.substr(5));
			size_t colon1 = command.find(':', 5);
			size_t colon2 = command.find(':', colon1 + 1);
			size_t colon3 = command.find(':', colon2 + 1);

			if (colon1 != std::string::npos && colon2 != std::string::npos && colon3 != std::string::npos) {
				size_t offset = std::stoull(command.substr(5, colon1 - 5));
				size_t size = std::stoull(command.substr(colon1 + 1, colon2 - colon1 - 1));
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
		} else if (command.find(command_exit) == 0) {
			break;
		}
	}
}
