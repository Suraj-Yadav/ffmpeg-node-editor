#pragma once

#include <absl/strings/string_view.h>

#include <filesystem>
#include <functional>
#include <string_view>
#include <vector>

using LineScannerCallback = std::function<bool(absl::string_view line)>;

class Runner {
	std::filesystem::path path;

   public:
	Runner(std::filesystem::path p) : path(p) {}
	int lineScanner(
		std::vector<std::string> args, LineScannerCallback cb) const;

	std::pair<int, std::string> play(
		absl::string_view filter,
		const std::vector<std::string>& outputs) const;
};
