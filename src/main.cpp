#include <cmath>
#include <string>
#include <vector>

#include "ffmpeg/filter.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS
#include <absl/strings/ascii.h>
#include <imgui-node-editor/imgui_node_editor.h>
#include <imgui.h>

#include <iostream>
#include <ranges>
#include <string_view>

#include "backend.hpp"
#include "ffmpeg/filter_node.hpp"
#include "ffmpeg/profile.hpp"
#include "imgui_extras.hpp"

namespace ed = ax::NodeEditor;

int id = 0;

class NodeEditor {
	ed::EditorContext* context = nullptr;
	std::vector<FilterNode> nodes;
	std::string name;
	int id;

	void drawNode(const FilterNode& node) {
		using namespace ImGui;

		const auto nodeId = id;

		ed::BeginNode(id);

		const auto& style = ed::GetStyle();
		const auto lPad = style.NodePadding.x;
		const auto rPad = style.NodePadding.z;
		const auto borderW = style.NodeBorderWidth;
		const float pinRadius = 5;
		const auto nSize = ed::GetNodeSize(nodeId);
		const auto nPos = ed::GetNodePosition(nodeId);

		PushRightEdge(nSize.x - lPad - rPad - 2 * borderW);

		AlignedText(node.ref.name, TextAlign::AlignCenter);

		std::vector<std::pair<ImVec2, ImColor>> pins;

		const auto& ins = node.ref.input;
		const auto& outs = node.ref.output;
		for (auto i = 0u; i < std::max(ins.size(), outs.size()); ++i) {
			if (i < ins.size()) {
				ed::BeginPin(id++, ed::PinKind::Input);
				AlignedText(ins[i].name);
				pins.emplace_back(
					GetItemRectPoint(0, 0.5) - ImVec2(lPad - borderW / 2, 0),
					IM_COL32(255, 0, 0, 255));
				ed::PinRect(
					pins.back().first - ImVec2(pinRadius, pinRadius),
					pins.back().first + ImVec2(pinRadius, pinRadius));
				ed::EndPin();
			}
			if (i < outs.size()) {
				if (i < ins.size()) { ImGui::SameLine(); }
				ed::BeginPin(id++, ed::PinKind::Output);
				ImGui::AlignedText(outs[i].name, ImGui::AlignRight);
				pins.emplace_back(
					GetItemRectPoint(1, 0.5) + ImVec2(rPad - borderW / 2, 0),
					IM_COL32(0, 255, 0, 255));
				ed::PinRect(
					pins.back().first - ImVec2(pinRadius, pinRadius),
					pins.back().first + ImVec2(pinRadius, pinRadius));
				ed::EndPin();
			}
		}
		// 	// auto p = ImVec2(
		// 	// 	ImGui::GetItemRectMin().x - editorStyle.NodePadding.x,
		// 	// 	(ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) /
		// 2);
		// ed::BeginPin(nodeB_InputPinId1, ed::PinKind::Input);
		// 	// const int radius = 5;
		// 	// ed::PinRect(p - ImVec2(radius, radius), p + ImVec2(radius,
		// 	// radius));
		// 	ed::EndPin();

		// 	ed::BeginPin(nodeB_OutputPinId, ed::PinKind::Output);
		// 	ImGui::Text("Out ->");
		// 	ed::EndPin();

		// 	ImGui::TableNextRow();
		// 	ImGui::TableSetColumnIndex(0);
		// 	ed::BeginPin(nodeB_InputPinId2, ed::PinKind::Input);
		// 	ImGui::Text("-> In2");
		// 	ed::EndPin();
		PopRightEdge();
		ed::EndNode();
		const auto list = ed::GetNodeBackgroundDrawList(nodeId);

		ImGui::AddRoundedFilledRect(
			list, nPos + ImVec2(borderW / 2, borderW / 2),
			nPos + ImVec2(nSize.x - borderW / 2, GetFrameHeightWithSpacing()),
			style.NodeRounding, ImColor(255, 0, 0),
			Corner_TopLeft | Corner_TopRight);

		for (auto& pin : pins) {
			// ImGui::GetWindowDrawList()->AddCircleFilled(
			list->AddCircleFilled(pin.first, pinRadius, pin.second);
		}
		id++;
	}

   public:
	NodeEditor(const std::string& n = "Untitled") : name(n) {
		ed::Config config;
		config.SettingsFile = nullptr;
		context = ed::CreateEditor(&config);
	}
	~NodeEditor() { ed::DestroyEditor(context); }
	void draw() {
		ImGui::Begin(name.c_str());
		ed::SetCurrentEditor(context);
		ed::Begin(name.c_str());

		id = 1;
		for (auto& node : nodes) { drawNode(node); }

		if (ed::BeginCreate()) {
			ed::PinId inputPinId, outputPinId;
			if (ed::QueryNewLink(&inputPinId, &outputPinId)) {
				std::cout << inputPinId.Get() << "->" << outputPinId.Get()
						  << std::endl;
				// QueryNewLink returns true if editor want to create new
				// link between pins.
				//
				// Link can be created only for two valid pins, it is up to
				// you to validate if connection make sense. Editor is happy
				// to make any.
				//
				// Link always goes from input to output. User may choose to
				// drag link from output pin or input pin. This determine
				// which pin ids are valid and which are not:
				//   * input valid, output invalid - user started to drag
				//   new ling from input pin
				//   * input invalid, output valid - user started to drag
				//   new ling from output pin
				//   * input valid, output valid   - user dragged link over
				//   other pin, can be validated

				// if (inputPinId &&
				// 	outputPinId)  // both are valid, let's accept link
				// {
				// 	// ed::AcceptNewItem() return true when user release
				// 	// mouse button.
				// 	if (ed::AcceptNewItem()) {
				// 		// Since we accepted new link, lets add one to our
				// 		// list of links.
				// 		m_Links.push_back(
				// 			{ed::LinkId(m_NextLinkId++), inputPinId,
				// 			 outputPinId});

				// 		// Draw new link.
				// 		ed::Link(
				// 			m_Links.back().Id, m_Links.back().InputId,
				// 			m_Links.back().OutputId);
				// 	}

				// 	// You may choose to reject connection between these
				// 	// nodes by calling ed::RejectNewItem(). This will allow
				// 	// editor to give visual feedback by changing link
				// 	// thickness and color.
				// }
			}
		}
		ed::EndCreate();  // Wraps up object creation action handling.

		ed::End();
		ed::SetCurrentEditor(nullptr);
		ImGui::End();
	}
	void addNode(const Filter& filter) { nodes.emplace_back(filter); }
};

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

	NodeEditor editor("My Editor");

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

		// // Handle deletion action
		// if (ed::BeginDelete()) {
		// 	// There may be many links marked for deletion, let's loop over
		// 	// them.
		// 	ed::LinkId deletedLinkId;
		// 	while (ed::QueryDeletedLink(&deletedLinkId)) {
		// 		// If you agree that link can be deleted, accept deletion.
		// 		if (ed::AcceptDeletedItem()) {
		// 			// Then remove link from your data.
		// 			for (auto& link : m_Links) {
		// 				if (link.Id == deletedLinkId) {
		// 					m_Links.erase(&link);
		// 					break;
		// 				}
		// 			}
		// 		}

		// 		// You may reject link deletion by calling:
		// 		// ed::RejectDeletedItem();
		// 	}
		// }
		// ed::EndDelete();  // Wrap up deletion action

		// End of interaction with editor.

		// Rendering
		backend.Render(clear_color);
	}

	// Cleanup
	backend.Shutdown();

	return 0;
}
