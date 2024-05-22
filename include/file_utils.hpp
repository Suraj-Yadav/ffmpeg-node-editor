#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

std::optional<std::filesystem::path> openFile();
std::optional<std::filesystem::path> saveFile(std::string_view fileType);

void showErrorMessage(std::string const&, std::string const&);

std::optional<std::filesystem::path> selectFont();

std::vector<unsigned char> getFontData(std::string);
