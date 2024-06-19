#include "file_utils.hpp"

#include <tinyfiledialogs.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <string_utils.hpp>
#include <string_view>

#include "util.hpp"

std::optional<std::filesystem::path> openFile() {
	auto* selection =
		tinyfd_openFileDialog("Open File", nullptr, 0, nullptr, nullptr, 0);
	if (selection == nullptr) { return {}; }
	return selection;
}
std::optional<std::filesystem::path> saveFile(std::string_view fileType) {
	std::array<const char*, 1> type{fileType.data()};
	auto* selection =
		tinyfd_saveFileDialog("Save File", nullptr, 1, type.data(), nullptr);
	if (selection == nullptr) { return {}; }
	fileType.remove_prefix(1);
	if (str::ends_with(selection, fileType)) { return selection; }
	return selection + std::string(fileType);
	return selection;
}

void showErrorMessage(std::string const& title, std::string const& text) {
	std::string title_copy = title, text_copy = text;
	std::replace(title_copy.begin(), title_copy.end(), '\'', '|');
	std::replace(title_copy.begin(), title_copy.end(), '"', '|');
	std::replace(text_copy.begin(), text_copy.end(), '\'', '|');
	std::replace(text_copy.begin(), text_copy.end(), '"', '|');
	tinyfd_messageBox(title_copy.c_str(), text_copy.c_str(), "ok", "error", 0);
}

int showActionDialog(
	std::string const& title, std::string const& text, std::string_view type) {
	std::string title_copy = title, text_copy = text;
	std::replace(title_copy.begin(), title_copy.end(), '\'', '|');
	std::replace(title_copy.begin(), title_copy.end(), '"', '|');
	std::replace(text_copy.begin(), text_copy.end(), '\'', '|');
	std::replace(text_copy.begin(), text_copy.end(), '"', '|');
	return tinyfd_messageBox(
		title_copy.c_str(), text_copy.c_str(), "yesnocancel", "question", 0);
}

#if defined(APP_OS_WINDOWS)
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
