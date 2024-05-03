#pragma once

#include <filesystem>
#include <functional>
#include <vector>

#include "pref.hpp"

using LineScannerCallback = std::function<bool(std::string_view line)>;

class Runner {
	std::filesystem::path path;

   public:
	Runner() : path("ffmpeg") {}
	Runner(std::filesystem::path p) : path(p) {}
	int lineScanner(
		std::vector<std::string> args, LineScannerCallback cb,
		bool readStdErr = false) const;

	std::pair<int, std::string> play(
		const Preference& pref, const std::vector<std::string>& inputs,
		std::string_view filter, const std::vector<std::string>& outputs) const;
};
