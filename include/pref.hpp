#pragma once

#include <imgui.h>

#include <filesystem>
#include <map>

enum class StyleColor {
	NodeHeader = 0,
	NodeBg,
	Border,
	Wire,
	VideoSocket,
	AudioSocket,
	SubtitleSocket
};

struct Style {
	std::map<StyleColor, ImU32> colors;
	int colorPicker;
	Style();
};

struct Paths {
	std::filesystem::path appDir;
	std::filesystem::path prefs;
	std::string iniFile;
	Paths();
};

const Paths path;

struct Preference {
	Style style;
	std::filesystem::path font;
	int fontSize;
	std::string player;
	bool unsaved = false;

	bool isOpen = false;

	Preference();

	void setOptions() const;
	void draw();

	void close();

	[[nodiscard]] bool hasChanges() const { return unsaved; }
	bool load();
	bool save();
};
