#ifdef IMGUI_USE_GLFW_OPENGL
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "pref.hpp"
#include "util.hpp"

ImGuiContext* InitImGui(
	ImGuiConfigFlags flags, const Preference& pref,
	float highDPI_ScaleFactor = 1.0f);

namespace Window {
	constexpr auto glsl_version = "#version 330 core";
	// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
	GLFWwindow* window;
	ImGuiContext* ctx;
	// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

	void glfw_error_callback(int error, const char* description) {
		SPDLOG_ERROR("GLFW ERROR: {}: {}", error, description);
	}

	bool InitWindow(ImGuiConfigFlags flags, const Preference& pref) {
		glfwSetErrorCallback(glfw_error_callback);
		if (glfwInit() == 0) { return false; }

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		float highDPIscaleFactor = 1.0;
#ifdef _WIN32
		glfwWindowHint(GLFW_WIN32_KEYBOARD_MENU, GLFW_TRUE);
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
		window = glfwCreateWindow(
			WIDTH, HEIGHT, "ffmpeg Node Editor", nullptr, nullptr);
		if (window == nullptr) { return false; }
		glfwMakeContextCurrent(window);

		ctx = InitImGui(flags, pref, highDPIscaleFactor);

		return true;
	}

	void Setup() {
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init(glsl_version);
	}

	bool IsNewFrameAvailable() {
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

	void Render(const ImVec4& clear_color) {
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

	void Shutdown() {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		if (ctx != nullptr) { ImGui::DestroyContext(ctx); }
		glfwDestroyWindow(window);
		glfwTerminate();
	}

}  // namespace Window
#endif
