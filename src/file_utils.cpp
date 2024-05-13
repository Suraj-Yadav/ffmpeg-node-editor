#include "file_utils.hpp"

#include <tinyfiledialogs/tinyfiledialogs.h>

#include <filesystem>
#include <string>

std::optional<std::filesystem::path> openFile() {
	auto* selection =
		tinyfd_openFileDialog("Open File", nullptr, 0, nullptr, nullptr, 0);
	if (selection == nullptr) { return {}; }
	return selection;
}
std::optional<std::filesystem::path> saveFile() {
	auto* selection =
		tinyfd_saveFileDialog("Save File", nullptr, 0, nullptr, nullptr);
	if (selection == nullptr) { return {}; }
	return selection;
}

void showErrorMessage(std::string const& title, std::string const& text) {
	std::string title_copy = title, text_copy = text;
	std::replace(title_copy.begin(), title_copy.end(), '\'', '`');
	std::replace(title_copy.begin(), title_copy.end(), '"', '`');
	std::replace(text_copy.begin(), text_copy.end(), '\'', '`');
	std::replace(text_copy.begin(), text_copy.end(), '"', '`');
	tinyfd_messageBox(title_copy.c_str(), text_copy.c_str(), "ok", "error", 0);
}

#ifdef _WIN32
#include <Windows.h>
#include <wingdi.h>
#include <winnt.h>
#include <winreg.h>

std::string GetRegString(
	HKEY key, std::string_view subKey, std::string_view valueName) {
	DWORD keyType = 0;
	DWORD dataSize = 0;
	const DWORD flags = RRF_RT_REG_SZ;	// Only read strings (REG_SZ)
	auto result = ::RegGetValue(
		key, subKey.data(), valueName.data(), flags, &keyType, nullptr,
		&dataSize);
	if (result != ERROR_SUCCESS) { return ""; }

	if (keyType != REG_SZ) { return ""; }

	std::string text(dataSize, '\0');

	result = ::RegGetValue(
		key, subKey.data(), valueName.data(), flags, nullptr,
		text.data(),  // Write string in this destination buffer
		&dataSize);
	if (result != ERROR_SUCCESS) { return ""; }

	// Remove \0 from end
	while (text.size() > 0 && text.back() == '\0') { text.pop_back(); }

	return text;
}

std::optional<std::filesystem::path> selectFont() {
	CHOOSEFONT cf;	// common dialog box structure
	LOGFONT lf;		// logical font structure

	// Initialize CHOOSEFONT
	ZeroMemory(&cf, sizeof(cf));
	cf.lStructSize = sizeof(cf);
	cf.lpLogFont = &lf;
	cf.Flags = CF_FORCEFONTEXIST | CF_TTONLY;

	if (::ChooseFont(&cf) == TRUE) {
		auto path = GetRegString(
			HKEY_LOCAL_MACHINE,
			R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts)",
			std::string(cf.lpLogFont->lfFaceName) + " (TrueType)");
		if (path.empty()) {
			path = GetRegString(
				HKEY_CURRENT_USER,
				R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts)",
				std::string(cf.lpLogFont->lfFaceName) + " (TrueType)");
		} else {
			path = R"(C:\Windows\Fonts\)" + path;
		}
		if (!path.empty() && std::filesystem::exists(path)) { return path; }
	}

	return {};
}
#endif
