#include <fmt/core.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <vector>

#include "backend.hpp"
#include "ffmpeg/profile.hpp"
#include "file_utils.hpp"
#include "node_editor.hpp"
#include "pref.hpp"

enum MenuAction {
	MenuActionNone = 0,
	MenuActionNew,
	MenuActionOpen,
	MenuActionSave,
	MenuActionPreference,
};

const auto NO_ACTIVE_EDITOR = "No Active Editor";

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
	Preference pref;

	auto profile = GetProfile();
	pref.load();

	BackendWrapperGlfw3OpenGL3 backend;
	if (!backend.InitWindow(
			ImGuiConfigFlags_NavEnableKeyboard |
				ImGuiConfigFlags_NavEnableGamepad,
			pref)) {
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
		{"File", "Save", MenuActionSave, ImGuiKey_S, true},
		{"File", "Preferences", MenuActionPreference, ImGuiKey_Comma, true},
	});

	bool showPreference = false;

	// Our state
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	std::vector<NodeEditor> editors;
	int untitledCount = 0;
	int focusedEditor = -1;

	// Main loop
	while (backend.IsNewFrameAvailable()) {
		MenuAction menuAction = static_cast<MenuAction>(backend.DrawMenu());
		switch (menuAction) {
			case MenuActionNew: {
				editors.emplace_back(
					profile, fmt::format("Untitled {}", untitledCount));
				untitledCount++;
				break;
			}
			case MenuActionOpen: {
				if (auto path = openFile(); path.has_value()) {
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
			case MenuActionSave: {
				if (focusedEditor != -1) {
					auto& editor = editors[focusedEditor];
					if (!editor.getPath().empty()) {
						editor.save(editor.getPath());
					} else {
						if (auto path = saveFile(); path.has_value()) {
							editor.save(path.value());
							editor.setPath(path.value());
						} else {
							SPDLOG_INFO("User pressed cancel");
						}
					}
				} else {
					ImGui::OpenPopup(NO_ACTIVE_EDITOR);
				}
				break;
			}
			case MenuActionPreference: {
				showPreference = !showPreference;
				break;
			}
			case MenuActionNone: {
			}
		}

		focusedEditor = -1;
		for (auto i = 0; i < editors.size(); ++i) {
			if (editors[i].draw(pref)) { focusedEditor = i; };
		}

		if (showPreference) { pref.draw(); }

		if (ImGui::BeginPopupModal(NO_ACTIVE_EDITOR)) {
			ImGui::BeginHorizontal(__LINE__);
			ImGui::Spring();
			ImGui::Text("No Editor is active");
			ImGui::Spring();
			ImGui::EndHorizontal();
			ImGui::BeginHorizontal(__LINE__);
			ImGui::Spring();
			if (ImGui::Button("Ok")) { ImGui::CloseCurrentPopup(); }
			ImGui::Spring();
			ImGui::EndHorizontal();
			ImGui::EndPopup();
		}

		// Rendering
		backend.Render(clear_color);
	}

	// Cleanup
	backend.Shutdown();

	return 0;
}
