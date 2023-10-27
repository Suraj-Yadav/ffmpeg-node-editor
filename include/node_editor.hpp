#pragma once

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

	void drawNode(const FilterNode& node);
	void searchBar();

   public:
	NodeEditor(Profile* p = nullptr);
	~NodeEditor();
	void draw();
	void addNode(const Filter& filter) { g.addNode(filter); }
};
