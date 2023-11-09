#include <imgui.h>
#include <nfd.h>
#include <spdlog/spdlog.h>

#include <iostream>
#include <vector>

#include "backend.hpp"
#include "node_editor.hpp"

int main() {
	GraphState state;
	std::set_terminate([]() {
		auto excPtr = std::current_exception();
		try {
			if (excPtr) { std::rethrow_exception(excPtr); }
		} catch (const std::exception& e) {
			spdlog::critical("Unhandled exception {}\n", e.what());
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

	// Our state
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	std::vector<NodeEditor> editors;
	int untitledCount = 0;

	// Main loop
	while (backend.IsNewFrameAvailable()) {
		// Menu Bar
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("New", "CTRL+N")) {
					editors.emplace_back(
						&profile, fmt::format("Untitled {}", untitledCount++));
				}
				if (ImGui::MenuItem("Open..", "CTRL+O")) {
					nfdchar_t* outPath = NULL;
					nfdresult_t result = NFD_OpenDialog(NULL, NULL, &outPath);
					if (result == NFD_OKAY) {
						NodeEditor e(
							&profile,
							fmt::format("Untitled {}", untitledCount));
						if (e.load(outPath)) { editors.push_back(e); }
						free(outPath);
					} else if (result == NFD_CANCEL) {
						spdlog::info("User pressed cancel");
					} else {
						spdlog::error("Error: {}", NFD_GetError());
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		for (auto& editor : editors) { editor.draw(); }

		// Rendering
		backend.Render(clear_color);
	}

	// Cleanup
	backend.Shutdown();

	return 0;
}
