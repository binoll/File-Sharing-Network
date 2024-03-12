#include "dir.hpp"

Dir::Dir(const std::string& path) : work_path(path) { }

int8_t Dir::list_files(std::list<std::filesystem::path>& list) {
	if (!list.empty()) {
		list.clear();
	}

	if (this->work_path.empty()) {
		std::cerr << "Error: Work path is empty." << std::endl;
		return -1;
	}

	for (auto& file : this->work_path) {
		list.push_back(file.filename());
	}
	return 0;
}

int64_t Dir::get_file(const std::filesystem::path& path, std::byte* buf, int64_t off, int64_t size) {
	if (path.empty()) {
		std::cerr << "Error: User path is empty." << std::endl;
		return -1;
	}

	if (this->work_path.empty()) {
		std::cerr << "Error: Work path is empty." << std::endl;
		return -1;
	}

	for (auto& item : this->work_path) {
		if (!item.filename().compare(path.filename())) {
			continue;
		}

		std::ifstream file(path, std::ios_base::binary);
		int64_t size_read;

		if (!file.is_open()) {
			std::cerr << "Error: Can not open the file." << std::endl;
			return -1;
		}

		file.seekg(std::streampos(off), std::ios_base::beg);

		if (file.tellg() == -1) {
			file.close();
			return -2;
		}

		size_read = std::min<int64_t>(size, ONE_KB);
		file.read(reinterpret_cast<char*>(buf), size_read);

		if (file.gcount() <= 0) {
			std::cerr << "Error: Can read from the file." << std::endl;
			file.close();
			return -1;
		}
		return size_read;
	}
	return -1;
}

int8_t Dir::set_work_path(const std::string& path) {
	std::filesystem::directory_entry entry(path);

	if (!entry.exists()) {
		try {
			std::filesystem::create_directory(entry);
		} catch (std::exception& err) {
			std::cerr << "Error: Can not create the dir." << std::endl;
			return -1;
		}
	}

	if (!entry.is_directory()) {
		std::cerr << "Error: The path is not dir." << std::endl;
		return -1;
	}

	this->work_path = std::filesystem::path(path);
	return 0;
}

std::filesystem::path Dir::get_path() {
	return this->work_path;
}
