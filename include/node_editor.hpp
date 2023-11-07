#pragma once

#include <absl/strings/string_view.h>

#include "ffmpeg/filter_node.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS

#include <imgui-node-editor/imgui_node_editor.h>
#include <imgui.h>

#include "ffmpeg/filter_graph.hpp"
#include "ffmpeg/profile.hpp"

class NodeEditor {
	ax::NodeEditor::EditorContext* context = nullptr;
	FilterGraph g;
	Profile* profile;

	bool searchStarted = false;
	ImGuiTextFilter searchFilter;

	absl::string_view popup;
	std::string popupString;
	NodeId activeNode;

	void drawNode(FilterNode& node, const NodeId& id);
	void searchBar();
	void popups();

   public:
	NodeEditor(Profile* p = nullptr);
	~NodeEditor();
	void draw();
	void addNode(const Filter& filter) { g.addNode(filter); }
	void play(const NodeId& id = INVALID_NODE);
};
