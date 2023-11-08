#pragma once

#include <absl/strings/string_view.h>

#include <memory>

#include "ffmpeg/filter_node.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS

#include <imgui-node-editor/imgui_node_editor.h>
#include <imgui.h>

#include "ffmpeg/filter_graph.hpp"
#include "ffmpeg/profile.hpp"

class NodeEditor {
	std::shared_ptr<ax::NodeEditor::EditorContext> context;
	FilterGraph g;
	const Profile* profile;

	bool searchStarted = false;
	ImGuiTextFilter searchFilter;

	absl::string_view popup;
	std::string popupString;
	NodeId activeNode;

	void drawNode(const FilterNode& node, const NodeId& id);
	void searchBar();
	void popups();

	std::string name;

   public:
	NodeEditor(const Profile* p = nullptr, const std::string& n = "Untitled");
	NodeEditor(
		FilterGraph& g, const std::string& n, const Profile* p = nullptr);
	~NodeEditor();
	void draw();
	void addNode(const Filter& filter) { g.addNode(filter); }
	void play(const NodeId& id = INVALID_NODE);
	bool save(const std::filesystem::path& path) const;
};

bool load(
	const std::filesystem::path& path, const Profile& profile,
	std::vector<NodeEditor>& editors);
