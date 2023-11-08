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

	// NodeEditor editor(&profile);
	namespace ed = ax::NodeEditor;

	ed::Config config;
	config.SettingsFile = nullptr;
	auto m_Context = ed::CreateEditor(&config);
	ed::Config config2;
	config2.SettingsFile = nullptr;
	auto m_Context2 = ed::CreateEditor(&config2);

	// Main loop
	while (backend.IsNewFrameAvailable()) {
		// ImGui::Begin("asd1");
		// ed::SetCurrentEditor(m_Context);
		// ed::Begin("My Editor");
		// int uniqueId = 1;
		// // Start drawing nodes.
		// ed::BeginNode(uniqueId++);
		// ImGui::Text("Node A");
		// ed::BeginPin(uniqueId++, ed::PinKind::Input);
		// ImGui::Text("-> In");
		// ed::EndPin();
		// ImGui::SameLine();
		// ed::BeginPin(uniqueId++, ed::PinKind::Output);
		// ImGui::Text("Out ->");
		// ed::EndPin();
		// ed::EndNode();
		// ed::End();
		// ed::SetCurrentEditor(nullptr);
		// ImGui::End();

		// ImGui::Begin("asd");
		// ed::SetCurrentEditor(m_Context2);
		// ed::Begin("My Editor");
		// uniqueId = 1;
		// // Start drawing nodes.
		// ed::BeginNode(uniqueId++);
		// ImGui::Text("Node B");
		// ed::BeginPin(uniqueId++, ed::PinKind::Input);
		// ImGui::Text("-> In");
		// ed::EndPin();
		// ImGui::SameLine();
		// ed::BeginPin(uniqueId++, ed::PinKind::Output);
		// ImGui::Text("Out ->");
		// ed::EndPin();
		// ed::EndNode();
		// ed::End();
		// ed::SetCurrentEditor(nullptr);
		// ImGui::End();

		// Menu Bar
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("New", "CTRL+N")) {
					editors.emplace_back(&profile);
				}
				if (ImGui::MenuItem("Open..", "CTRL+O")) {
					nfdchar_t* outPath = NULL;
					nfdresult_t result = NFD_OpenDialog(NULL, NULL, &outPath);

					if (result == NFD_OKAY) {
						load(outPath, profile, editors);
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
