#pragma once

#include <imgui.h>

#include <filesystem>

enum StyleColor {
	NodeHeader = 0,
	NodeBg,
	Border,
	Wire,
	VideoSocket,
	AudioSocket,
	SubtitleSocket,
	COUNT
};

struct Style {
	ImVec4 colors[StyleColor::COUNT];
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
