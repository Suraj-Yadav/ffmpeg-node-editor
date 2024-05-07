#include <algorithm>
#include <cctype>
#include <charconv>
#include <regex>
#include <string_view>

namespace str {
	inline bool ignore_case_cmp(const char& x, const char& y) {
		return std::tolower(x) == std::tolower(y);
	}

	inline bool isspace(const char& ch) { return std::isspace(ch) != 0; }

	inline bool starts_with(
		const std::string_view& text, const std::string_view& prefix,
		bool ignoreCase = false) {
		if (!prefix.empty() && text.size() < prefix.size()) { return false; }
		if (ignoreCase) {
			return std::equal(
				prefix.begin(), prefix.end(), text.begin(), ignore_case_cmp);
		}
		return std::equal(prefix.begin(), prefix.end(), text.begin());
	}

	inline bool ends_with(
		const std::string_view& text, const std::string_view& suffix,
		bool ignoreCase = false) {
		if (!suffix.empty() && text.size() < suffix.size()) { return false; }
		if (ignoreCase) {
			return std::equal(
				suffix.rbegin(), suffix.rend(), text.rbegin(), ignore_case_cmp);
		}
		return std::equal(suffix.rbegin(), suffix.rend(), text.rbegin());
	}

	inline bool contains(
		const std::string_view& haystack, const std::string_view& needle,
		bool ignoreCase = false) {
		if (ignoreCase) {
			return std::search(
					   haystack.begin(), haystack.end(), needle.begin(),
					   needle.end(), ignore_case_cmp) != haystack.end();
		}
		return haystack.find(needle) != haystack.npos;
	}

	inline std::string_view strip_leading(std::string_view str) {
		while (!str.empty() && std::isspace(str.front())) {
			str.remove_prefix(1);
		}
		return str;
	}

	inline std::string_view strip_trialing(std::string_view str) {
		while (!str.empty() && std::isspace(str.back())) {
			str.remove_suffix(1);
		}
		return str;
	}

	inline std::string_view strip_prefix(
		std::string_view str, std::string_view prefix) {
		if (starts_with(str, prefix)) { str.remove_prefix(prefix.size()); }
		return str;
	}

	inline std::string_view strip_suffix(
		std::string_view str, std::string_view suffix) {
		if (ends_with(str, suffix)) { str.remove_suffix(suffix.size()); }
		return str;
	}

	inline std::string_view strip(std::string_view str) {
		return strip_leading(strip_trialing(str));
	}

	inline bool stoi(std::string_view str, unsigned int& v, int base = 10) {
		if (base == 16 && str::starts_with(str, "0x", true)) {
			str.remove_prefix(2);
		}
		const auto& end = str.data() + str.size();
		auto [ptr, ec] = std::from_chars(str.data(), end, v, base);
		return ptr == end && ec == std::errc();
	}

	inline bool stod(std::string_view str, double& v) {
		v = std::numeric_limits<double>::infinity();
		v = std::stod(std::string(str), nullptr);
		return true;
		// const auto& end = str.data() + str.size();
		// auto [ptr, ec] = std::from_chars(str.data(), end, v);
		// return ptr == end && ec == std::errc();
	}

	inline std::string tolower(std::string_view str) {
		std::string result(str);
		for (auto& ch : result) { ch = std::tolower(ch); }
		return result;
	}

	inline bool equals(
		std::string_view a, std::string_view b, bool ignoreCase = false) {
		if (ignoreCase) {
			return std::equal(
				a.begin(), a.end(), b.begin(), b.end(), ignore_case_cmp);
		}
		return std::equal(a.begin(), a.end(), b.begin(), b.end());
	}

	bool match(
		std::string_view txt, const std::regex& re,
		std::initializer_list<std::reference_wrapper<std::string_view>> dest);

	std::vector<std::string_view> split(std::string_view str, char splitter);
};	// namespace str
