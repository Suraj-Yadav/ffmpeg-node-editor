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
	std::filesystem::path prefs;
	Paths();
};

struct Preference {
	Style style;
	std::filesystem::path font;
	int fontSize;
	std::string player;

	Preference();

	bool load();
	bool save();
	void draw();
};
