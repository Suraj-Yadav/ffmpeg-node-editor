#include "string_utils.hpp"

namespace str {
	std::vector<std::string_view> split(std::string_view str, char splitter) {
		std::vector<std::string_view> array;
		std::string_view current(str.data(), 0);
		for (char ch : str) {
			if (ch == splitter) {
				if (!current.empty()) { array.push_back(current); }
				current =
					std::string_view(current.data() + current.size() + 1, 0);
			} else {
				current = std::string_view(current.data(), current.size() + 1);
			}
		}
		if (!current.empty()) array.push_back(current);
		return array;
	}

	bool match(
		std::string_view txt, const std::regex& re,
		std::initializer_list<std::reference_wrapper<std::string_view>> dest) {
		std::cmatch parts;
		if (!std::regex_search(
				txt.data(), txt.data() + txt.size(), parts, re)) {
			return false;
		}
		size_t i = 1;
		for (auto& elem : dest) {
			if (i >= parts.size()) { return false; }
			elem.get() = std::string_view(parts[i].first, parts[i].length());
			i++;
		}
		return i >= parts.size();
	}

};	// namespace str
