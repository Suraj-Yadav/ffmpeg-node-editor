#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include "util.hpp"

std::optional<std::filesystem::path> openFile();
std::optional<std::filesystem::path> saveFile(std::string_view fileType);

void showErrorMessage(std::string const&, std::string const&);

int showActionDialog(
	std::string const& title, std::string const& text,
	std::string_view type = "yesnocancel");

#if defined(APP_OS_WINDOWS)
#include <vector>
std::optional<std::filesystem::path> selectFont();

std::vector<unsigned char> getFontData(std::string);
#endif
