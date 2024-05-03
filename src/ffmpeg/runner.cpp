#include "ffmpeg/runner.hpp"

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <process.hpp>
#include <thread>
#include <vector>

#include "pref.hpp"
#include "string_utils.hpp"
namespace {
	void reader(
		TinyProcessLib::Process* cmd, bool& running,
		std::string& incompleteLine, LineScannerCallback cb, const char* bytes,
		size_t n) {
		auto data = std::string_view(bytes, n);
		for (auto idx = data.find_first_of('\n'); idx != std::string_view::npos;
			 idx = data.find_first_of('\n')) {
			auto line = data.substr(0, idx);
			data.remove_prefix(idx + 1);
			if (incompleteLine.size() > 0) {
				incompleteLine.append(line.begin(), line.end());
				line = std::string_view(incompleteLine);
			}
			if (running) {
				if (!cb(line)) {
					running = false;
					cmd->kill();
					return;
				}
			}
			incompleteLine.clear();
		}
		if (data.size() > 0) {
			incompleteLine.append(data.begin(), data.end());
		}
	}

	const int PID = getpid();
	std::atomic_int filename_index = 0;

}  // namespace
int Runner::lineScanner(
	std::vector<std::string> args, LineScannerCallback cb,
	bool readStdErr) const {
	args.insert(args.begin(), path.string());
	std::string incompleteLine;
	bool running = true;

	TinyProcessLib::Process* cmd;
	auto readLambda = [cb, &cmd, &running, &incompleteLine](
						  const char* bytes, size_t n) {
		return reader(cmd, running, incompleteLine, cb, bytes, n);
	};
	if (!readStdErr) {
		cmd = new TinyProcessLib::Process(args, "", readLambda);
	} else {
		cmd = new TinyProcessLib::Process(args, "", nullptr, readLambda);
	}
	if (incompleteLine.size() > 0) { cb(incompleteLine); }
	auto status = cmd->get_exit_status();
	delete cmd;
	return status;
}

std::pair<int, std::string> Runner::play(
	const Preference& pref, const std::vector<std::string>& inputs,
	std::string_view filter, const std::vector<std::string>& outputs) const {
	const auto tempPath =
		std::filesystem::temp_directory_path() / "ffmpeg_node_editor" /
		fmt::format("temp{}.mkv", PID * 1000 + (++filename_index));

	std::filesystem::create_directories(tempPath.parent_path());
	std::ofstream tempfile(tempPath, std::ios::binary);

	std::vector<std::string> args{path.string(), "-hide_banner"};
	if (inputs.empty()) {
		args.insert(args.end(), {"-f", "lavfi", "-i", "nullsrc"});
	} else {
		for (auto& i : inputs) {
			auto itr = std::find_if(i.begin(), i.end(), [](auto c) {
				return std::isspace(static_cast<unsigned char>(c));
			});
			if (itr != i.end()) {
				args.insert(args.end(), {"-i", '"' + i + '"'});
			} else {
				args.insert(args.end(), {"-i", i});
			}
		}
	}
	if (!filter.empty()) {
		args.push_back("-filter_complex");
		args.push_back(filter.data());
	}

	for (auto& o : outputs) { args.insert(args.end(), {"-map", o}); }
	args.insert(args.end(), {"-f", "matroska", "-"});

	fmt::memory_buffer std_err;

	TinyProcessLib::Process ffmpeg(
		args, "",
		[&](const char* bytes, size_t n) { tempfile.write(bytes, n); },
		[&](const char* bytes, size_t n) { std_err.append(bytes, bytes + n); },
		true);

	auto ffmpeg_status = 0, player_status = 0;

	while (!std::filesystem::exists(tempPath) ||
		   std::filesystem::file_size(tempPath) == 0) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		if (ffmpeg.try_get_exit_status(ffmpeg_status)) { break; }
	}

	if (!ffmpeg.try_get_exit_status(ffmpeg_status)) {
		std::vector<std::string> player_args;
		for (auto& elem : str::split(pref.player, '\n')) {
			if (elem == "%f") {
				player_args.emplace_back(tempPath.string());
			} else {
				player_args.emplace_back(str::strip(elem));
			}
		}
		TinyProcessLib::Process player(player_args);
		auto f = std::async([&]() {
			SPDLOG_DEBUG("waiting for player process");
			player_status = player.get_exit_status();
			SPDLOG_DEBUG("player process exited with status {}", player_status);
			ffmpeg.write("q");
			SPDLOG_DEBUG("Asked ffmpeg process to quit");
		});
	}

	SPDLOG_DEBUG("waiting for ffmpeg process");
	ffmpeg_status = ffmpeg.get_exit_status();
	SPDLOG_DEBUG("ffmpeg process exited with status {}", ffmpeg_status);
	if (ffmpeg_status != 0) {
		SPDLOG_ERROR("ffmpeg stderr: {}", fmt::to_string(std_err));
		SPDLOG_ERROR("ffmpeg args: {}", fmt::join(args, " "));
	}

	tempfile.flush();
	tempfile.close();
	std::filesystem::remove(tempPath);
	return {ffmpeg_status, fmt::to_string(std_err)};
}
