#include <iostream>

#include "backend.hpp"
#include "node_editor.hpp"

int main() {
	std::set_terminate([]() {
		auto excPtr = std::current_exception();
		try {
			if (excPtr) { std::rethrow_exception(excPtr); }
		} catch (const std::exception& e) {
			std::cout << "Unhandled exception: " << e.what() << std::endl;
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
	ImGuiIO& io = ImGui::GetIO();

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	// ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	backend.Setup();

	// Our state
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	NodeEditor editor(&profile);

	editor.addNode(profile.filters.front());
	editor.addNode(profile.filters.front());
	editor.addNode(profile.filters.front());
	editor.addNode(profile.filters.front());
	editor.addNode(profile.filters.front());

	// Main loop
	while (backend.IsNewFrameAvailable()) {
		//
		// 1) Commit known data to editor
		//

		editor.draw();

		// Submit Links
		// for (auto& linkInfo : m_Links)
		// 	ed::Link(linkInfo.Id, linkInfo.InputId, linkInfo.OutputId);

		//
		// 2) Handle interactions
		//

		// Handle creation action, returns true if editor want to create new
		// object (node or link)

		// Rendering
		backend.Render(clear_color);
	}

	// Cleanup
	backend.Shutdown();

	return 0;
}
