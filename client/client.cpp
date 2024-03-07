#include "client.hpp"
#include "cmd/cmd.hpp"
#include "connection/connection.hpp"

int32_t main() {
	std::filesystem::path path;
	struct data::client connection_client { };
	struct data::server connection_server { };

	try {
		path = init(connection_server, connection_client);

		cmd(path);
		finalize(connection_server, connection_client);
	} catch (std::exception& err) {
		std::cout << "Error!" << err.what() << std::endl;
		return EXIT_FAILURE;
	}

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

	while (connection_server.sockfd != -1 &&
			connection_client.sockfd != -1) {
		try {
			connect_server(connection_server, connection_client);
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
		std::cout << "Error! " << err.what() << std::endl
				<< "Try again! [-]" << std::endl;
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