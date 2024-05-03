#include "ffmpeg/profile.hpp"

#include <spdlog/spdlog.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>

#include "ffmpeg/filter.hpp"
#include "string_utils.hpp"
#include "util.hpp"

NLOHMANN_JSON_SERIALIZE_ENUM(
	SocketType, {
					{Video, "video"},
					{Audio, "audio"},
				});

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Socket, name, type);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AllowedValues, value, desc);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
	Option, name, desc, type, defaultValue, min, max, allowed);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
	Filter, name, desc, input, output, options, dynamicInput, dynamicOutput);

namespace {

	bool readWords(std::string_view text, std::string_view& dest) {
		text = str::strip_leading(text);
		auto it = std::find_if(text.begin(), text.end(), str::isspace);
		if (it == text.begin()) { return false; }
		auto pos = static_cast<size_t>(it - text.begin());
		dest = text.substr(0, pos);
		return true;
	}
	template <typename... Args>
	bool readWords(
		std::string_view text, std::string_view& dest, Args&... rest) {
		text = str::strip_leading(text);
		return readWords(text, dest) &&
			   readWords(text.substr(dest.size()), rest...);
	}

	int getIndent(std::string_view text) {
		auto itr = std::find_if_not(text.begin(), text.end(), str::isspace);
		int indent = static_cast<int>(std::distance(text.begin(), itr));
		if (itr == text.end()) { indent = 0; }
		return indent;
	}

	bool isTimelineText(std::string_view str) {
		return str ==
			   "This filter has support for timeline through the 'enable' "
			   "option.";
	}
	bool isSliceThreadingText(std::string_view str) {
		return str == "slice threading supported";
	}
	bool isInput(std::string_view str) { return str == "Inputs:"; }
	bool isOutput(std::string_view str) { return str == "Outputs:"; }
	bool isNoneSocket(std::string_view str) {
		return str == "none (source filter)" || str == "none (sink filter)";
	}
	bool isDynamicSocket(std::string_view str) {
		return str == "dynamic (depending on the options)";
	}

	bool isOption(std::string_view str) {
		return str::ends_with(str, "AVOptions:");
	}

	void splitOption(
		std::string_view text, std::string_view& name, std::string_view& type,
		std::string_view& flag, std::string_view& desc) {
		std::vector<std::string_view> parts;
		std::string_view str;

		for (int i = 0; i < 3; ++i) {
			if (readWords(text, str)) {
				parts.push_back(str);
				text = str::strip_leading(text);
				text.remove_prefix(str.size());
			} else {
				break;
			}
		}
		if (!text.empty()) { parts.push_back(str::strip_leading(text)); }
		// auto parts = std::vector<absl::string_view>(
		// 	absl::StrSplit(text, absl::MaxSplits(' ', 4)));
		name = parts[0];
		if (parts.size() == 2) {  // name and flag
			flag = parts[1];
		} else if (parts.size() == 3) {	 // name, type and flag
			type = parts[1];
			flag = parts[2];
		} else if (parts.size() == 4) {
			type = parts[1];
			flag = parts[2];
			desc = parts[3];
		}
	}
}  // namespace

class StateStack {
	std::vector<int> indents;
	std::vector<std::string> texts;

	std::optional<std::pair<std::string, int>> lastElement;

   public:
	void clear() {
		indents.clear();
		texts.clear();
		lastElement.reset();
	}
	bool empty() { return indents.empty(); }
	std::string_view pop(std::string_view text) {
		if (lastElement.has_value()) {
			texts.push_back(lastElement->first);
			indents.push_back(lastElement->second);
			lastElement.reset();
		}

		auto indent = getIndent(text);
		text = str::strip(text);

		lastElement.emplace(std::string(text), indent);

		while (!indents.empty()) {
			if (indents.back() < indent) { return text; }
			indents.pop_back();
			texts.pop_back();
		}
		return text;
	}
	int checkParent(std::function<bool(std::string_view line)> f) {
		int i = 0;
		for (auto itr = texts.rbegin(); itr != texts.rend(); itr++, i++) {
			if (f(*itr)) { return i; }
		}
		return -1;
	}
};

static const std::regex SOCKET_REGEX(R"(#\d+: (\w+) \((\w+)\))");
static const std::regex OPTION_DEFAULT_REGEX(R"((.*)\(default (.+)\))");
static const std::regex OPTION_RANGE_REGEX(R"((.*)\(from (.+) to (.+)\))");

class FilterParser {
	Filter filter;
	StateStack stack;

	bool isFilterHeader(std::string_view str) {
		return str::starts_with(str, "Filter ") &&
			   str::ends_with(str, filter.name);
	}

