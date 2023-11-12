#include "ffmpeg/runner.hpp"

#include <absl/strings/ascii.h>
#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <array>
#include <future>
#include <iostream>
#include <iterator>
#include <process.hpp>
#include <string>

int Runner::lineScanner(
	std::vector<std::string> args, LineScannerCallback cb) const {
	args.insert(args.begin(), path.string());
	std::string incompleteLine;
	bool running = true;
	TinyProcessLib::Process cmd(
		args, "",
		[cb, &cmd, &running, &incompleteLine](const char* bytes, size_t n) {
			auto data = absl::string_view(bytes, n);
			for (auto idx = data.find_first_of('\n');
				 idx != std::string_view::npos;
				 idx = data.find_first_of('\n')) {
				auto line = data.substr(0, idx);
				data.remove_prefix(idx + 1);
				if (incompleteLine.size() > 0) {
					incompleteLine.append(line.begin(), line.end());
					line = absl::string_view(incompleteLine);
				}
				if (running) {
					if (!cb(line)) {
						running = false;
						cmd.kill();
						return;
					}
				}
				incompleteLine.clear();
			}
			if (data.size() > 0) {
				incompleteLine.append(data.begin(), data.end());
			}
		});
	if (incompleteLine.size() > 0) { cb(incompleteLine); }
	return cmd.get_exit_status();
}

std::pair<int, std::string> Runner::play(
	absl::string_view filter, const std::vector<std::string>& outputs) const {
	TinyProcessLib::Process process2(
		std::vector<std::string>{"mpvnet", "-"}, "", nullptr, nullptr, true);
	std::vector<std::string> args{
		"ffmpeg",  "-hide_banner",	  "-f",			 "lavfi", "-i",
		"nullsrc", "-filter_complex", filter.data(),
	};
	for (auto& o : outputs) { args.insert(args.end(), {"-map", o}); }
	args.insert(args.end(), {"-f", "matroska", "-"});

	fmt::memory_buffer std_err;

	TinyProcessLib::Process process1(
		args, "",
		[&](const char* bytes, size_t n) { process2.write(bytes, n); },
		[&](const char* bytes, size_t n) { std_err.append(bytes, bytes + n); },
		true);

	auto status1 = 0, status2 = 0;
	auto f = std::async([&]() {
		spdlog::info("waiting for player process");
		status2 = process2.get_exit_status();
		spdlog::info("player process exited with status {}", status2);
		process1.write("q");
		spdlog::info("Asked ffmpeg process to quit");
	});
	spdlog::info("waiting for ffmpeg process");
	status1 = process1.get_exit_status();
	spdlog::info("ffmpeg process exited with status {}", status1);
	if (!process2.try_get_exit_status(status2)) {
		process2.close_stdin();
		if (status1 != 0) {
			process2.kill(true);
			spdlog::info("killed player process");
		}
	}
	spdlog::info("waiting for player process to finish");
	f.wait();
	spdlog::info("done waiting for player process");
	return {status1 + status2, fmt::to_string(std_err)};
}
