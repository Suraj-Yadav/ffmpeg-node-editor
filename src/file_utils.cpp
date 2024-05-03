#include "file_utils.hpp"

#include <Windows.h>
#include <nfd.h>
#include <spdlog/spdlog.h>
#include <wingdi.h>
#include <winnt.h>
#include <winreg.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <string>

std::optional<std::filesystem::path> openFile() {
	nfdchar_t* outPath = nullptr;
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
std::optional<std::filesystem::path> saveFile() {
	nfdchar_t* savePath = nullptr;
	nfdresult_t result = NFD_SaveDialog("json", NULL, &savePath);
	if (result == NFD_OKAY) {
		std::filesystem::path v(savePath);
		if (!v.has_extension()) { v.replace_extension(".json"); }
		free(savePath);
		return v;
	} else if (result == NFD_CANCEL) {
		return {};
	} else {
		throw std::runtime_error(NFD_GetError());
	}
	return {};
}

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
			"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
			std::string(cf.lpLogFont->lfFaceName) + " (TrueType)");
		if (path.empty()) {
			path = GetRegString(
				HKEY_CURRENT_USER,
				"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
				std::string(cf.lpLogFont->lfFaceName) + " (TrueType)");
		} else {
			path = "C:\\Windows\\Fonts\\" + path;
		}
		if (!path.empty() && std::filesystem::exists(path)) { return path; }
	}

	return {};
}
