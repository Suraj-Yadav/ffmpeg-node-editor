#include "ffmpeg/runner.hpp"

#include <absl/strings/ascii.h>
#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>

#include <iostream>
#include <process.hpp>

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
