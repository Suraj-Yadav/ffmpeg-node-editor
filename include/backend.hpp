#pragma once

#include <GLFW/glfw3.h>
#include <absl/strings/string_view.h>
#include <imgui.h>

#include <string>
#include <vector>

struct MenuItem {
	absl::string_view root;
	absl::string_view name;
	int id;
	ImGuiKey key = ImGuiKey_None;
	bool keyCtrl = false, keyShift = false, keyAlt = false;
	std::string shortcut;
};

class BackendWrapperGlfw3OpenGL3 {
	// Data
	std::string glsl_version;
	GLFWwindow* window;
	ImGuiContext* ctx;
	std::vector<MenuItem> menu;

   public:
	bool InitWindow(ImGuiConfigFlags flags);
	void Setup();
	void SetupMenuBar(const std::initializer_list<MenuItem>&);
	int DrawMenu();
	bool IsNewFrameAvailable();
	void Render(const ImVec4& clear_color);
	void Shutdown();
};
