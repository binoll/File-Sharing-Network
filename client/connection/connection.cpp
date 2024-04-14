#include "connection.hpp"

Connection::Connection(std::string dir) : dir(std::move(dir)) {
	socket_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	socket_communicate = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socket_listen < 0 || socket_communicate < 0) {
		std::cout << "[-] Error: Failed to create socket." << std::endl;
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
		std::cout << "[-] Error: Invalid server address." << std::endl;
		return false;
	}
	if (connect(socket_listen, reinterpret_cast<struct sockaddr*>(&addr_listen), sizeof(addr_listen)) < 0) {
		std::cout << "[-] Error: Failed connect to the server." << std::endl;
		return false;
	}

	if (inet_pton(AF_INET, ip.c_str(), &addr_communicate.sin_addr) < 0) {
		std::cout << "[-] Error: Invalid server address." << std::endl;
		return false;
	}
	if (connect(socket_communicate, reinterpret_cast<struct sockaddr*>(&addr_communicate),
	            sizeof(addr_communicate)) < 0) {
		std::cout << "[-] Error: Failed connect to the server." << std::endl;
		return false;
	}

	if (sendList(socket_communicate) == -1) {
		std::cout << "[-] Error: Failed send the list of files." << std::endl;
		return false;
	}
	std::cout << "[+] The connection is established: " << ip << ':' << htons(addr_listen.sin_port) << ' ' << ip << ':'
			<< htons(addr_communicate.sin_port) << '.' << std::endl;
	thread = std::thread(&Connection::handleServer, this);
	thread.detach();
	return true;
}

int64_t Connection::getFile(const std::string& filename) {
	int64_t total_bytes;
	int64_t bytes;
	int64_t size;
	std::byte buffer[BUFFER_SIZE];
	std::string message;
	const std::string command_get = commands[1] + ":" + filename;
	const std::string& command_error = commands[3];

	if (isFileExist(filename)) {
		return -2;
	}

	std::ofstream file(filename, std::ios::binary);
	if (!file.is_open()) {
		return -1;
	}

	bytes = sendMessage(socket_communicate, command_get, MSG_CONFIRM);
	if (bytes < 0) {
		return -1;
	}

	bytes = receiveMessage(socket_communicate, message, MSG_WAITFORONE);
	if (bytes < 0 || message == command_error) {
		return -1;
	} else if (bytes == 0) {
		return 0;
	}

	total_bytes = bytes;
	size = processResponse(message);

	if (!message.empty()) {
		file.write(reinterpret_cast<char*>(message.data()), static_cast<int64_t>(message.size()));
	}

	while (size > 0) {
		int64_t bytes_to_receive = (size > BUFFER_SIZE) ? BUFFER_SIZE : size;

		bytes = receiveBytes(socket_communicate, buffer, bytes_to_receive, MSG_WAITFORONE);
		if (bytes < 0) {
			return -1;
		}
		file.write(reinterpret_cast<char*>(buffer), bytes);
		size -= bytes;
		total_bytes += bytes;
	}
	return total_bytes;
}

int64_t Connection::getList(std::vector<std::string>& list) {
	int64_t bytes;
	int64_t total_bytes;
	int64_t size;
	std::string message;
	const std::string& command_list = commands[0];
	const std::string& command_error = commands[3];

	bytes = sendMessage(socket_communicate, command_list, MSG_CONFIRM);
	if (bytes < 0) {
		return -1;
	}

	bytes = receiveMessage(socket_communicate, message, MSG_WAITFORONE);
	if (bytes < 0 || message == command_error) {
		return -1;
	}

	total_bytes = 0;
	size = processResponse(message);

	while (true) {
		std::istringstream iss(message);
		std::string filename;

		while (std::getline(iss, filename, ' ')) {
			list.emplace_back(filename);
		}

		if ((size -= bytes) <= 0) {
			break;
		}

		bytes = receiveMessage(socket_communicate, message, MSG_WAITFORONE);
		if (bytes < 0 || message == command_error) {
			return -1;
		}
		total_bytes += bytes;
	}
	return total_bytes;
}

bool Connection::exit() {
	const std::string& command_exit = commands[2];
	return sendMessage(socket_communicate, command_exit, MSG_CONFIRM) > 0;
}

bool Connection::isConnection() const {
	return checkConnection(socket_communicate) && checkConnection(socket_listen);
}

