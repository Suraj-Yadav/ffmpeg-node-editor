#include <GLFW/glfw3.h>
#include <imgui.h>

#include <string>

class BackendWrapperGlfw3OpenGL3 {
	// Data
	std::string glsl_version;
	GLFWwindow* window;
	ImGuiContext* ctx;

   public:
	bool InitWindow(ImGuiConfigFlags flags);
	void Setup();
	bool IsNewFrameAvailable();
	void Render(const ImVec4& clear_color);
	void Shutdown();
};
