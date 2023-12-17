#include "ffmpeg/runner.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <filesystem>
#include <future>
#include <process.hpp>
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
	args.insert(
		args.end(), {"-c", "copy", "-movflags", "frag_keyframe+faststart",
					 tempfile.string()});

	fmt::memory_buffer std_err;

	TinyProcessLib::Process process1(
		args, "", nullptr,
		[&](const char* bytes, size_t n) { std_err.append(bytes, bytes + n); },
		true);

	TinyProcessLib::Process process2(
		std::vector<std::string>{"mpvnet", tempfile.string()});

	auto status1 = 0, status2 = 0;
	auto f = std::async([&]() {
		SPDLOG_INFO("waiting for player process");
		status2 = process2.get_exit_status();
		SPDLOG_INFO("player process exited with status {}", status2);
		process1.write("q");
		SPDLOG_INFO("Asked ffmpeg process to quit");
	});
	SPDLOG_INFO("waiting for ffmpeg process");
	status1 = process1.get_exit_status();
	SPDLOG_INFO("ffmpeg process exited with status {}", status1);
	if (status1 != 0) {
		SPDLOG_ERROR("ffmpeg stderr: {}", fmt::to_string(std_err));
	}
	if (!process2.try_get_exit_status(status2)) {
		process2.close_stdin();
		if (status1 != 0) {
			process2.kill(true);
			SPDLOG_INFO("killed player process");
		}
	}
	SPDLOG_INFO("waiting for player process to finish");
	f.wait();
	SPDLOG_INFO("done waiting for player process");

	std::filesystem::remove(tempfile);
	return {status1, fmt::to_string(std_err)};
}
