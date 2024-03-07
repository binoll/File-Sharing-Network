#include "client.hpp"
#include "cmd/cmd.hpp"
#include "connection/connection.hpp"

int32_t main() {
	std::filesystem::path path;
	struct data::client connection_client { };
	struct data::server connection_server { };

	try {
		path = init(connection_server, connection_client);

		connect_server(connection_server, connection_client);
		cmd(path);
		connect_close(connection_server, connection_client);
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
			std::cout << "Can not create dir! Please write another path!" << std::endl;
		}
	}

	return path;
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