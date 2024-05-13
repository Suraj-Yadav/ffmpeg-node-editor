#include "pref.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

#include "imgui_extras.hpp"
#include "string_utils.hpp"

Style::Style() : colorPicker(0) {
	using namespace ImGui;

	// ed::Style v;
	colors[StyleColor::NodeHeader] = IM_COL32(255, 0, 0, 255);
	// colors[StyleColor::NodeBg] =
	// 	ColorConvertFloat4ToU32(v.Colors[ed::StyleColor_NodeBg]);
	// colors[StyleColor::Border] =
	// 	ColorConvertFloat4ToU32(v.Colors[ed::StyleColor_NodeBorder]);
	// colors[Wire] = v.Colors[ed::StyleColor_];
	colors[StyleColor::VideoSocket] = IM_COL32(255, 0, 0, 255);
	colors[StyleColor::AudioSocket] = IM_COL32(0, 255, 0, 255);
	colors[StyleColor::SubtitleSocket] = IM_COL32(0, 0, 255, 255);
}

Preference::Preference()
	:
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
	  font(R"(C:\Windows\Fonts\segoeui.ttf)"),
#else
	  font(),
#endif
	  fontSize(24),
	  player("vlc\n%f") {
}

Paths::Paths() {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
	auto* p = std::getenv("APPDATA");
	if (p != nullptr) { appDir = p; }
#elif __linux__ || __unix__ || defined(_POSIX_VERSION)
	auto* p = std::getenv("HOME");
	if (p != nullptr) {
		appDir = std::filesystem::path(p) / ".local" / "share";
	}
// #elif __APPLE__
#else
#error "Unknown Platform"
#endif
	if (appDir.empty()) {
		SPDLOG_ERROR("Unable to find appdata folder");
		throw std::invalid_argument("Unable to find appdata folder");
	}

	appDir = appDir / "ffmpeg-node-editor";
	std::filesystem::create_directories(appDir);

	prefs = appDir / "prefs.json";
	iniFile = (appDir / "imgui.ini").string();
}

std::string_view StyleColorName(StyleColor val) {
	switch (val) {
		case StyleColor::NodeHeader:
			return "NodeHeader";
		case StyleColor::NodeBg:
			return "NodeBg";
		case StyleColor::Border:
			return "Border";
		case StyleColor::Wire:
			return "Wire";
		case StyleColor::VideoSocket:
			return "VideoSocket";
		case StyleColor::AudioSocket:
			return "AudioSocket";
		case StyleColor::SubtitleSocket:
			return "SubtitleSocket";
	}
	return "";
}

template <typename T>
inline void getNull(nlohmann::json& json, std::string_view name, T& dest) {
	if (json[name].is_null()) { return; }
	dest = json[name].get<T>();
}

bool Preference::load() {
	nlohmann::json json;
	try {
		json = nlohmann::json::parse(std::ifstream(path.prefs));
	} catch (nlohmann::json::exception&) { return false; }
	auto& colors = json["colors"];
	if (!colors.is_null()) {
		for (auto& elem : style.colors) {
			auto name = StyleColorName(elem.first);
			if (!colors[name].is_null()) {
				elem.second = ImGui::ColorConvertHexToU32(
					colors[name].get<std::string>());
			}
		}
	}

	getNull(json, "font", font);
	getNull(json, "font_size", fontSize);
	getNull(json, "color_picker", style.colorPicker);
	getNull(json, "player", player);

	return false;
}

bool Preference::save() {
	nlohmann::json obj;
	using namespace ImGui;
	auto& colors = obj["colors"];
	for (auto& elem : style.colors) {
		colors[StyleColorName(elem.first)] = ColorConvertU32ToHex(elem.second);
	}
	obj["color_picker"] = style.colorPicker;
	obj["font"] = font.string();
	obj["font_size"] = fontSize;
	obj["player"] = player;

	std::filesystem::create_directories(path.prefs.parent_path());

	std::ofstream o(path.prefs, std::ios_base::binary);
	o << std::setw(4) << obj;
	return true;
}

void Preference::draw() {
	if (!show) { return; }
	using namespace ImGui;
	if (ImGui::Begin("Edit Preference", &show)) {
		BeginVertical("preference");
		PushID("preference");
		if (CollapsingHeader("Colors")) {
			for (auto& elem : style.colors) {
				PushID(static_cast<int>(elem.first));
				BeginHorizontal(&elem);
				Text(StyleColorName(elem.first));
				Spring();
				if (ImVec4 col = ColorConvertU32ToFloat4(elem.second);
					ColorEdit4(
						"##col", &col.x,
						(style.colorPicker == 0
							 ? ImGuiColorEditFlags_PickerHueBar
							 : ImGuiColorEditFlags_PickerHueWheel) |
							ImGuiColorEditFlags_NoInputs |
							ImGuiColorEditFlags_NoTooltip)) {
					elem.second = ColorConvertFloat4ToU32(col);
				}
				EndHorizontal();
				PopID();
			}
		}
		auto width = 0.0f;
		if (CollapsingHeader("UI", ImGuiTreeNodeFlags_DefaultOpen)) {
			{
				BeginHorizontal(&style.colorPicker);
				TextUnformatted("Color Selector");
				Spring();
				Combo("##picker", &style.colorPicker, "HueBar\0HueWheel\0");
				width = GetItemRectSize().x;
				EndHorizontal();
			}
			{
				BeginHorizontal(&font);
				TextUnformatted("Font");
				Spring();
				auto fontStr = font.string();
				if (InputFont("##font", fontStr, width)) { font = fontStr; }
				EndHorizontal();
			}
			{
				BeginHorizontal(&fontSize);
				TextUnformatted("Font Size");
				Spring();
				DragInt("##fonstsize", &fontSize, 1.0f, 12, 1000);
				EndHorizontal();
			}
			{
				BeginHorizontal(&player);
				TextUnformatted("Player");
				if (ImGui::BeginItemTooltip()) {
					TextUnformatted("Requirements:");
					TextUnformatted("a command to invoke player for preview");
					TextUnformatted(
						"command should block and wait for user to exit the "
						"player");
					TextUnformatted(
						"each argument should be in a separate line");
					TextUnformatted(
						"do not use \" or ' to quote args with space");
					TextUnformatted(
						"use '%f' to specify where filename should be put");
					TextUnformatted(
						"Eg (assuming vlc is in PATH)\n\tvlc\n\t%f ");
					EndTooltip();
				}
				Spring();
				if (InputTextMultiline("##player", &player)) {
					for (auto& elem : str::split(player, '\n')) {
						SPDLOG_DEBUG("elem = {}", str::strip(elem));
					};
				}
				EndHorizontal();
			}
		}
		{
			BeginHorizontal(this);
			Spring();
			if (Button("Reset")) {
				*this = {};
				show = true;
			}
			Spring(0.5);
			if (Button("Save")) { save(); }
			Spring();
			EndHorizontal();
		}
		PopID();
		EndVertical();
	}
	ImGui::End();
}
