#include "ffmpeg/runner.hpp"

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <subprocess.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <vector>

#include "string_utils.hpp"
#include "util.hpp"

using namespace std::chrono_literals;

namespace {
	const int PID = getpid();
	std::atomic_int filename_index = 0;

	auto convertArgs(const std::vector<std::string>& args) {
		std::vector<const char*> result;
		result.reserve(args.size() + 1);
		for (const auto& e : args) { result.push_back(e.data()); }
		result.push_back(nullptr);
		return result;
	}

}  // namespace

class Process {
	subprocess_s process;
	int status;
	static constexpr auto BUFFER_SIZE = 4096u;

	static std::string readStream(std::FILE* file) {
		std::string buffer(BUFFER_SIZE, '\0'), str;
		for (; std::fgets(buffer.data(), buffer.size(), file) != nullptr;) {
			str += buffer.substr(0, buffer.find_first_of('\0'));
			std::ranges::fill(buffer, '\0');
		}
		return str;
	}

   public:
	Process() = default;
	Process(const Process&) = delete;
	Process(Process&&) = delete;
	Process& operator=(Process&&) = delete;
	Process& operator=(const Process&) = delete;

	~Process() {
		if (isRunning()) { subprocess_terminate(&process); }
		finish();
		subprocess_destroy(&process);
	}

	bool start(const std::vector<std::string>& args, int options) {
		auto aargs = convertArgs(args);
		status = subprocess_create(aargs.data(), options, &process);
		return status == 0;
	}

	bool isRunning() { return subprocess_alive(&process) != 0; }
	bool failed() {
		if (isRunning()) { return false; }
		finish();
		return status != 0;
	}

	void finish() { subprocess_join(&process, &status); }
	auto getStdErr() { return readStream(subprocess_stderr(&process)); }
	auto getStdOut() { return readStream(subprocess_stdout(&process)); }
	[[nodiscard]] auto returnCode() const { return status; }

	void lineReader(const LineScannerCallback& cb, bool readStdErr) {
		auto reader = subprocess_read_stdout;
		if (readStdErr) { reader = subprocess_read_stderr; }

		std::string incompleteLine, buffer(BUFFER_SIZE, '\0');
		for (auto read = reader(&process, buffer.data(), buffer.size());
			 read > 0; read = reader(&process, buffer.data(), buffer.size())) {
			auto data = std::string_view(buffer);
			for (auto idx = data.find_first_of('\n');
				 idx != std::string_view::npos;
				 idx = data.find_first_of('\n')) {
				incompleteLine += data.substr(0, idx);
				if (!cb(incompleteLine)) { return; }
			}
		}
	}
};

int Runner::lineScanner(
	std::vector<std::string> args, const LineScannerCallback& cb,
	bool readStdErr) const {
	Process process{};

	args.insert(args.begin(), path.string());

	if (!process.start(
			args, subprocess_option_enable_async |
					  subprocess_option_search_user_path)) {
		return process.returnCode();
	}

	if (cb == nullptr) {
		process.finish();
		return process.returnCode();
	}

	process.lineReader(cb, readStdErr);

	process.finish();

	return process.returnCode();
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

	std::vector<std::string> args{path.string(), "-hide_banner", "-v", "error"};
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

	auto aargs = convertArgs(args);

	Process ffmpeg_process;

	SPDLOG_DEBUG("ffmpeg start: \"{}\"", fmt::join(args, " "));
	// 1. Start the ffmpeg process
	if (!ffmpeg_process.start(args, subprocess_option_search_user_path)) {
		return {ffmpeg_process.returnCode(), ffmpeg_process.getStdErr()};
	}

	// 2. We have to wait until ffmpeg writes something to the file
	while (!fs::exists(tempPath) || fs::file_size(tempPath) == 0) {
		SPDLOG_DEBUG(
			"Checking for file size: {}",
			fs::exists(tempPath) && fs::file_size(tempPath));
		if (!ffmpeg_process.isRunning()) { break; }
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	if (ffmpeg_process.failed()) {
		return {
			ffmpeg_process.returnCode(),
			"ffmpeg error: " + ffmpeg_process.getStdErr()};
	}

	SPDLOG_DEBUG(
		"Found files {} with size {}", tempPath.string(),
		fs::file_size(tempPath));

#if defined(APP_OS_WINDOWS)
	std::vector<std::string_view> player_args{"cmd", "/C", "start"};
#else
	std::vector<std::string> player_args{};
#endif
	for (auto& elem : str::split(player, '\n')) {
		if (elem == "%f") {
			player_args.emplace_back(tempPath.string());
		} else {
			player_args.emplace_back(str::strip(elem));
		}
	}

	auto player_aargs = convertArgs(player_args);

	SPDLOG_DEBUG("player args: {}", player_args);

	Process player_process{};

	if (!player_process.start(
			player_args, subprocess_option_search_user_path |
							 subprocess_option_inherit_environment)) {
		return {player_process.returnCode(), player_process.getStdErr()};
	}

	player_process.finish();

	return {player_process.returnCode(), player_process.getStdErr()};
}

bool try_ffprobe(MediaInfo& info, const std::filesystem::path& p) {
	std::vector<std::string> args{"ffprobe",	   "-v",
								  "quiet",		   "-print_format",
								  "json",		   "-show_format",
								  "-show_streams", "-print_format",
								  "json",		   p};
	SPDLOG_DEBUG("ffprobe args: \"{}\"", fmt::join(args, " "));

	Process ffprobe{};

	if (!ffprobe.start(args, subprocess_option_search_user_path)) {
		return false;
	}

	ffprobe.finish();

	auto output = ffprobe.getStdOut();
	SPDLOG_DEBUG("ffprobe output:\n{}", output);

	nlohmann::json json;
	try {
		json = nlohmann::json::parse(output);
	} catch (nlohmann::json::exception&) { return false; }
	auto& streams = json["streams"];
	if (streams.is_null()) { return {}; }
	int index = 0;
	for (auto& elem : streams) {
		info.streams.emplace_back();
		info.streams.back().index = index++;
		auto& type = elem["codec_type"];
		if (type.is_string()) {
			info.streams.back().type = type.get<std::string>();
		}
		if (!elem["tags"].is_null() && elem["tags"]["title"].is_string()) {
			info.streams.back().name = elem["tags"]["title"].get<std::string>();
		}
	}

	return true;
}

MediaInfo Runner::getInfo(const std::filesystem::path& p) const {
	MediaInfo info;
	if (p.empty()) { return info; }
	static bool can_try_ffprobe = true;
	if (can_try_ffprobe) {
		if (try_ffprobe(info, p)) { return info; }
		can_try_ffprobe = false;
	}
	info.streams.clear();

	bool inputStarted = false;
	(void)lineScanner(
		{"-i", p},
		[&](std::string_view line) {
			if (!inputStarted) {
				inputStarted = str::starts_with(line, "Input #0");
				return true;
			}
			int index = 0;
			if (str::starts_with(line, "  Stream #0")) {
				if (str::contains(line, "Video:")) {
					info.streams.push_back(Stream{index++, "", "video"});
				} else if (str::contains(line, "Audio:")) {
					info.streams.push_back(Stream{index++, "", "audio"});
				} else if (str::contains(line, "Subtitle:")) {
					info.streams.push_back(Stream{index++, "", "subtitle"});
				} else {
					info.streams.push_back(Stream{index++, "", "video"});
				}
			}
			return true;
		},
		true);
	return info;
}
