#include "connection.hpp"

Connection::Connection(const std::string& path) : dir(path), server() {
	std::memset(&this->server.addr, 0, sizeof(sockaddr_in));
}

Connection::~Connection() {
	try {
		this->listen.~thread();
	} catch (std::exception& err) {
		std::cerr << "Error: Can not terminate the thread." << std::endl;
	}

	if (close(this->server.fd) == -1) {
		std::cerr << "Error: Can not close the socket." << std::endl;
	}
}

int8_t Connection::create_connection(const std::string& ip, uint64_t port) {
	std::memset(&this->server.addr, 0, sizeof(sockaddr_in));

	if ((this->server.fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		std::cerr << "Error: Can not create the socket." << std::endl;
		return -1;
	}

	if (inet_pton(AF_INET, ip.c_str(), &this->server.addr.sin_addr) == -1) {
		std::cerr << "Error: Bad ip-address ot port." << std::endl;
		return -1;
	}

	this->server.addr.sin_family = AF_INET;
	this->server.addr.sin_port = htons(port);

	if (connect(this->server.fd,
	            reinterpret_cast<const sockaddr*>(&this->server.addr),
	            sizeof(struct sockaddr_in)) == -1) {
		std::cerr << "Error: Can not connect to the server." << std::endl;
		return -1;
	}

	listen = std::thread(&Connection::listen_server, this);
	listen.detach();
	return 0;
}

int8_t Connection::update_dir(const std::string& path) {
	return this->dir.set_work_path(path);
}

int8_t Connection::list(std::list<std::string>& list) {
	std::lock_guard<std::mutex> lock(mutex);
	std::byte buf[ONE_KB];
	int64_t bytes;
	std::string command = "list";

	if (this->send_msg(command) == -1) {
		return -1;
	}

	if ((bytes = recv(this->server.fd, buf, sizeof(buf), 0)) <= 0) {
		return -1;
	}

	std::string msg(reinterpret_cast<char*>(buf), bytes);

	if (msg == START_MSG) {
		while (true) {
			if ((bytes = recv(this->server.fd, buf, sizeof(buf), 0)) <= 0) {
				return -1;
			}

			std::string msg(reinterpret_cast<char*>(buf), bytes);

			if (msg == END_MSG) {
				break;
			}

			list.emplace_back(msg);
		}
	} else {
		return -1;
	}
	return 0;
}

int8_t Connection::get(const std::filesystem::path& path) {
	std::lock_guard<std::mutex> lock(mutex);
	std::byte buf[ONE_KB];
	int64_t bytes;
	std::string command = "get:" + path.filename().string();

	if (this->send_msg(command) == -1) {
		return -1;
	}

	if ((bytes = recv(this->server.fd, buf, sizeof(buf), 0)) <= 0) {
		return -1;
	}

	std::string msg(reinterpret_cast<char*>(buf), bytes);

	if (msg == START_MSG) {
		while (true) {
			if ((bytes = recv(this->server.fd, buf, sizeof(buf), 0)) <= 0) {
				return -1;
			}

			if (msg == END_MSG) {
				break;
			}

			if (this->dir.set_file(path, buf, bytes) == -1) {
				return -1;
			}
		}
	} else {
		return -1;
	}
	return 0;
}

void Connection::listen_server() {
	std::byte buf[ONE_KB];
	int64_t bytes;

	while (true) {
		std::lock_guard<std::mutex> lock(mutex);

		if ((bytes = recv(this->server.fd, buf, sizeof(buf), 0)) > 0) {
			this->send_response(buf, bytes);
		}
	}
}

int8_t Connection::send_list() {
	for (auto& entry : std::filesystem::directory_iterator(this->dir.get_work_path())) {
		const std::filesystem::path& filename = entry.path().filename();

		if (send(this->server.fd, filename.c_str(), filename.string().size(), 0) == -1) {
			return -1;
		}
	}
	return 0;
}

int8_t Connection::send_file(const std::string& path, int64_t offset, int64_t size) {
	std::byte buf[size];
	int64_t bytes;

	while (true) {
		if ((bytes = this->dir.get_file(path, buf, offset, size)) == -1) {
			return -1;
		} else if (bytes == -2) {
			break;
		}

		if (send(this->server.fd, buf, size, 0) == -1) {
			return -1;
		}
		offset += size;
	}
	return 0;
}

int8_t Connection::send_file(const std::string& path, int64_t size) {
	std::byte buf[size];
	int64_t offset = 0;
	int64_t bytes;

	while (true) {
		if ((bytes = this->dir.get_file(path, buf, offset, size)) == -1) {
			return -1;
		} else if (bytes == -2) {
			break;
		}

		if (send(this->server.fd, buf, size, 0) == -1) {
			return -1;
		}
		offset += size;
	}
	return 0;
}

int8_t Connection::send_msg(const std::string& msg) {
	if (send(this->server.fd, msg.c_str(), msg.size(), 0) == -1) {
		return -1;
	}
	return 0;
}

void Connection::send_response(std::byte* buf, uint64_t bytes) {
	std::string msg(reinterpret_cast<char*>(buf), bytes);

	if (msg == "list") {
		if (this->send_list() == -1) {
			return;
		}
	} else if (msg.find("part:") == 0) {
		std::regex regex("part:([0-9]+):([0-9]+):(.+)");
		std::smatch match;
		std::string filename;
		int64_t offset = 0;
		int64_t size = 0;

		if (std::regex_match(msg, match, regex)) {
			offset = std::stoll(match[1]);
			size = std::stoll(match[2]);
			filename = match[3];
		}

		if (this->send_file(filename, offset, size) == -1) {
			return;
		}
	} else if (msg.find("get:") == 0) {
		std::regex regex("get:([0-9]+):(.+)");
		std::smatch match;
		std::string filename;
		int64_t size = 0;

		if (std::regex_match(msg, match, regex)) {
			size = std::stoll(match[1]);
			filename = match[2];
		}

		if (send(this->server.fd, START_MSG.c_str(), START_MSG.size(), 0) == -1 ||
				this->send_file(filename, size) == -1 ||
				send(this->server.fd, END_MSG.c_str(), END_MSG.size(), 0) == -1) {
			return;
		}
	} else {
		std::string err = "err";

		if (this->send_msg(err) == -1) {
			return;
		}
	}
}
