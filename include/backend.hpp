#pragma once

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <string>
#include <string_view>
#include <vector>

#include "pref.hpp"

struct MenuItem {
	std::string_view name;
	int id;
	ImGuiKey key = ImGuiKey_None;
	bool keyCtrl = false, keyAlt = false, keyShift = false;
	std::string shortcut;
};

class BackendWrapperGlfw3OpenGL3 {
	// Data
	std::string glsl_version;
	GLFWwindow* window;
	ImGuiContext* ctx;
	std::vector<std::pair<std::string_view, std::vector<MenuItem>>> menu;

   public:
	bool InitWindow(ImGuiConfigFlags flags, const Preference& pref);
	void Setup();
	void AddMenu(
		std::string_view menuName, const std::initializer_list<MenuItem>&);
	int DrawMenu();
	bool IsNewFrameAvailable();
	void Render(const ImVec4& clear_color);
	void Shutdown();
};
