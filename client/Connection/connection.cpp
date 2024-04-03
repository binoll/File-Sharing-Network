#include "connection.hpp"

Connection::Connection(const std::string& dir) : dir(std::move(dir)), server_address() {
	if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		std::cerr << "Error: Can not create socket" << std::endl;
		return;
	}
}

Connection::~Connection() {
	close(fd);
}

bool Connection::connectToServer(const std::string& server_ip, size_t port) {
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port);

	if (inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr) <= 0) {
		std::cerr << "Error: Invalid address or address not supported" << std::endl;
		return false;
	}
	if (connect(fd, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
		std::cerr << "Error: Can not Connection failed" << std::endl;
		return false;
	}
	std::thread(&Connection::handleServer, this).join();
	return true;
}

int8_t Connection::getFile(const std::string& filename) {
	std::string command = commands_client[1] + ':' + filename;
	std::ofstream file(filename, std::ios::binary);
	std::string response;

	if (sendMsgToServer(reinterpret_cast<const std::byte*>(command.data()), command.size()) == -1) {
		return -1;
	}
	if (!file.is_open()) {
		std::cerr << "Error: Failed to open file for writing: " << filename << std::endl;
		return -1;
	}

	response = receive();
	size_t pos_start = response.find(start_marker);

	if (pos_start != std::string::npos) {
		response.erase(pos_start, start_marker.length());
		return -1;
	}
	do {
		size_t pos_end = response.find(end_marker);

		if (pos_end != std::string::npos) {
			response.erase(pos_end, end_marker.length());
			file << response;
			break;
		} else {
			file << response;
		}
	} while (!(response = receive()).empty());
	file.close();
	return 0;
}

std::list<std::string> Connection::getList() {
	std::string command = commands_client[0];
	std::list<std::string> list;
	std::string response;

	if (sendMsgToServer(reinterpret_cast<const std::byte*>(command.data()), command.size()) == -1) {
		return list;
	}

	response = receive();
	size_t pos_start = response.find(start_marker);

	if (pos_start != std::string::npos) {
		response.erase(pos_start, start_marker.length());
		return list;
	}
	do {
		std::istringstream iss(response);
		size_t pos_end = response.find(end_marker);
		std::string filename;
		std::string hash;

		if (pos_end != std::string::npos) {
			response.erase(pos_end, end_marker.length());

			while (std::getline(iss, filename, ' ')) {
				list.emplace_back(filename);
			}
			break;
		} else {
			while (std::getline(iss, filename, ' ')) {
				list.emplace_back(filename);
			}
		}
	} while (!(response = receive()).empty());
	return list;
}

std::list<std::string> Connection::filesList() {
	std::list<std::string> list;

	try {
		for (const auto& entry : std::filesystem::directory_iterator(dir)) {
			if (std::filesystem::is_regular_file(entry)) {
				list.push_back(entry.path().filename().string());
			}
		}
	} catch (const std::exception& err) {
		std::cerr << "Error: " << err.what() << std::endl;
		return list;
	}
	return list;
}

std::string Connection::receive() {
	std::byte buffer[BUFFER_SIZE];
	ssize_t bytesRead;

	if ((bytesRead = recv(fd, buffer, BUFFER_SIZE, 0)) > 0) {
		return std::string(reinterpret_cast<char*>(buffer), bytesRead);
	}
	return "";
}

int8_t Connection::sendListToServer(const std::list<std::string>& list) {
	if (sendMsgToServer(reinterpret_cast<const std::byte*>(start_marker.data()),
	                    start_marker.size()) == -1) {
		std::cerr << "Error: Failed to send the start marker of file list" << std::endl;
		return -1;
	}
	for (const auto& filename : list) {
		std::ostringstream oss;
		std::string hash = calculateFileHash(filename);
		oss << filename << ':' << hash << ' ';
		std::string file_info = oss.str();

		if (sendMsgToServer(reinterpret_cast<const std::byte*>(file_info.data()), file_info.size()) == -1) {
			std::cerr << "Error: Failed to send file info" << std::endl;
			return -1;
		}
	}
	if (sendMsgToServer(reinterpret_cast<const std::byte*>(end_marker.data()),
	                    end_marker.size()) == -1) {
		std::cerr << "Error: Failed to send the end marker of file list" << std::endl;
		return -1;
	}
	return 0;
}

int8_t Connection::sendFileToServer(const std::string& filename, size_t size, size_t offset) {
	std::ifstream file(filename, std::ios::binary);
	std::byte buffer[size];

	if (!file) {
		std::cerr << "Error: Failed to open file for reading: " << filename << std::endl;
		return -1;
	}
	file.seekg(offset);
	file.read(reinterpret_cast<char*>(buffer), size);
	std::string msg(reinterpret_cast<const char*>(buffer), size);

	if (send(fd, buffer, size, 0) == -1) {
		std::cerr << "Error: Failed to send the file to server: " << filename << std::endl;
		return -1;
	}
	return 0;
}

int8_t Connection::sendMsgToServer(const std::byte* buffer, size_t size) {
	if (send(fd, reinterpret_cast<const char*>(buffer), size, 0) == -1) {
		return -1;
	}
	return 0;
}

std::string Connection::calculateFileHash(const std::string& filename) {
	std::ifstream file(filename, std::ios::binary);
	std::hash<std::string> hash_fn;
	std::ostringstream oss;

	if (!file) {
		return "Error: Unable to open file";
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
			mutex.lock();
			std::list<std::string> file_list = filesList();
			mutex.unlock();
			sendListToServer(file_list);
		} else if (command.find(command_get) == 0) {
			std::istringstream iss(command.substr(4));
			size_t colon1 = command.find(':', 4);
			size_t colon2 = command.find(':', colon1 + 1);
			size_t colon3 = command.find(':', colon2 + 1);

			if (colon1 != std::string::npos && colon2 != std::string::npos && colon3 != std::string::npos) {
				size_t size = std::stoull(command.substr(4, colon1 - 4));
				size_t offset = std::stoull(command.substr(colon1 + 1, colon2 - colon1 - 1));
				std::string filename = command.substr(colon3 + 1);
				sendFileToServer(filename, size, offset);
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
				std::vector<char> buffer(size);

				if (!file) {
					std::cerr << "Error: Can not open the file for reading: " << filename << std::endl;
					continue;
				}
				file.seekg(offset);
				file.read(buffer.data(), size);
				sendFileToServer(filename, offset, size);
			}
		} else if (command.find(command_exit) == 0) {
			break;
		}
	}
}
