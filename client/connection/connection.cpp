// Copyright 2024 binoll
#include "connection.hpp"

Connection::Connection(std::string dir) : dir(std::move(dir)) {
	socket_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	socket_communicate = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (socket_listen < 0) {
		std::cout << "[-] Failed to create socket for listen" << std::endl;
		return;
	}

	if (socket_communicate < 0) {
		std::cout << "[-] Failed to create socket for communicate" << std::endl;
		return;
	}
}

Connection::~Connection() {
	if (thread.joinable()) {
		thread.join();
	}

	close(socket_listen);
	close(socket_communicate);
}

bool Connection::connectToServer(const std::string& ip, int32_t port_listen, int32_t port_communicate) {
	addr_listen.sin_family = AF_INET;
	addr_communicate.sin_family = AF_INET;
	addr_listen.sin_port = htons(port_listen);
	addr_communicate.sin_port = htons(port_communicate);

	if (inet_pton(AF_INET, ip.c_str(), &addr_listen.sin_addr) < 0) {
		std::cout << "[-] Invalid server address" << std::endl;
		return false;
	}

	if (connect(socket_listen, reinterpret_cast<struct sockaddr*>(&addr_listen), sizeof(addr_listen)) < 0) {
		std::cout << "[-] Failed connect to the server" << std::endl;
		return false;
	}

	if (inet_pton(AF_INET, ip.c_str(), &addr_communicate.sin_addr) < 0) {
		std::cout << "[-] Invalid server address" << std::endl;
		return false;
	}

	if (connect(socket_communicate, reinterpret_cast<struct sockaddr*>(&addr_communicate),
	            sizeof(addr_communicate)) < 0) {
		std::cout << "[-] Failed connect to the server" << std::endl;
		return false;
	}

	if (sendList(socket_listen) < 0) {
		std::cout << "[-] Failed send the list of files" << std::endl;
		return false;
	}

	std::cout << "[+] The connection is established: " << ip << ':' << htons(addr_listen.sin_port) << ' '
			<< ip << ':' << htons(addr_communicate.sin_port) << std::endl;

	thread = std::thread(&Connection::processingServer, this);
	thread.detach();

	return true;
}

int64_t Connection::getFile(const std::string& filename) const {
	char buffer[BUFFER_SIZE];
	int64_t total_bytes;
	int64_t bytes;
	int64_t message_size;
	std::string message;
	std::ofstream file;
	const std::string& command_get = commands[1] + ':' + filename;
	const std::string& command_error = commands[2];
	const std::string& command_exist = commands[3];

	bytes = sendMessage(socket_communicate, command_get, MSG_CONFIRM | MSG_NOSIGNAL);
	if (bytes < 0) {
		file.close();
		std::remove(filename.c_str());
		return -1;
	}

	bytes = receiveMessage(socket_communicate, message, MSG_NOSIGNAL);
	if (bytes < 0 || message == command_error) {
		return -1;
	} else if (message == command_exist) {
		return -2;
	}

	file = std::ofstream(filename, std::ios::binary);
	if (!file.is_open()) {
		std::remove(filename.c_str());
		return -1;
	}

	message_size = processResponse(message);
	if (message == command_error || message_size == -1) {
		file.close();
		std::remove(filename.c_str());
		return -1;
	}

	file.write(message.data(), static_cast<int64_t>(message.size()));
	message_size -= static_cast<int64_t>(message.size());
	total_bytes = static_cast<int64_t>(message.size());

	while (message_size > 0) {
		bytes = receiveBytes(socket_communicate, buffer, sizeof(buffer), MSG_NOSIGNAL);
		if (bytes < 0) {
			file.close();
			std::remove(filename.c_str());
			return -1;
		}

		file.write(reinterpret_cast<char*>(buffer), bytes);

		message_size -= bytes;
		total_bytes += bytes;

		if (message_size <= 0) {
			file.close();
			break;
		}
	}

	return total_bytes;
}

int64_t Connection::getList(std::vector<std::string>& list) const {
	int64_t bytes;
	int64_t total_bytes;
	int64_t message_size;
	std::string message;
	const std::string& command_list = commands[0];
	const std::string& command_error = commands[2];

	bytes = sendMessage(socket_communicate, command_list, MSG_CONFIRM | MSG_NOSIGNAL);
	if (bytes < 0) {
		return -1;
	}

	bytes = receiveMessage(socket_communicate, message, MSG_NOSIGNAL);
	if (bytes < 0 || message == command_error) {
		return -1;
	}

	message_size = processResponse(message);
	if (message == command_error) {
		return -1;
	}

	total_bytes = 0;

	while (message_size > 0) {
		std::istringstream iss(message);
		std::string filename;

		while (std::getline(iss, filename, ' ')) {
			list.emplace_back(filename);
		}

		message_size -= static_cast<int64_t>(message.size());
		total_bytes += static_cast<int64_t>(message.size());

		if (message_size <= 0) {
			break;
		}

		bytes = receiveMessage(socket_communicate, message, MSG_NOSIGNAL);
		if (bytes < 0 || message == command_error) {
			return -1;
		}
	}

	return total_bytes;
}

