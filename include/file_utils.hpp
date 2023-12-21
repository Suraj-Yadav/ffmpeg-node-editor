#include <nfd.h>

#include <filesystem>
#include <optional>
#include <stdexcept>

inline std::optional<std::filesystem::path> openFile() {
	nfdchar_t* outPath = NULL;
	nfdresult_t result = NFD_OpenDialog(NULL, NULL, &outPath);
	if (result == NFD_OKAY) {
		std::filesystem::path v(outPath);
		free(outPath);
		return v;
	} else if (result == NFD_CANCEL) {
		return {};
	} else {
		throw std::runtime_error(NFD_GetError());
	}
	return {};
}
