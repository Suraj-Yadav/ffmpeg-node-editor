#include <fmt/core.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <vector>

#include "backend.hpp"
#include "extras/IconsFontAwesome6.h"
#include "extras/IconsFontAwesome6.h_fa-solid-900.ttf.h"
#include "ffmpeg/profile.hpp"
#include "file_utils.hpp"
#include "node_editor.hpp"

enum MenuAction {
	MenuActionNone = 0,
	MenuActionNew,
	MenuActionOpen,
};

int main() {
	std::set_terminate([]() {
		auto excPtr = std::current_exception();
		try {
			if (excPtr) { std::rethrow_exception(excPtr); }
		} catch (const std::exception& e) {
			SPDLOG_CRITICAL("Unhandled exception {}\n", e.what());
		}
		std::abort();
	});

	auto profile = GetProfile();

	BackendWrapperGlfw3OpenGL3 backend;
	if (!backend.InitWindow(
			ImGuiConfigFlags_NavEnableKeyboard |
			ImGuiConfigFlags_NavEnableGamepad)) {
		return 1;
	}

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	// ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	backend.Setup();
	backend.SetupMenuBar({
		{"File", "New", MenuActionNew, ImGuiKey_N, true},
		{"File", "Open..", MenuActionOpen, ImGuiKey_O, true},
	});

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Build();
	float baseFontSize = io.Fonts->Fonts.front()->FontSize;
	// FontAwesome fonts need to have their sizes reduced
	// by 2.0f/3.0f in order to align correctly
	float iconFontSize = baseFontSize * 2.0f / 3.0f;

	// merge in icons from Font Awesome
	static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
	ImFontConfig icons_config;
	icons_config.MergeMode = true;
	icons_config.PixelSnapH = true;
	icons_config.GlyphMinAdvanceX = iconFontSize;
	icons_config.FontDataOwnedByAtlas = false;
	auto f = io.Fonts->AddFontFromMemoryTTF(
		s_fa_solid_900_ttf, sizeof(s_fa_solid_900_ttf), iconFontSize,
		&icons_config, icons_ranges);
	if (f != nullptr) { io.FontDefault = f; }

	// Our state
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	std::vector<NodeEditor> editors;
	int untitledCount = 0;

	Style style;

	// Main loop
	while (backend.IsNewFrameAvailable()) {
		MenuAction menuAction = static_cast<MenuAction>(backend.DrawMenu());
		ImGui::ShowDemoWindow();
		switch (menuAction) {
			case MenuActionNew: {
				editors.emplace_back(
					profile, fmt::format("Untitled {}", untitledCount));
				untitledCount++;
				break;
			}
			case MenuActionOpen: {
				auto path = openFile();
				if (path.has_value()) {
					auto itr = std::find_if(
						editors.begin(), editors.end(), [&](const auto& e) {
							return std::filesystem::equivalent(
								e.getPath(), path.value());
						});
					if (itr == editors.end()) {
						NodeEditor e(profile, "");
						if (e.load(path.value())) { editors.push_back(e); }
					} else {
						ImGui::SetWindowFocus(itr->getName().c_str());
					}
				} else {
					SPDLOG_INFO("User pressed cancel");
				}
				break;
			}
			case MenuActionNone: {
			}
		}

		for (auto& editor : editors) { editor.draw(style); }

		// Rendering
		backend.Render(clear_color);
	}

	// Cleanup
	backend.Shutdown();

	return 0;
}