void Connection::processingServer() {
	int64_t bytes;
	const std::string& command_list = commands[0];
	const std::string& command_get = commands[1];
	const std::string& command_error = commands[2];

	while (true) {
		std::string command = "empty";
		std::vector<std::string> list;

		receiveMessage(socket_listen, command, MSG_DONTWAIT | MSG_NOSIGNAL);

		if (command == command_list) {
			bytes = sendList(socket_listen);
			if (bytes == -1) {
				std::cout << std::endl << "[-] Failed send list of files" << std::endl;
				sendMessage(socket_listen, command_error, MSG_CONFIRM | MSG_NOSIGNAL);
			}
		} else if (command.compare(0, command_get.length(), command_get) == 0) {
			std::vector<std::string> tokens;
			int64_t offset;
			int64_t size;
			std::string filename;

			split(command, ':', tokens);

			if (tokens.size() < 4) {
				sendMessage(socket_listen, command_error, MSG_CONFIRM | MSG_NOSIGNAL);
				std::cout << std::endl << "[-] Invalid command format" << std::endl;
				continue;
			}

			try {
				offset = static_cast<int64_t>(std::stoull(tokens[1]));
				size = static_cast<int64_t>(std::stoull(tokens[2]));
				filename = tokens[3];
			} catch (const std::exception& err) {
				sendMessage(socket_listen, command_error, MSG_CONFIRM | MSG_NOSIGNAL);
				std::cout << std::endl << "[-] Invalid command format" << std::endl;
				continue;
			}

			bytes = sendFile(socket_listen, filename, offset, size);
			if (bytes == -1) {
				sendMessage(socket_listen, command_error, MSG_CONFIRM | MSG_NOSIGNAL);
				std::cout << std::endl << "[-] Failed to send the file: " << filename << std::endl;
			} else if (bytes == -2) {
				sendMessage(socket_listen, command_error, MSG_CONFIRM);
				std::cout << std::endl << "[-] Failed to open the file: " << filename << std::endl;
			}
		} else if (command.empty()) {
			break;
		}
	}
}

int64_t Connection::sendFile(int32_t socket, const std::string& filename, int64_t offset, int64_t size) {
	std::lock_guard<std::mutex> lock(mutex);

	std::ifstream file(filename, std::ios::binary);
	char buffer[BUFFER_SIZE];
	int64_t total_bytes = 0;

	if (!file.is_open()) {
		return -2;
	}

	file.seekg(offset, std::ios::beg);

	while (total_bytes < size && !file.eof()) {
		const int64_t bytes_to_read = std::min<int64_t>(BUFFER_SIZE, size - total_bytes);
		file.read(reinterpret_cast<char*>(buffer), bytes_to_read);

		const int64_t bytes = sendBytes(socket, buffer, bytes_to_read, MSG_CONFIRM | MSG_NOSIGNAL);
		if (bytes < 0) {
			return -1;
		}

		total_bytes += bytes;
	}

	return total_bytes;
}

int64_t Connection::sendList(int32_t socket) {
	int64_t bytes;
	int64_t total_bytes;
	std::string data;
	std::string message_size;

	total_bytes = 0;
	auto filenames = getListFiles();

	for (const auto& filename : filenames) {
		std::ostringstream oss;
		std::string hash = calculateFileHash(filename);
		int64_t size = getFileSize(filename);

		if (hash.empty() || size < 0) {
			return -1;
		}

		oss << filename << ':' << size << ':' << hash << ' ';
		data += oss.str();
	}

	message_size = std::to_string(data.size()) + ':';

	bytes = sendMessage(socket, message_size, MSG_CONFIRM | MSG_NOSIGNAL);
	if (bytes < 0) {
		return -1;
	}

	bytes = sendMessage(socket, data, MSG_CONFIRM | MSG_NOSIGNAL);
	if (bytes < 0) {
		return -1;
	}

	total_bytes += bytes;

	return total_bytes;
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

std::vector<std::string> Connection::getListFiles() {
	std::vector<std::string> vector;
	std::lock_guard<std::mutex> lock(mutex);

	try {
		std::filesystem::directory_iterator iterator(dir);

		for (const auto& entry : iterator) {
			if (std::filesystem::is_regular_file(entry)) {
				vector.push_back(entry.path().filename().string());
			}
		}
	} catch (const std::exception& err) {
		std::cout << err.what() << std::endl;
	}

	return vector;
}

int64_t Connection::getFileSize(const std::string& filename) {
	std::lock_guard<std::mutex> lock(mutex);
	std::ifstream file(filename, std::ios::binary | std::ios::ate);

	if (!file.is_open()) {
		return -1;
	}

	return file.tellg();
}

std::string Connection::calculateFileHash(const std::string& filename) {
	std::ifstream file(filename, std::ios::binary);
	char buffer[8192];
	unsigned char hash[EVP_MAX_MD_SIZE];
	uint32_t lengthOfHash = 0;
	std::ostringstream oss;

	if (!file.is_open()) {
		return "";
	}

	EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
	if (mdctx == nullptr) {
		return "";
	}

	if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
		EVP_MD_CTX_free(mdctx);
		return "";
	}

	while (file.read(buffer, sizeof(buffer))) {
		if (EVP_DigestUpdate(mdctx, buffer, file.gcount()) != 1) {
			EVP_MD_CTX_free(mdctx);
			return "";
		}
	}

	if (file.gcount() > 0) {
		if (EVP_DigestUpdate(mdctx, buffer, file.gcount()) != 1) {
			EVP_MD_CTX_free(mdctx);
			return "";
		}
	}

	if (EVP_DigestFinal_ex(mdctx, hash, &lengthOfHash) != 1) {
		EVP_MD_CTX_free(mdctx);
		return "";
	}

	EVP_MD_CTX_free(mdctx);

	for (uint32_t i = 0; i < lengthOfHash; ++i) {
		oss << std::hex << std::setw(2) << std::setfill('0') << (int) hash[i];
	}

	return oss.str();
}

void Connection::split(const std::string& str, char delim, std::vector<std::string>& tokens) {
	std::string token;
	std::istringstream tokenStream(str);

	while (std::getline(tokenStream, token, delim)) {
		tokens.push_back(token);
	}
}
