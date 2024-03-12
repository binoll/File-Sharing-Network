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

int8_t Connection::list(std::list<std::filesystem::path>& list) {

}

int8_t Connection::get(const std::filesystem::path& path) {

}

int8_t Connection::update_dir(const std::string& path) {
	return this->dir.set_work_path(path);
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

int8_t Connection::send_file(const std::string& path) {
	std::byte buf[ONE_KB];
	int64_t size;
	int64_t off = 0;

	while (true) {
		if ((size = this->dir.get_file(path, buf, off, ONE_KB)) == -1) {
			return -1;
		} else if (size == -2) {
			break;
		}

		if (send(this->server.fd, buf, size, 0) == -1) {
			return -1;
		}
		off += ONE_KB;
	}
	return 0;
}

int8_t Connection::send_err(const std::string& err) {
	uint64_t size = err.size();
	std::byte buf[size];

	if (send(this->server.fd, buf, size, 0) == -1) {
		return -1;
	}
	return 0;
}

void Connection::send_response(std::byte* buf, uint64_t size) {
	std::string request(reinterpret_cast<char*>(buf), size);

	if (request == "list") {
		if (this->send_list()) {
			std::cerr << "Error: Can not send list of files." << std::endl;
		}
	} else if (request.find("get ") == 0) {
		std::string filename = request.substr(4);

		if (this->send_file(filename) == -1) {
			std::cerr << "Error: Can not send file." << std::endl;
		}
	} else {
		std::string err = "Error: Unrecognized request.";

		if (this->send_err(err) == -1) {
			std::cerr << "Error: Can send error." << std::endl;
		}
	}
}
