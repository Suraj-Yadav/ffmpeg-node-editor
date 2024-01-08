#pragma once

#include <filesystem>
#include <optional>
#include <vector>

std::optional<std::filesystem::path> openFile();
std::optional<std::filesystem::path> saveFile();

std::optional<std::filesystem::path> selectFont();

std::vector<unsigned char> getFontData(std::string);
