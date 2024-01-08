#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include <absl/strings/string_view.h>
#include <imgui-node-editor/imgui_node_editor.h>
#include <imgui.h>

#include <filesystem>
#include <memory>

#include "ffmpeg/filter_graph.hpp"
#include "pref.hpp"

class FilterNode;
class Profile;

struct Popup {
	absl::string_view type;
	std::string msg;
	NodeId nodeId;
	int optId;
	bool isInputTextActive;
	bool isInputTextEnterPressed;
	ImVec2 position;
};

class NodeEditor {
	FilterGraph g;
	std::shared_ptr<ax::NodeEditor::EditorContext> context;

	bool searchStarted = false;
	ImGuiTextFilter searchFilter;

	Popup popup;

	void drawNode(const Style& style, const FilterNode& node, const NodeId& id);
	void searchBar();
	void popups(const Preference& pref);

	std::string name;
	std::filesystem::path path;

   public:
	NodeEditor(const Profile& p, const std::string& n);
	~NodeEditor();
	const std::string getName() const;
	const std::filesystem::path& getPath() const { return path; };
	void setPath(std::filesystem::path& p) { path = p; };
	bool draw(const Preference& pref);
	bool save(const std::filesystem::path& path) const;
	bool load(const std::filesystem::path& path);
};
