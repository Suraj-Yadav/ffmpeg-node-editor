#include "ffmpeg/runner.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <process.hpp>
#include <thread>
namespace {
	void reader(
		TinyProcessLib::Process* cmd, bool& running,
		std::string& incompleteLine, LineScannerCallback cb, const char* bytes,
		size_t n) {
		auto data = absl::string_view(bytes, n);
		for (auto idx = data.find_first_of('\n');
			 idx != absl::string_view::npos; idx = data.find_first_of('\n')) {
			auto line = data.substr(0, idx);
			data.remove_prefix(idx + 1);
			if (incompleteLine.size() > 0) {
				incompleteLine.append(line.begin(), line.end());
				line = absl::string_view(incompleteLine);
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

	const int PID = _getpid();
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
	const std::vector<std::string>& inputs, absl::string_view filter,
	const std::vector<std::string>& outputs) const {
	const auto tempfile =
		std::filesystem::temp_directory_path() / "ffmpeg_node_editor" /
		fmt::format("temp{}.mkv", PID * 1000 + (++filename_index));

	std::filesystem::create_directories(tempfile.parent_path());

	std::vector<std::string> args{"ffmpeg", "-hide_banner"};
	if (inputs.empty()) {
		args.insert(args.end(), {"-f", "lavfi", "-i", "nullsrc"});
	} else {
		for (auto& i : inputs) { args.insert(args.end(), {"-i", i}); }
	}
	if (!filter.empty()) {
		args.push_back("-filter_complex");
		args.push_back(filter.data());
	}

	for (auto& o : outputs) { args.insert(args.end(), {"-map", o}); }
	args.insert(args.end(), {tempfile.string()});

	fmt::memory_buffer std_err;

	TinyProcessLib::Process ffmpeg(
		args, "", nullptr,
		[&](const char* bytes, size_t n) { std_err.append(bytes, bytes + n); },
		true);

	auto ffmpeg_status = 0, player_status = 0;

	while (!std::filesystem::exists(tempfile) ||
		   std::filesystem::file_size(tempfile) == 0) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		if (ffmpeg.try_get_exit_status(ffmpeg_status)) { break; }
	}

	if (!ffmpeg.try_get_exit_status(ffmpeg_status)) {
		TinyProcessLib::Process player(
			std::vector<std::string>{"mpvnet", tempfile.string()});
		auto f = std::async([&]() {
			SPDLOG_INFO("waiting for player process");
			player_status = player.get_exit_status();
			SPDLOG_INFO("player process exited with status {}", player_status);
			ffmpeg.write("q");
			SPDLOG_INFO("Asked ffmpeg process to quit");
		});
	}

	SPDLOG_INFO("waiting for ffmpeg process");
	ffmpeg_status = ffmpeg.get_exit_status();
	SPDLOG_INFO("ffmpeg process exited with status {}", ffmpeg_status);
	if (ffmpeg_status != 0) {
		SPDLOG_ERROR("ffmpeg stderr: {}", fmt::to_string(std_err));
	}

	std::filesystem::remove(tempfile);
	return {ffmpeg_status, fmt::to_string(std_err)};
}
