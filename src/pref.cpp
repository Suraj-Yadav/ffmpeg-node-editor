#include "pref.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imnodes.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

#include "imgui_extras.hpp"
#include "string_utils.hpp"
#include "util.hpp"

Style::Style() : colorPicker(0) {
	using namespace ImGui;

	ImNodesStyle style;
	ImNodes::StyleColorsDark(&style);

	colors[StyleColor::NodeHeader] = style.Colors[ImNodesCol_TitleBar];
	colors[StyleColor::NodeBg] = style.Colors[ImNodesCol_NodeBackground];
	colors[StyleColor::Border] = style.Colors[ImNodesCol_NodeOutline];
	colors[StyleColor::Wire] = style.Colors[ImNodesCol_Link];
	colors[StyleColor::VideoSocket] = IM_COL32(255, 0, 0, 255);
	colors[StyleColor::AudioSocket] = IM_COL32(0, 255, 0, 255);
	colors[StyleColor::SubtitleSocket] = IM_COL32(0, 0, 255, 255);
}

Preference::Preference()
	:
#if defined(APP_OS_WINDOWS)
	  font(R"(C:\Windows\Fonts\segoeui.ttf)"),
	  fontSize(24),
#endif
	  player("vlc\n%f") {
}

Paths::Paths() {
#if defined(APP_OS_WINDOWS)
	char* p = nullptr;
	size_t len = 0;
	if (_dupenv_s(&p, &len, "APPDATA") == 0) { appDir = p; }
#elif defined(APP_OS_LINUX)
	auto* p = std::getenv("HOME");
	if (p != nullptr) {
		appDir = std::filesystem::path(p) / ".local" / "share";
	}
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
	unsaved = false;
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
	unsaved = false;
	return true;
}

void Preference::draw() {
	if (!isOpen) { return; }
	using namespace ImGui;
	bool changed = false;
	if (ImGui::Begin(
			"Edit Preference", &isOpen, UnsavedDocumentFlag(unsaved))) {
		BeginVertical("preference");
		PushID("preference");
		if (CollapsingHeader("Colors")) {
			for (auto& elem : style.colors) {
				PushID(&elem);
				BeginHorizontal(&elem);
				Text(StyleColorName(elem.first));
				Spring();
				if (ImVec4 col = ColorConvertU32ToFloat4(elem.second);
					ColorEdit4("##col", &col.x)) {
					changed = true;
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
				if (Combo(
						"##picker", &style.colorPicker, "HueBar\0HueWheel\0")) {
					changed = true;
				}
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
				isOpen = true;
				changed = true;
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
	unsaved = unsaved || changed;

	if (!isOpen) { close(); }

	if (changed) { setOptions(); }
}

void Preference::close() {
	using namespace ImGui;
	isOpen = false;

	switch (UnsavedDocumentClose(
		unsaved, isOpen, "Unsaved Preference Changes",
		"Do you want to save current preferences?")) {
		case UnsavedDocumentAction::SAVE_CHANGES:
			save();
			break;
		case UnsavedDocumentAction::CANCEL_CLOSE:
			isOpen = true;
			break;
		case UnsavedDocumentAction::DISCARD_CHANGES:
			load();
			break;
		case UnsavedDocumentAction::NO_OP:
			break;
	}
}

void Preference::setOptions() const {
	ImNodesStyle& imNodesStyle = ImNodes::GetStyle();
	imNodesStyle.Colors[ImNodesCol_TitleBar] =
		style.colors.at(StyleColor::NodeHeader);
	imNodesStyle.Colors[ImNodesCol_NodeBackground] =
		style.colors.at(StyleColor::NodeBg);
	imNodesStyle.Colors[ImNodesCol_NodeOutline] =
		style.colors.at(StyleColor::Border);
	imNodesStyle.Colors[ImNodesCol_Link] = style.colors.at(StyleColor::Wire);

	ImGui::SetColorEditOptions(
		(style.colorPicker == 0 ? ImGuiColorEditFlags_PickerHueBar
								: ImGuiColorEditFlags_PickerHueWheel) |
		ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip);
}