void Connection::handleServer() {
	int64_t bytes;
	const std::string& command_list = commands[0];
	const std::string& command_get = commands[1] + ':';
	const std::string& command_exit = commands[2];
	const std::string& command_error = commands[3];

	while (isConnection()) {
		std::string command;
		receiveMessage(socket_listen, command, MSG_DONTWAIT);

		if (command == command_list) {
			bytes = sendList(socket_communicate);
			if (bytes < 0) {
				sendMessage(socket_communicate, command_error, MSG_CONFIRM);
				continue;
			}
		} else if (command.substr(0, 4) == command_get) {
			std::istringstream iss(command.substr(4));
			std::vector<std::string> tokens;
			std::string token;

			while (std::getline(iss, token, ':')) {
				tokens.push_back(token);
			}

			if (tokens.size() < 3) {
				sendMessage(socket_communicate, command_error, MSG_CONFIRM);
				std::cout << "[-] Error: Invalid command format." << std::endl;
				continue;
			}

			int64_t offset;
			int64_t size;
			std::ostringstream stream;
			try {
				offset = static_cast<int64_t>(std::stoull(tokens[0]));
				size = static_cast<int64_t>(std::stoull(tokens[1]));
				stream << tokens[2];
			} catch (const std::exception& err) {
				sendMessage(socket_communicate, command_error, MSG_CONFIRM);
				std::cout << "[-] Error: Invalid command format." << std::endl;
				return;
			}

			for (uint64_t i = 3; i < tokens.size(); ++i) {
				stream << ":" << tokens[i];
			}
			const std::string filename = stream.str();

			bytes = sendFile(socket_communicate, filename, offset, size);
			if (bytes == -1) {
				sendMessage(socket_communicate, command_error, MSG_CONFIRM);
				std::cout << "[-] Error: Failed to send the file: " << filename << '.' << std::endl;
			} else if (bytes == -2) {
				sendMessage(socket_communicate, command_error, MSG_CONFIRM);
				std::cout << "[-] Error: Failed to open the file: " << filename << '.' << std::endl;
			}
			continue;
		} else if (command == command_exit) {
			break;
		}
	}
}

int64_t Connection::sendFile(int32_t socket, const std::string& filename, int64_t offset, int64_t size) {
	std::ifstream file(filename, std::ios::binary);
	std::byte buffer[BUFFER_SIZE];
	int64_t total_bytes = 0;

	if (!file.is_open()) {
		return -2;
	}

	file.seekg(offset, std::ios::beg);

	while (total_bytes < size && !file.eof()) {
		const int64_t bytes_to_read = std::min<int64_t>(BUFFER_SIZE, size - total_bytes);
		file.read(reinterpret_cast<char*>(buffer), bytes_to_read);

		const int64_t bytes_sent = sendBytes(socket, buffer, bytes_to_read, MSG_CONFIRM);
		if (bytes_sent < 0) {
			return -1;
		}
		total_bytes += bytes_sent;
	}
	return total_bytes;
}

int64_t Connection::sendList(int32_t socket) {
	int64_t bytes;
	int64_t total_bytes;
	std::string data;
	std::string message_size;

	total_bytes = 0;

	auto files = getListFiles();

	for (const auto& file : files) {
		std::ostringstream oss;
		std::string hash = calculateFileHash(file);
		uint64_t size = getFileSize(file);

		oss << file << ':' << size << ':' << hash << ' ';
		data += oss.str();
	}

	bytes = static_cast<int64_t>(data.size() + std::to_string(data.size() + ':').size());
	message_size = std::to_string(bytes) + ':';

	bytes = sendMessage(socket, message_size, MSG_CONFIRM);
	if (bytes < 0) {
		return -1;
	}

	bytes = sendMessage(socket, data, MSG_CONFIRM);
	if (bytes < 0) {
		return -1;
	}
	total_bytes += bytes;
	return total_bytes;
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

bool Connection::checkConnection(int32_t socket) {
	int32_t optval;
	socklen_t optlen = sizeof(optval);

	if (getsockopt(socket, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1 || optval != 0) {
		return false;
	}
	return true;
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
		std::cout << "[-] Error: " << err.what() << '.' << std::endl;
	}
	return vector;
}

uint64_t Connection::getFileSize(const std::string& filename) {
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		return -1;
	}
	return file.tellg();
}

std::string Connection::calculateFileHash(const std::string& filename) {
	uint64_t hash_result;
	std::ifstream file(filename, std::ios::binary);
	std::ostringstream oss;
	std::stringstream ss;
	std::string file_content;
	std::hash<std::string> hash_fn;
	std::lock_guard<std::mutex> lock(mutex);

	if (!file) {
		std::cout << "[-] Error: Failed to open the file." << std::endl;
		return "";
	}

	oss << file.rdbuf();
	file_content = oss.str();
	hash_result = hash_fn(file_content);
	ss << std::hex << hash_result;
	return ss.str();
}

bool Connection::isFileExist(const std::string& filename) {
	std::lock_guard<std::mutex> lock(mutex);

	try {
		std::filesystem::directory_iterator iterator(dir);
		for (const auto& entry : iterator) {
			if (std::filesystem::is_regular_file(entry) &&
					entry.path().filename() == filename) {
				return true;
			}
		}
	} catch (const std::exception& err) {
		std::cout << "[-] Error: " << err.what() << '.' << std::endl;
	}
	return false;
}
