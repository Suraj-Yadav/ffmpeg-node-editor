#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <vector>

#include "backend.hpp"
#include "ffmpeg/profile.hpp"
#include "file_utils.hpp"
#include "node_editor.hpp"

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

	// Our state
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	std::vector<NodeEditor> editors;
	int untitledCount = 0;

	// Main loop
	while (backend.IsNewFrameAvailable()) {
		// Menu Bar
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				auto possibleName = fmt::format("Untitled {}", untitledCount);
				if (ImGui::MenuItem("New", "CTRL+N")) {
					editors.emplace_back(&profile, possibleName);
					untitledCount++;
				}
				if (ImGui::MenuItem("Open..", "CTRL+O")) {
					auto path = openFile();
					if (path.has_value()) {
						auto itr = std::find_if(
							editors.begin(), editors.end(), [&](const auto& e) {
								return std::filesystem::equivalent(
									e.getPath(), path.value());
							});
						if (itr == editors.end()) {
							NodeEditor e(&profile, possibleName);
							if (e.load(path.value())) { editors.push_back(e); }

						} else {
							ImGui::SetWindowFocus(itr->getName().c_str());
						}
					} else {
						SPDLOG_INFO("User pressed cancel");
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
