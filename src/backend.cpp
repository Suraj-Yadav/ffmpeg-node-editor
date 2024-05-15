#include "backend.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <array>
#include <iostream>

#include "IconsFontAwesome6.h"
#include "IconsFontAwesome6.h_fa-solid-900.ttf.h"

constexpr std::array<ImWchar, 3> icons_ranges{ICON_MIN_FA, ICON_MAX_16_FA, 0};

static void glfw_error_callback(int error, const char* description) {
	std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

bool BackendWrapperGlfw3OpenGL3::InitWindow(
	ImGuiConfigFlags flags, const Preference& pref) {
	glfwSetErrorCallback(glfw_error_callback);
	if (glfwInit() == 0) { return false; }

	glsl_version = "#version 330 core";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	float highDPIscaleFactor = 1.0;
#ifdef _WIN32
	// if it's a HighDPI monitor, try to scale everything
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	float xScale = 1, yScale = 1;
	glfwGetMonitorContentScale(monitor, &xScale, &yScale);
	if (xScale > 1 || yScale > 1) {
		highDPIscaleFactor = xScale;
		glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
	}
#elif __APPLE__
	// to prevent 1200x800 from becoming 2400x1600
	glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
#endif

	// Create window with graphics context
	constexpr auto WIDTH = 1280, HEIGHT = 720;
	window =
		glfwCreateWindow(WIDTH, HEIGHT, "ffmpeg Node Editor", nullptr, nullptr);
	if (window == nullptr) { return false; }
	glfwMakeContextCurrent(window);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ctx = ImGui::CreateContext();

	// Setup Font
	auto fontSize = pref.fontSize * float(highDPIscaleFactor);
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

	// static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};

	ImFontConfig icons_config;
	icons_config.MergeMode = true;
	icons_config.PixelSnapH = true;
	icons_config.GlyphMinAdvanceX = iconFontSize;
	icons_config.FontDataOwnedByAtlas = false;
	auto* f = io.Fonts->AddFontFromMemoryTTF(
		static_cast<void*>(s_fa_solid_900_ttf), sizeof(s_fa_solid_900_ttf),
		iconFontSize, &icons_config, icons_ranges.data());
	if (f != nullptr) { io.FontDefault = f; }

	ImGui::GetStyle().ScaleAllSizes(highDPIscaleFactor);

	return true;
}

void BackendWrapperGlfw3OpenGL3::Setup() {
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version.c_str());
}

bool BackendWrapperGlfw3OpenGL3::IsNewFrameAvailable() {
	auto shouldClose = glfwWindowShouldClose(window) != 0;
	if (shouldClose) { return false; }

	// Poll and handle events (inputs, window resize, etc.)
	// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to
	// tell if dear imgui wants to use your inputs.
	// - When io.WantCaptureMouse is true, do not dispatch mouse input data
	// to your main application, or clear/overwrite your copy of the mouse
	// data.
	// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input
	// data to your main application, or clear/overwrite your copy of the
	// keyboard data. Generally you may always pass all inputs to dear
	// imgui, and hide them from your application based on those two flags.
	glfwPollEvents();

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	return true;
}

void BackendWrapperGlfw3OpenGL3::Render(const ImVec4& clear_color) {
	ImGui::Render();
	int display_w = 0, display_h = 0;
	glfwGetFramebufferSize(window, &display_w, &display_h);
	glViewport(0, 0, display_w, display_h);
	glClearColor(
		clear_color.x * clear_color.w, clear_color.y * clear_color.w,
		clear_color.z * clear_color.w, clear_color.w);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	glfwSwapBuffers(window);
}

void BackendWrapperGlfw3OpenGL3::Shutdown() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	if (ctx != nullptr) { ImGui::DestroyContext(ctx); }
	glfwDestroyWindow(window);
	glfwTerminate();
}

void BackendWrapperGlfw3OpenGL3::AddMenu(
	std::string_view menuName, const std::initializer_list<MenuItem>& items) {
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

int BackendWrapperGlfw3OpenGL3::DrawMenu() {
	using namespace ImGui;
	int returnValue = 0;
	if (BeginMainMenuBar()) {
		for (auto& elem : menu) {
			if (BeginMenu(elem.first.data())) {
				for (auto& item : elem.second) {
					auto clicked =
						ImGui::MenuItem(item.name.data(), item.shortcut.data());
					if (clicked) { returnValue = item.id; }
				}
				EndMenu();
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
