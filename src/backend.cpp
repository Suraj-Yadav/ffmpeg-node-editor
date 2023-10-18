#include "backend.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cstdio>
#include <iostream>

static void glfw_error_callback(int error, const char* description) {
	std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

bool BackendWrapperGlfw3OpenGL3::InitWindow(ImGuiConfigFlags flags) {
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) return false;

	glsl_version = "#version 330 core";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	float highDPIscaleFactor = 1.0;
#ifdef _WIN32
	// if it's a HighDPI monitor, try to scale everything
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	float xscale, yscale;
	glfwGetMonitorContentScale(monitor, &xscale, &yscale);
	if (xscale > 1 || yscale > 1) {
		highDPIscaleFactor = xscale;
		glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
	}
#elif __APPLE__
	// to prevent 1200x800 from becoming 2400x1600
	glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
#endif

	// Create window with graphics context
	window = glfwCreateWindow(1280, 720, "Test", nullptr, nullptr);
	if (window == nullptr) return false;
	glfwMakeContextCurrent(window);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ctx = ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= flags;
	io.Fonts->Clear();
	io.Fonts->AddFontFromFileTTF(
		"C:\\Windows\\Fonts\\segoeui.ttf", 24.0f * highDPIscaleFactor);

	ImGui::GetStyle().ScaleAllSizes(highDPIscaleFactor);

	return true;
}

void BackendWrapperGlfw3OpenGL3::Setup() {
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version.c_str());
}

bool BackendWrapperGlfw3OpenGL3::IsNewFrameAvailable() {
	auto shouldClose = glfwWindowShouldClose(window);
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
	int display_w, display_h;
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
