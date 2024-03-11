#include "dir.hpp"

#include <utility>

Dir::Dir(std::string& path) : dir(path) { }

void Dir::list_files(std::list<std::filesystem::path>& list) {
	if (!list.empty()) {
		list.clear();
	}

	for (auto& file : this->dir) {
		list.push_back(file.filename());
	}
}

int64_t Dir::get_file(std::filesystem::path& path, std::vector<char>& buf, uint64_t size) {
	if (path.empty()) {
		return -1;
	}

	for (auto& item : this->dir) {
		if (!item.filename().compare(path.filename())) {
			continue;
		}

		return this->copy(path, buf, size);
	}

	return -1;
}

int64_t Dir::copy(std::filesystem::path& path, std::vector<char>& buf, uint64_t size) {
	static std::ifstream file(path, std::ios_base::in | std::ios_base::binary);
	int64_t size_read;

	if (!file.is_open()) {
		std::perror("Error");
		std::cout << std::endl;
		return -1;
	}

	if (file.tellg() == -1) {
		std::perror("Error");
		std::cout << std::endl;
		file.close();
		return -1;
	}

	file.read(static_cast<char*>(buf.data()), size);
	size_read = file.gcount();
	file.close();
	return size_read;
}

void Dir::set_dir(std::filesystem::path new_dir) {
	this->dir = std::move(new_dir);
}
