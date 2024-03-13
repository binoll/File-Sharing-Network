#include "files.hpp"

Files::Files(const uint64_t id, const std::string& filename, const std::string& hash) {
	this->add(id, filename, hash);
}

void Files::add(const uint64_t id, const std::string& filename, const std::string& hash) {
	list[hash].emplace_back(id, filename);
}

std::vector<std::pair<uint64_t, std::string>> Files::find_by_hash(const std::string& hash) {
	std::vector<std::pair<uint64_t, std::string>> result;
	auto it = list.find(hash);

	if (it != list.end()) {
		for (auto& file : it->second) {
			result.push_back(file);
		}
	}
	return result;
}

std::vector<std::pair<uint64_t, std::string>> Files::find_by_id(const uint64_t id) {
	std::vector<std::pair<uint64_t, std::string>> result;

	for (const auto& pair : list) {
		for (const auto& file : pair.second) {
			if (file.first == id) {
				result.push_back(file);
			}
		}
	}
	return result;
}

void Files::remove_by_id(const uint64_t id) {
	for (auto& pair : list) {
		auto& files = pair.second;
		auto func = [id](const auto& file) {
			return file.first == id;
		};
		auto it = std::remove_if(files.begin(), files.end(), func);

		files.erase(it, files.end());
	}
}

void Files::remove_by_filename(const std::string& filename) {
	for (auto& pair : list) {
		auto& files = pair.second;
		auto func = [filename](const auto& file) {
			return file.second == filename;
		};
		auto it = std::remove_if(files.begin(), files.end(), func);

		files.erase(it, files.end());
	}
}
