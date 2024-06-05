#include "ffmpeg/runner.hpp"

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#if __has_include(<unistd.h>)
#include <unistd.h>
#endif

#include <reproc/reproc.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <reproc++/run.hpp>
#include <vector>

#include "string_utils.hpp"
#include "util.hpp"

using namespace std::chrono_literals;

namespace {
	auto reader(
		std::string& incompleteLine, const LineScannerCallback& cb,
		const uint8_t* bytes, size_t n) {
		auto data = std::string_view(reinterpret_cast<const char*>(bytes), n);
		for (auto idx = data.find_first_of('\n'); idx != std::string_view::npos;
			 idx = data.find_first_of('\n')) {
			auto line = data.substr(0, idx);
			data.remove_prefix(idx + 1);
			if (incompleteLine.size() > 0) {
				incompleteLine.append(line.begin(), line.end());
				line = std::string_view(incompleteLine);
			}
			if (!cb(line)) {
				return std::make_error_code(reproc::error::interrupted);
			}
			incompleteLine.clear();
		}
		if (data.size() > 0) {
			incompleteLine.append(data.begin(), data.end());
		}
		return std::error_code();
	}

	const int PID = getpid();
	std::atomic_int filename_index = 0;

}  // namespace
int Runner::lineScanner(
	std::vector<std::string> args, LineScannerCallback cb,
	bool readStdErr) const {
	args.insert(args.begin(), path.string());
	std::string incompleteLine;

	reproc::options options;
	if (cb == nullptr) {
		options.redirect.discard = true;
		options.timeout = 1s;

		return reproc::run(args, options).first;
	}

	SPDLOG_INFO("args: {}", args);

	auto readLambda = [&](auto stream, auto bytes, auto n) {
		if (readStdErr && stream != reproc::stream::err) {
			return std::error_code();
		}
		if (!readStdErr && stream != reproc::stream::out) {
			return std::error_code();
		}
		return reader(incompleteLine, cb, bytes, n);
	};

	options.redirect.err.type = reproc::redirect::pipe;
	auto status = reproc::run(args, options, readLambda, readLambda);
	if (incompleteLine.size() > 0) { cb(incompleteLine); }
	return status.first;
}

auto isRunning(reproc::process& process, int& status) {
	auto val = process.wait(reproc::milliseconds(0));
	if (val.first == REPROC_ETIMEDOUT) { return true; }
	status = val.first;
	return false;
}

auto failed(reproc::process& process, int& status) {
	if (isRunning(process, status)) { return false; }
	return status != 0;
}

std::pair<int, std::string> Runner::play(
	const std::vector<std::string>& inputs, std::string_view filter,
	const std::vector<std::string>& outputs, const std::string& player) const {
	namespace fs = std::filesystem;

	const auto tempPath =
		fs::temp_directory_path() / "ffmpeg_node_editor" /
		fmt::format("temp{}.mkv", PID * 1000 + (++filename_index));

	fs::create_directories(tempPath.parent_path());

	defer tempfileDefer([&]() {
		std::error_code err;
		fs::remove(tempPath, err);
	});

	std::vector<std::string> args{path.string(), "-hide_banner"};
	if (inputs.empty()) {
		args.insert(args.end(), {"-f", "lavfi", "-i", "nullsrc"});
	} else {
		for (const auto& i : inputs) { args.insert(args.end(), {"-i", i}); }
	}
	if (!filter.empty()) {
		args.emplace_back("-filter_complex");
		args.emplace_back(filter.data());
	}

	for (const auto& o : outputs) { args.insert(args.end(), {"-map", o}); }
	if (inputs.empty()) {
		// If input is empty add a hard limit of 5 min for now
		args.insert(args.end(), {"-y", "-t", "300", tempPath.string()});
	} else {
		args.insert(args.end(), {"-y", tempPath.string()});
	}

	reproc::options options;
	options.redirect.err.type = reproc::redirect::pipe;

	// 1. Start the ffmpeg process
	reproc::process ffmpeg;
	SPDLOG_DEBUG("ffmpeg args: \"{}\"", fmt::join(args, " "));

	int ffmpeg_status = 0;
	auto ffmpeg_err = ffmpeg.start(args, options);
	if (ffmpeg_err) { return {ffmpeg_err.value(), ffmpeg_err.message()}; }

	std::string output;
	reproc::sink::string sink(output);

	// 2. Drain stdout and stderr in another thread
	SPDLOG_DEBUG("async");
	auto f = std::async([&]() {
		SPDLOG_DEBUG("drain started");
		ffmpeg_err = reproc::drain(ffmpeg, sink, sink);
		SPDLOG_DEBUG("drain ended");
	});
	// 3. We have to wait until ffmpeg writes something to the file
	while (!fs::exists(tempPath) || fs::file_size(tempPath) == 0) {
		SPDLOG_DEBUG(
			"Checking for file size: {}",
			fs::exists(tempPath) && fs::file_size(tempPath));
		if (!isRunning(ffmpeg, ffmpeg_status)) { break; }
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	if (failed(ffmpeg, ffmpeg_status)) { return {ffmpeg_status, output}; }

	auto tempString = tempPath.string();

	std::vector<std::string_view> player_args;
	for (auto& elem : str::split(player, '\n')) {
		if (elem == "%f") {
			player_args.emplace_back(tempString);
		} else {
			player_args.emplace_back(str::strip(elem));
		}
	}

	SPDLOG_DEBUG("player args: {}", player_args);
	auto player_response = reproc::run(player_args);

	reproc::stop_actions stop{
		{reproc::stop::noop, 0ms},
		{reproc::stop::terminate, 500ms},
		{reproc::stop::kill, 200ms}};
	ffmpeg.stop(stop);

	return {player_response.second.value(), player_response.second.message()};
}
