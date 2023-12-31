#pragma once

#include <absl/strings/string_view.h>

#include <filesystem>
#include <functional>
#include <vector>

using LineScannerCallback = std::function<bool(absl::string_view line)>;

class Runner {
	std::filesystem::path path;

   public:
	Runner() : path("ffmpeg") {}
	Runner(std::filesystem::path p) : path(p) {}
	int lineScanner(
		std::vector<std::string> args, LineScannerCallback cb,
		bool readStdErr = false) const;

	std::pair<int, std::string> play(
		const std::vector<std::string>& inputs, absl::string_view filter,
		const std::vector<std::string>& outputs) const;
};