	bool parseSocket(std::string_view text) {
		std::string_view name, type;
		if (isNoneSocket(text)) { return true; }
		if (isDynamicSocket(text)) {
			if (stack.checkParent(isInput) == 0) { filter.dynamicInput = true; }
			if (stack.checkParent(isOutput) == 0) {
				filter.dynamicOutput = true;
			}
			return true;
		} else if (str::match(text, SOCKET_REGEX, {name, type})) {
		} else {
			return false;
		}
		Socket skt;
		skt.name = std::string(name);
		if (type == "video") {
			skt.type = SocketType::Video;
		} else if (type == "audio") {
			skt.type = SocketType::Audio;
		} else {
			return false;
		}
		if (stack.checkParent(isInput) == 0) { filter.input.push_back(skt); }
		if (stack.checkParent(isOutput) == 0) { filter.output.push_back(skt); }
		return true;
	}

	bool parseOption(std::string_view text) {
		auto isSubOption = stack.checkParent(isOption) == 1;
		std::string_view name, type, flag, desc;
		splitOption(text, name, type, flag, desc);
		if (flag == "") {
			//
			splitOption(text, name, type, flag, desc);
			return false;
		}
		if (isSubOption) {
			filter.options.back().allowed.push_back(
				AllowedValues{std::string(desc), std::string(name)});
			return true;
		}
		std::string_view a, b;
		Option opt{
			std::string(name), "",
			std::string(str::strip_suffix(str::strip_prefix(type, "<"), ">"))};
		if (str::match(desc, OPTION_DEFAULT_REGEX, {a, b})) {
			desc = a;
			if (b.front() == '"' && b.back() == '"') {
				b.remove_prefix(1);
				b.remove_suffix(1);
			}
			opt.defaultValue = std::string(b);
		}
		if (str::match(desc, OPTION_RANGE_REGEX, {desc, a, b})) {
			opt.min = std::string(a);
			opt.max = std::string(b);
		}
		opt.desc = std::string(str::strip(desc));
		filter.options.push_back(opt);
		return true;
	}

	bool processLine(std::string_view text) {
		text = stack.pop(text);

		auto filterHeaderParent =
			stack.checkParent([this](auto x) { return isFilterHeader(x); });

		if (text.empty()) {
		} else if (stack.empty() && isTimelineText(text)) {
			filter.options.push_back(Option{
				"enable", "The filter is enabled if evaluation is non-zero.",
				"boolean"});
		} else if (stack.empty() && isFilterHeader(text)) {
		} else if (filterHeaderParent == 0) {
			filter.desc = std::string(text);
		} else if (filterHeaderParent == 1 && isSliceThreadingText(text)) {
			// nothing to do yet
		} else if (filterHeaderParent == 1 && isInput(text)) {
		} else if (filterHeaderParent == 1 && isOutput(text)) {
		} else if (
			filterHeaderParent == 2 && (stack.checkParent(isInput) == 0 ||
										stack.checkParent(isOutput) == 0)) {
			return parseSocket(text);
		} else if (stack.empty() && isOption(text)) {
		} else if (
			stack.checkParent(isOption) == 0 ||
			stack.checkParent(isOption) == 1) {
			parseOption(text);
		} else {
			return false;
		}
		return true;
	}

   public:
	std::optional<Filter> parseFilter(
		const Runner& runner, std::string_view text) {
		filter = Filter();
		stack.clear();

		if (!str::starts_with(text, " ") || str::starts_with(text, "  ")) {
			return {};
		}

		std::string_view flags, name;
		if (!readWords(text, flags, name)) { return {}; }

		filter.name = std::string(name);

		runner.lineScanner({"--help", "filter=" + filter.name}, [this](auto x) {
			return processLine(x);
		});

		return filter;
	}
};

const std::vector<Option> InputNodeOptions = {
	{"filename", "path to input", "string"},
};
const std::vector<Option> OutputNodeOptions = {
	{"filename", "path to output", "string"},
};

Profile GetProfile() {
	Runner runner;
	Profile profile(runner);

	try {
		auto json = nlohmann::json::parse(std::ifstream("filters.json"));
		profile.filters = json.template get<std::vector<Filter>>();
	} catch (nlohmann::json::exception& e) {
		SPDLOG_ERROR("parse error: {}", e.what());
		FilterParser p;
		runner.lineScanner(
			{"-filters"}, [&runner, &p, &profile](std::string_view line) {
				auto f = p.parseFilter(runner, line);
				if (f.has_value()) {
					std::set<std::string> optNames;
					for (auto itr = f->options.begin();
						 itr != f->options.end();) {
						if (contains(optNames, itr->name)) {
							itr = f->options.erase(itr);
						} else {
							optNames.insert(itr->name);
							++itr;
						}
					}
					profile.filters.push_back(f.value());
				}
				return true;
			});

		std::sort(
			profile.filters.begin(), profile.filters.end(),
			[](const auto& a, const auto& b) { return a.name < b.name; });

		nlohmann::json json = profile.filters;
		std::ofstream o("filters.json", std::ios_base::binary);
		o << std::setw(4) << json;
	}

	profile.filters.push_back(
		{INPUT_FILTER_NAME,
		 "Load from path",
		 {},
		 {},
		 InputNodeOptions,
		 false,
		 true});
	profile.filters.push_back(
		{OUTPUT_FILTER_NAME, "Write to path", {}, {}, OutputNodeOptions, true});

	return profile;
}
