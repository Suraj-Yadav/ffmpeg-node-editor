#pragma once

#include <filesystem>
#include <functional>
#include <utility>
#include <vector>

using LineScannerCallback = std::function<bool(std::string_view line)>;

struct Stream {
	int index;
	std::string name;
	std::string type;
};

struct MediaInfo {
	std::vector<Stream> streams;
};

class Runner {
	std::filesystem::path path;

   public:
	Runner() : path("ffmpeg") {}
	Runner(std::filesystem::path p) : path(std::move(p)) {}
	[[nodiscard]] int lineScanner(
		std::vector<std::string> args, const LineScannerCallback& cb,
		bool readStdErr = false) const;

	[[nodiscard]] std::pair<int, std::string> play(
		const std::vector<std::string>& inputs, std::string_view filter,
		const std::vector<std::string>& outputs,
		const std::string& player) const;

	[[nodiscard]] MediaInfo getInfo(const std::filesystem::path& p) const;
};
