#include "backend.hpp"

#include <imgui.h>

#include <array>

//
#include <IconsFontAwesome6.h>
#include <IconsFontAwesome6.h_fa-solid-900.ttf.h>

constexpr std::array<ImWchar, 3> icons_ranges{ICON_MIN_FA, ICON_MAX_16_FA, 0};

namespace Window {
	// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

	std::vector<std::pair<std::string_view, std::vector<MenuItem>>> menu;

	// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

	void AddMenu(
		std::string_view menuName,
		const std::initializer_list<MenuItem>& items) {
		menu.emplace_back();
		menu.back().first = menuName;
		menu.back().second.insert(menu.back().second.end(), items);
		for (auto& item : menu.back().second) {
			if (item.key == ImGuiKey_None) { continue; }
			item.shortcut = std::string(item.keyCtrl ? "Ctrl+" : "") +
							std::string(item.keyAlt ? "Alt+" : "") +
							std::string(item.keyShift ? "Shift+" : "") +
							ImGui::GetKeyName(item.key);
		}
	}

	int DrawMenu() {
		using namespace ImGui;
		int returnValue = 0;
		if (BeginMainMenuBar()) {
			for (auto& elem : menu) {
				if (BeginMenu(elem.first.data())) {
					for (auto& item : elem.second) {
						auto clicked = ImGui::MenuItem(
							item.name.data(), item.shortcut.data());
						if (clicked) { returnValue = item.id; }
					}
					ImGui::EndMenu();
				}
			}
			EndMainMenuBar();
		}
		ImGuiIO& io = ImGui::GetIO();
		for (auto& elem : menu) {
			for (auto& item : elem.second) {
				auto ctrl = !item.keyCtrl || io.KeyCtrl;
				auto shift = !item.keyShift || io.KeyShift;
				auto alt = !item.keyAlt || io.KeyAlt;
				if (ctrl && shift && alt && IsKeyPressed(item.key, false)) {
					returnValue = item.id;
				}
			}
		}
		return returnValue;
	}

}  // namespace Window

ImGuiContext* InitImGui(
	ImGuiConfigFlags flags, const Preference& pref,
	float highDPI_ScaleFactor = 1.0f) {
	IMGUI_CHECKVERSION();
	auto* ctx = ImGui::CreateContext();

	// Setup Font
	auto fontSize = pref.fontSize * float(highDPI_ScaleFactor);
	// FontAwesome fonts need to have their sizes reduced
	// by 2.0f/3.0f in order to align correctly
	constexpr auto ICON_FONT_SCALE = 2.0f / 3.0f;
	float iconFontSize = fontSize * ICON_FONT_SCALE;

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = path.iniFile.c_str();

	io.ConfigFlags |= flags;
	io.Fonts->Clear();
	if (!pref.font.empty()) {
		io.Fonts->AddFontFromFileTTF(pref.font.string().c_str(), fontSize);
	} else {
		io.Fonts->AddFontDefault();
	}

	ImFontConfig icons_config;
	icons_config.MergeMode = true;
	icons_config.PixelSnapH = true;
	icons_config.GlyphMinAdvanceX = iconFontSize;
	icons_config.FontDataOwnedByAtlas = false;
	auto* f = io.Fonts->AddFontFromMemoryTTF(
		static_cast<void*>(s_fa_solid_900_ttf), sizeof(s_fa_solid_900_ttf),
		iconFontSize, &icons_config, icons_ranges.data());
	if (f != nullptr) { io.FontDefault = f; }

	ImGui::GetStyle().ScaleAllSizes(highDPI_ScaleFactor);

	return ctx;
}
