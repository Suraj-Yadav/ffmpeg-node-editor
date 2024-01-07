#include "pref.hpp"

#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>
#include <imgui-node-editor/imgui_node_editor.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <stdexcept>
#include <string>

#include "imgui_extras.hpp"

namespace ed = ax::NodeEditor;

Style::Style() : colorPicker(0) {
	using namespace ImGui;

	ed::Style v;
	colors[NodeHeader] = ColorConvertU32ToFloat4(IM_COL32(255, 0, 0, 255));
	colors[NodeBg] = v.Colors[ed::StyleColor_NodeBg];
	colors[Border] = v.Colors[ed::StyleColor_NodeBorder];
	// colors[Wire] = v.Colors[ed::StyleColor_];
	colors[VideoSocket] = ColorConvertU32ToFloat4(IM_COL32(255, 0, 0, 255));
	colors[AudioSocket] = ColorConvertU32ToFloat4(IM_COL32(0, 255, 0, 255));
	colors[SubtitleSocket] = ColorConvertU32ToFloat4(IM_COL32(0, 0, 255, 255));
}

Preference::Preference()
	: style(),
	  font("C:\\Windows\\Fonts\\segoeui.ttf"),
	  fontSize(24),
	  player("vlc\n%f") {}

Paths::Paths() {
	auto p = std::getenv("APPDATA");
	if (p == nullptr) {
		SPDLOG_ERROR("Unable to find appdata folder");
		throw std::invalid_argument("Unable to find appdata folder");
	}
	std::filesystem::path root(p);
	root = root / "ffmpeg-node-editor";

	std::filesystem::create_directories(root);

	prefs = root / "prefs.json";
}

const Paths path;

absl::string_view StyleColorName(StyleColor val) {
#define CASE(VAL) \
	case VAL:     \
		return #VAL
	switch (val) {
		CASE(NodeHeader);
		CASE(NodeBg);
		CASE(Border);
		CASE(Wire);
		CASE(VideoSocket);
		CASE(AudioSocket);
		CASE(SubtitleSocket);
		case COUNT:
			return "";
	}
#undef CASE
	return "";
}

bool Preference::load() {
	nlohmann::json json;
	try {
		json = nlohmann::json::parse(std::ifstream(path.prefs));
	} catch (nlohmann::json::exception&) { return false; }
	auto& colors = json["colors"];
	if (!colors.is_null()) {
		for (int i = 0; i < StyleColor::COUNT; ++i) {
			auto name = StyleColorName(StyleColor(i));
			if (!colors[name].is_null()) {
				style.colors[i] = ImGui::ColorConvertHexToFloat4(
					colors[name].get<std::string>());
			}
		}
	}
#define SET(X, Y, T) \
	if (!json[X].is_null()) { (Y) = json[X].get<T>(); }

	SET("font", font, std::filesystem::path);
	SET("font_size", fontSize, int);
	SET("color_picker", style.colorPicker, int);
	SET("player", player, std::string);

#undef SET
	return true;
}

bool Preference::save() {
	nlohmann::json obj;
	using namespace ImGui;
	auto& colors = obj["colors"];

	for (int i = 0; i < StyleColor::COUNT; ++i) {
		colors[StyleColorName(StyleColor(i))] =
			ColorConvertFloat4ToHex(style.colors[i]);
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
	using namespace ImGui;
	if (ImGui::Begin("Edit Preference")) {
		BeginVertical("preference");
		PushID("preference");
		if (CollapsingHeader("Colors")) {
			for (int i = 0; i < StyleColor::COUNT; ++i) {
				PushID(i);
				BeginHorizontal(i);
				Text("%s", StyleColorName(StyleColor(i)).data());
				Spring();
				ColorEdit4(
					"##col", &style.colors[i].x,
					(style.colorPicker == 0
						 ? ImGuiColorEditFlags_PickerHueBar
						 : ImGuiColorEditFlags_PickerHueWheel) |
						ImGuiColorEditFlags_NoInputs |
						ImGuiColorEditFlags_NoTooltip);
				EndHorizontal();
				PopID();
			}
		}
		int id = -1;
		if (CollapsingHeader("UI"), ImGuiTreeNodeFlags_DefaultOpen) {
			{
				BeginHorizontal(id--);
				Text("Color Selector");
				Spring();
				Combo("##picker", &style.colorPicker, "HueBar\0HueWheel\0");
				EndHorizontal();
			}
			{
				BeginHorizontal(id--);
				Text("Font");
				Spring();
				auto fontStr = font.string();
				if (InputFont("##font", fontStr)) { font = fontStr; }
				EndHorizontal();
			}
			{
				BeginHorizontal(id--);
				Text("Font Size");
				Spring();
				DragInt("##fonstsize", &fontSize, 1.0f, 12, 1000);
				EndHorizontal();
			}
			{
				BeginHorizontal(id--);
				Text("Player");
				if (ImGui::BeginItemTooltip()) {
					ImGui::Text("Requirements:");
					ImGui::Text("a command to invoke player for preview");
					ImGui::Text(
						"command should block and wait for user to exit the "
						"player");
					ImGui::Text("each argument should be in a separate line");
					ImGui::Text("do not use \" or ' to quote args with space");
					ImGui::Text(
						"use '%%f' to specify where filename should be put");
					ImGui::Text("Eg (assuming vlc is in PATH)\n\tvlc\n\t%%f ");
					ImGui::EndTooltip();
				}
				Spring();
				if (InputTextMultiline("##player", &player)) {
					for (auto& elem :
						 absl::StrSplit(player, '\n', absl::SkipWhitespace())) {
						SPDLOG_INFO("elem = {}", std::string(elem));
					};
				}

				EndHorizontal();
			}
		}
		{
			BeginHorizontal(id--);
			Spring();
			if (Button("Reset")) { *this = {}; }
			Spring();
			if (Button("Save")) { save(); }
			Spring();
			EndHorizontal();
		}
		PopID();
		EndVertical();
	}
	ImGui::End();
}
