#include "ffmpeg/profile.hpp"

#include <absl/strings/ascii.h>
#include <absl/strings/match.h>
#include <absl/strings/strip.h>
#include <re2/re2.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>

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

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Filter, name, desc, input, output, options);

namespace {

	bool readWords(absl::string_view text, absl::string_view& dest) {
		text = absl::StripLeadingAsciiWhitespace(text);
		auto it = std::find_if(text.begin(), text.end(), absl::ascii_isspace);
		if (it == text.begin()) { return false; }
		auto pos = static_cast<size_t>(it - text.begin());
		dest = text.substr(0, pos);
		return true;
	}
	template <typename... Args>
	bool readWords(
		absl::string_view text, absl::string_view& dest, Args&... rest) {
		text = absl::StripLeadingAsciiWhitespace(text);
		return readWords(text, dest) &&
			   readWords(text.substr(dest.size()), rest...);
	}

	int getIndent(absl::string_view text) {
		auto itr =
			std::find_if_not(text.begin(), text.end(), absl::ascii_isspace);
		int indent = static_cast<int>(std::distance(text.begin(), itr));
		if (itr == text.end()) { indent = 0; }
		return indent;
	}

	bool isTimelineText(absl::string_view str) {
		return str ==
			   "This filter has support for timeline through the 'enable' "
			   "option.";
	}
	bool isSliceThreadingText(absl::string_view str) {
		return str == "slice threading supported";
	}
	bool isInput(absl::string_view str) { return str == "Inputs:"; }
	bool isOutput(absl::string_view str) { return str == "Outputs:"; }
	bool isNoneSocket(absl::string_view str) {
		return str == "none (source filter)" || str == "none (sink filter)";
	}
	bool isDynamicSocket(absl::string_view str) {
		return str == "dynamic (depending on the options)";
	}

	bool isOption(absl::string_view str) {
		return absl::EndsWith(str, "AVOptions:");
	}

	void splitOption(
		absl::string_view text, absl::string_view& name,
		absl::string_view& type, absl::string_view& flag,
		absl::string_view& desc) {
		std::vector<absl::string_view> parts;
		absl::string_view str;

		for (int i = 0; i < 3; ++i) {
			if (readWords(text, str)) {
				parts.push_back(str);
				text = absl::StripLeadingAsciiWhitespace(text);
				text.remove_prefix(str.size());
			} else {
				break;
			}
		}
		if (!text.empty()) {
			parts.push_back(absl::StripLeadingAsciiWhitespace(text));
		}
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
	absl::string_view pop(absl::string_view text) {
		if (lastElement.has_value()) {
			texts.push_back(lastElement->first);
			indents.push_back(lastElement->second);
			lastElement.reset();
		}

		auto indent = getIndent(text);
		text = absl::StripAsciiWhitespace(text);

		lastElement.emplace(std::string(text), indent);

		while (!indents.empty()) {
			if (indents.back() < indent) { return text; }
			indents.pop_back();
			texts.pop_back();
		}
		return text;
	}
	int checkParent(std::function<bool(absl::string_view line)> f) {
		int i = 0;
		for (auto itr = texts.rbegin(); itr != texts.rend(); itr++, i++) {
			if (f(*itr)) { return i; }
		}
		return -1;
	}
};

static constexpr re2::LazyRE2 SOCKET_REGEX = {R"(#\d+: (\w+) \((\w+)\))"};
static constexpr re2::LazyRE2 OPTION_DEFAULT_REGEX = {
	R"((.*)\(default (.+)\))"};
static constexpr re2::LazyRE2 OPTION_RANGE_REGEX = {
	R"((.*)\(from (.+) to (.+)\))"};

class FilterParser {
	Filter filter;
	StateStack stack;

	bool isFilterHeader(absl::string_view str) {
		return absl::StartsWith(str, "Filter ") &&
			   absl::EndsWith(str, filter.name);
	}

	bool parseSocket(absl::string_view text) {
		absl::string_view name, type;
		if (isNoneSocket(text) || isDynamicSocket(text)) {
			return true;
		} else if (re2::RE2::PartialMatch(text, *SOCKET_REGEX, &name, &type)) {
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

	bool parseOption(absl::string_view text) {
		auto isSubOption = stack.checkParent(isOption) == 1;
		absl::string_view name, type, flag, desc;
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
		absl::string_view a, b;
		Option opt{
			std::string(name), "",
			std::string(absl::StripSuffix(absl::StripPrefix(type, "<"), ">"))};
		if (re2::RE2::PartialMatch(desc, *OPTION_DEFAULT_REGEX, &a, &b)) {
			desc = a;
			opt.defaultValue = std::string(b);
		}
		if (re2::RE2::PartialMatch(desc, *OPTION_RANGE_REGEX, &desc, &a, &b)) {
			opt.min = std::string(a);
			opt.max = std::string(b);
		}
		opt.desc = std::string(absl::StripAsciiWhitespace(desc));
		filter.options.push_back(opt);
		return true;
	}

	bool processLine(absl::string_view text) {
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
		const Runner& runner, absl::string_view text) {
		filter = Filter();
		stack.clear();

		if (!absl::StartsWith(text, " ") || absl::StartsWith(text, "  ")) {
			return {};
		}

		absl::string_view flags, name;
		if (!readWords(text, flags, name)) { return {}; }

		filter.name = std::string(name);

		runner.lineScanner({"--help", "filter=" + filter.name}, [this](auto x) {
			return processLine(x);
		});

		return filter;
	}
};

const std::vector<Option> InputNodeOptions = {
	{"path", "path to input", "string"},
};
const std::vector<Option> OutputNodeOptions = {
	{"path", "path to output", "string"},
};

Profile GetProfile() {
	Runner runner("C:\\Users\\suraj\\scoop\\shims\\ffmpeg.exe");
	Profile profile(runner);

	try {
		auto json = nlohmann::json::parse(std::ifstream("filters.json"));
		profile.filters = json.template get<std::vector<Filter>>();
	} catch (nlohmann::json::exception&) {
		FilterParser p;
		runner.lineScanner(
			{"-filters"}, [&runner, &p, &profile](absl::string_view line) {
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
