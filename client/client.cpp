#include "client.hpp"
#include "cmd/cmd.hpp"
#include "connection/connection.hpp"

int32_t main() {
	std::filesystem::path path;
	struct data::client connection_client { };
	struct data::server connection_server { };

	path = init(connection_server, connection_client);

	if (path.empty()) {
		std::cout << "Error! Path to the dir is empty! [-]" << std::endl;
		return EXIT_FAILURE;
	}

	try {
		cmd(path);
	} catch (std::exception& err) {
		std::cout << "Error!" << err.what() << std::endl;
		return EXIT_FAILURE;
	}

	finalize(connection_server, connection_client);

	return EXIT_SUCCESS;
}

std::filesystem::path init(
		struct data::server& connection_server,
		struct data::client& connection_client) {
	std::string path;

	while (path.empty()) {
		std::cout << "Write path to dir: ";
		std::cin >> path;
		std::cout << std::endl;

		try {
			create_dir(path);
		} catch (std::system_error&) {
			std::cout << "Can not create dir! Please write another path! [-]" << std::endl;
		}
	}

	while (connection_server.sockfd > 0 &&
			connection_client.sockfd > 0) {
		try {
			if (connect_server(connection_server, connection_client) != 0) {
				return path;
			}
		} catch (std::system_error& err) {
			std::cout << "Error! " << "Try again! [-]" << std::endl;
			path.clear();
			return path;
		}
	}

	std::cout << "The connection is established! [+]";

	return path;
}

void finalize(struct data::server& connection_server,
              struct data::client& connection_client) {
	try {
		connect_close(connection_server, connection_client);
	} catch (std::system_error& err) {
		std::cout << "Error! " << "Try again! [-]" << std::endl;
		return;
	}

	std::cout << "The connection is closed! [+]";
}

void create_dir(std::string& path) {
	std::filesystem::directory_entry dir(path);

	if (!dir.exists()) {
		std::filesystem::create_directories(path);
		path = dir.path();
	} else if (dir.is_directory()) {
		return;
	} else {
		throw std::system_error();
	}
}