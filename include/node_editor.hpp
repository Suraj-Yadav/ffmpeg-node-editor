#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include <imgui.h>
#include <imnodes.h>

#include <filesystem>
#include <memory>

#include "ffmpeg/filter_graph.hpp"
#include "pref.hpp"

class FilterNode;
class Profile;

struct Popup {
	std::string_view type;
	std::string msg;
	NodeId nodeId;
	int optId;
	bool isInputTextActive;
	bool isInputTextEnterPressed;
	ImVec2 position;
};

class NodeEditor {
	FilterGraph g;
	std::shared_ptr<ImNodesEditorContext> context;

	bool searchStarted = false;
	ImGuiTextFilter searchFilter;

	NodeId selectedNodeId = INVALID_NODE;

	void drawNode(const Style& style, const FilterNode& node, const NodeId& id);
	void searchBar();
	void handleNodeAddition();

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
