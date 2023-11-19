#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include <absl/strings/string_view.h>
#include <imgui-node-editor/imgui_node_editor.h>
#include <imgui.h>

#include <filesystem>
#include <memory>

#include "ffmpeg/filter_graph.hpp"

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
	void optHook(const NodeId& id, const int& optId, const std::string& value);

	std::string name;
	std::filesystem::path path;

   public:
	NodeEditor(const Profile* p, const std::string& n);
	~NodeEditor();
	const std::string getName() const;
	const std::filesystem::path& getPath() const;
	void draw();
	void addNode(const Filter& filter) { g.addNode(filter); }
	void play(const NodeId& id = INVALID_NODE);
	bool save(const std::filesystem::path& path) const;
	bool load(const std::filesystem::path& path);
};
