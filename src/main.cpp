#include <fmt/core.h>
#include <imgui.h>
#include <imnodes.h>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "backend.hpp"
#include "ffmpeg/profile.hpp"
#include "file_utils.hpp"
#include "node_editor.hpp"
#include "pref.hpp"
#include "util.hpp"

enum MenuAction {
	MenuActionNone = 0,
	MenuActionNew,
	MenuActionOpen,
	MenuActionSave,
	MenuActionPreference,
};

class Application {
	Preference pref;
	Profile profile;
	BackendWrapperGlfw3OpenGL3 backend;
	ImNodesContext* ctx;
	std::vector<NodeEditor> editors;
	int untitledCount = 0;
	int focusedEditor = -1;

	void openEditor() {
		if (auto path = openFile(); path.has_value()) {
			auto itr = std::find_if(
				editors.begin(), editors.end(), [&](const auto& e) {
					if (e.getPath().empty()) { return false; }
					return std::filesystem::equivalent(
						e.getPath(), path.value());
				});
			if (itr == editors.end()) {
				NodeEditor e(profile, "");
				if (e.load(path.value())) { editors.push_back(e); }
			} else {
				ImGui::SetWindowFocus(itr->getName().c_str());
			}
		} else {
			SPDLOG_DEBUG("User pressed cancel");
		}
	}

	void saveEditor() {
		if (focusedEditor != -1) {
			auto& editor = editors[focusedEditor];
			if (!editor.getPath().empty()) {
				editor.save(editor.getPath());
			} else {
				if (auto path = saveFile(); path.has_value()) {
					editor.save(path.value());
					editor.setPath(path.value());
				} else {
					SPDLOG_DEBUG("User pressed cancel");
				}
			}
		} else {
			showErrorMessage("No Active Editor", "No Editor is active");
		}
	}

	void handleMenu() {
		auto menuAction = static_cast<MenuAction>(backend.DrawMenu());
		switch (menuAction) {
			case MenuActionNew:
				editors.emplace_back(
					profile, fmt::format("Untitled {}", untitledCount));
				untitledCount++;
				return;
			case MenuActionOpen:
				openEditor();
				return;

			case MenuActionSave:
				saveEditor();
				return;

			case MenuActionPreference:
				pref.show = !pref.show;
				return;

			case MenuActionNone: {
			}
		}
	}

   public:
	Application() : profile(GetProfile()), backend() {
		if (!backend.InitWindow(ImGuiConfigFlags_NavEnableKeyboard, pref)) {
			throw std::runtime_error("Unable to initialize window");
		}
		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer backends
		backend.Setup();
		backend.AddMenu(
			"File",
			{
				{"New", MenuActionNew, ImGuiKey_N, true},
				{"Open..", MenuActionOpen, ImGuiKey_O, true},
				{"Save", MenuActionSave, ImGuiKey_S, true},
				{"Preferences", MenuActionPreference, ImGuiKey_Comma, true},
			});

		ctx = ImNodes::CreateContext();

		pref.setOptions();
	}

	Application(const Application&) = delete;
	Application(Application&&) = delete;
	Application& operator=(const Application&) = delete;
	Application& operator=(Application&&) = delete;

	~Application() {
		// Cleanup
		ImNodes::DestroyContext(ctx);
		backend.Shutdown();
	}

	void main() {
		while (backend.IsNewFrameAvailable()) {
			handleMenu();

			focusedEditor = -1;
			for (auto i = 0; i < editors.size(); ++i) {
				if (editors[i].draw(pref)) { focusedEditor = i; }
			}

			if (pref.draw()) { pref.setOptions(); }

			constexpr ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
			backend.Render(clear_color);
		}
	}
};

int main() {
	std::set_terminate([]() {
		auto exception = std::current_exception();
		try {
			if (exception) { std::rethrow_exception(exception); }
		} catch (const std::exception& e) {
			SPDLOG_CRITICAL("Unhandled exception {}\n", e.what());
		}
		std::abort();
	});

	Application app;

	app.main();

	return 0;
}
