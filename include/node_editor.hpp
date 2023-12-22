#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include <absl/strings/string_view.h>
#include <imgui-node-editor/imgui_node_editor.h>
#include <imgui.h>

#include <filesystem>
#include <memory>

#include "ffmpeg/filter_graph.hpp"

class FilterNode;
class Profile;

enum StyleColor {
	NodeHeader = 0,
	NodeBg,
	Border,
	Wire,
	VideoSocket,
	AudioSocket,
	SubtitleSocket,
	COUNT
};

struct Style {
	ImVec4 colors[StyleColor::COUNT];
	Style();
};

class NodeEditor {
	FilterGraph g;
	std::shared_ptr<ax::NodeEditor::EditorContext> context;

	bool searchStarted = false;
	ImGuiTextFilter searchFilter;

	absl::string_view popup;
	std::string popupString;
	NodeId activeNode;

	void drawNode(const Style& style, const FilterNode& node, const NodeId& id);
	void searchBar();
	void popups();

	std::string name;
	std::filesystem::path path;

   public:
	NodeEditor(const Profile& p, const std::string& n);
	~NodeEditor();
	const std::string getName() const;
	const std::filesystem::path& getPath() const;
	void draw(const Style& style);
	bool save(const std::filesystem::path& path) const;
	bool load(const std::filesystem::path& path);
};
