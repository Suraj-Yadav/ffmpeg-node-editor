#include "node_editor.hpp"

#include <fmt/core.h>
#include <imgui.h>
#include <vcruntime.h>

#include <functional>
#include <iostream>
#include <ostream>
#include <string>

#include "ffmpeg/filter_graph.hpp"
#include "ffmpeg/filter_node.hpp"
#include "imgui_extras.hpp"

#define IM_COL(R, G, B) IM_COL32(R, G, B, 255)

using namespace ImGui;
namespace ed = ax::NodeEditor;

NodeEditor::NodeEditor(Profile* p) : profile(p) {
	ed::Config config;
	config.SettingsFile = nullptr;
	context = ed::CreateEditor(&config);
}

NodeEditor::~NodeEditor() { ed::DestroyEditor(context); }

void NodeEditor::drawNode(const FilterNode& node) {
	const auto nodeId = node.id.val;

	ed::BeginNode(nodeId);

	const auto& style = ed::GetStyle();
	const auto lPad = style.NodePadding.x;
	const auto rPad = style.NodePadding.z;
	const auto borderW = style.NodeBorderWidth;
	const float pinRadius = 5;
	const auto nSize = ed::GetNodeSize(nodeId);
	const auto nPos = ed::GetNodePosition(nodeId);

	PushRightEdge(nSize.x - lPad - rPad - 2 * borderW);

	AlignedText(node.name, TextAlign::AlignCenter);
	SameLine();

	PushStyleColor(ImGuiCol_Text, IM_COL(0, 255, 0));
	PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);
	PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32_BLACK_TRANS);
	PushStyleColor(ImGuiCol_ButtonActive, IM_COL32_BLACK_TRANS);
	if (ArrowButton(fmt::format("{}", nodeId).c_str(), ImGuiDir_Right)) {
		g.iterateNodes(
			[](const FilterNode& node, const size_t u) {
				for (auto& i : node.inputSocketIds) { fmt::print("[s{}]", i); }
				fmt::print("{}@n{}", node.base().name, node.name);
				for (auto& o : node.outputSocketIds) { fmt::print("[s{}]", o); }
				fmt::println(";");
			},
			NodeIterOrder::Topological);
		std::cout << std::endl;
	}
	PopStyleColor(4);

	std::vector<std::pair<ImVec2, ImColor>> pins;

	const auto& ins = node.input();
	const auto& outs = node.output();
	for (auto i = 0u; i < std::max(ins.size(), outs.size()); ++i) {
		if (i < ins.size()) {
			ed::BeginPin(node.inputSocketIds[i], ed::PinKind::Input);
			AlignedText(ins[i].name);
			pins.emplace_back(
				GetItemRectPoint(0, 0.5) - ImVec2(lPad - borderW / 2, 0),
				IM_COL(255, 0, 0));
			ed::PinRect(
				pins.back().first - ImVec2(pinRadius, pinRadius),
				pins.back().first + ImVec2(pinRadius, pinRadius));
			ed::EndPin();
		}
		if (i < outs.size()) {
			if (i < ins.size()) { ImGui::SameLine(); }
			ed::BeginPin(node.outputSocketIds[i], ed::PinKind::Output);
			ImGui::AlignedText(outs[i].name, ImGui::AlignRight);
			pins.emplace_back(
				GetItemRectPoint(1, 0.5) + ImVec2(rPad + borderW / 2, 0),
				IM_COL(0, 255, 0));
			ed::PinRect(
				pins.back().first - ImVec2(pinRadius, pinRadius),
				pins.back().first + ImVec2(pinRadius, pinRadius));
			ed::EndPin();
		}
	}

	PopRightEdge();
	ed::EndNode();

	const auto list = ed::GetNodeBackgroundDrawList(nodeId);
	ImGui::AddRoundedFilledRect(
		list, nPos + ImVec2(borderW / 2, borderW / 2),
		nPos + ImVec2(nSize.x - borderW / 2, GetFrameHeightWithSpacing()),
		style.NodeRounding, ImColor(255, 0, 0),
		Corner_TopLeft | Corner_TopRight);

	for (auto& pin : pins) {
		list->AddCircleFilled(pin.first, pinRadius, pin.second);
	}
}

void NodeEditor::searchBar() {
	const auto POP_UP_ID = "add_node_popup";
	if (!ImGui::IsPopupOpen(POP_UP_ID) &&
		ImGui::IsKeyPressed(ImGuiKey_Space, false) && profile != nullptr) {
		ImGui::OpenPopup(POP_UP_ID);
		searchStarted = true;
	}
	if (ImGui::BeginPopup(POP_UP_ID)) {
		int count = 0;
		if (searchStarted) {
			searchStarted = false;
			ImGui::SetKeyboardFocusHere();
		}
		searchFilter.Draw(" ");
		for (auto& f : profile->filters) {
			if (searchFilter.PassFilter(f.name.c_str())) {
				if (ImGui::Selectable(f.name.c_str())) {
					addNode(f);
					searchFilter.Clear();
				}
				if (count++ == 5) { break; }
			}
		}
		ImGui::EndPopup();
	}
}

void NodeEditor::draw() {
	ImGui::Begin(g.getName().c_str());
	ed::SetCurrentEditor(context);
	ed::Begin(g.getName().c_str());

	g.iterateNodes(
		[this](const FilterNode& node, const size_t&) { drawNode(node); });

	g.iterateLinks([](const LinkId& id, const size_t& s, const size_t& d) {
		ed::Link(id.val, s, d);
	});

	if (ed::BeginCreate()) {
		ed::PinId inputPinId, outputPinId;
		if (ed::QueryNewLink(&inputPinId, &outputPinId)) {
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
			if (inputPinId && outputPinId) {
				auto pin1 = static_cast<int>(inputPinId.Get());
				auto pin2 = static_cast<int>(outputPinId.Get());
				if (g.canAddLink(pin1, pin2)) {
					// ed::AcceptNewItem() return true when user release
					// mouse button.
					if (ed::AcceptNewItem()) {
						const auto& linkId = g.addLink(pin1, pin2);
						if (linkId != INVALID_LINK) {
							ed::Link(linkId.val, pin1, pin2);
						}
					}
				} else {
					// You may choose to reject connection between these
					// nodes by calling ed::RejectNewItem(). This will
					// allow editor to give visual feedback by changing
					// link thickness and color.
					ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
				}
			}
		}
	}
	ed::EndCreate();  // Wraps up object creation action handling.

	// Handle deletion action
	if (ed::BeginDelete()) {
		// There may be many links marked for deletion, let's loop over
		// them.
		ed::LinkId deletedLinkId;
		while (ed::QueryDeletedLink(&deletedLinkId)) {
			// If you agree that link can be deleted, accept deletion.
			if (ed::AcceptDeletedItem()) {
				// Then remove link from your data.
				g.deleteLink(LinkId{deletedLinkId.Get()});
			}
		}
		ed::NodeId deletedNodeId;
		while (ed::QueryDeletedNode(&deletedNodeId)) {
			if (ed::AcceptDeletedItem(false)) {
				g.deleteNode(NodeId{deletedNodeId.Get()});
			}
		}
	}
	ed::EndDelete();  // Wrap up deletion action

	ed::End();
	ed::SetCurrentEditor(nullptr);

	searchBar();

	ImGui::End();
}
