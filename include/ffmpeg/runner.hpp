#include <absl/strings/string_view.h>

#include <filesystem>
#include <functional>
#include <string_view>

using LineScannerCallback = std::function<bool(absl::string_view line)>;

class Runner {
	std::filesystem::path path;

   public:
	Runner(std::filesystem::path p) : path(p) {}
	int lineScanner(
		std::vector<std::string> args, LineScannerCallback cb) const;
};
