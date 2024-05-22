#include "node_editor.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "ffmpeg/filter.hpp"
#include "ffmpeg/filter_graph.hpp"
#include "ffmpeg/filter_node.hpp"
#include "ffmpeg/profile.hpp"
#include "file_utils.hpp"
#include "imgui_extras.hpp"
#include "string_utils.hpp"
#include "util.hpp"

NodeEditor::NodeEditor(const Profile& p, const std::string& n) : g(p), name(n) {
	context = std::shared_ptr<ImNodesEditorContext>(
		ImNodes::EditorContextCreate(), ImNodes::EditorContextFree);
}

auto InputTextWithCompletion(
	const char* label, std::string& value,
	const std::vector<AllowedValues>& allowed) {
	using namespace ImGui;
	constexpr auto POPUP_OPTION_COMPLETION = "Completion";

	const bool isInputTextEnterPressed =
		InputText(label, &value, ImGuiInputTextFlags_EnterReturnsTrue);
	const bool isInputTextActive = IsItemActive();
	const bool isInputTextActivated = IsItemActivated();

	if (isInputTextActivated) { OpenPopup(POPUP_OPTION_COMPLETION); }

	SetNextWindowPos(ImVec2(GetItemRectMin().x, GetItemRectMax().y));

	if (BeginPopup(
			POPUP_OPTION_COMPLETION,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize | ImGuiWindowFlags_ChildWindow)) {
		for (const auto& elem : allowed) {
			if (str::contains(elem.value, value, true) ||
				str::contains(elem.desc, value, true)) {
				if (Selectable(elem.value.c_str())) { value = elem.value; }
				SameLine();
				Text(elem.desc);
			}
		}

		if (isInputTextEnterPressed ||
			(!isInputTextActive && !IsWindowFocused())) {
			ImGui::CloseCurrentPopup();
			return true;
		}

		EndPopup();
	}
	return false;
}

bool drawOption(
	const NodeId& id, const Option& option, int optId, std::string& value,
	const float& width) {
	using namespace ImGui;

	bool changed = false;
	BeginHorizontal(option.name.data());
	Text(option.name);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
		ImGui::SetTooltip("%s", option.desc.data());
	}
	Spring();

	PushItemWidth(width);
	PushID(option.name.c_str());
	if (option.allowed.size() > 0) {
		changed = InputTextWithCompletion("", value, option.allowed);
	} else {
		if (str::ends_with(option.name, "fontfile")) {
			changed = InputFont("", value, width);
		} else if (
			str::ends_with(option.name, "path", true) ||
			str::ends_with(option.name, "file", true) ||
			str::ends_with(option.name, "filename", true)) {
			changed = InputFile("", value, width);
		} else if (option.type == "boolean") {
			changed = InputCheckbox("", value, width);
		} else if (option.type == "color") {
			changed = InputColor("", value, width);
		} else {
			changed = ImGui::InputText("", &value);
		}
	}
	PopID();
	PopItemWidth();
	EndHorizontal();
	return changed;
}

void drawAttribute(
	const Style& style, bool isInput, const Socket& skt, const IdBaseType& id,
	const float& indent = 0) {
	ImU32 col = 0;
	switch (skt.type) {
		case SocketType::Video:
			col = style.colors.at(StyleColor::VideoSocket);
			break;
		case SocketType::Audio:
			col = style.colors.at(StyleColor::AudioSocket);
			break;
		case SocketType::Subtitle:
			col = style.colors.at(StyleColor::SubtitleSocket);
			break;
	}
	ImNodes::PushColorStyle(ImNodesCol_Pin, col);
	if (isInput) {
		ImNodes::BeginInputAttribute(id);
	} else {
		ImNodes::BeginOutputAttribute(id);
	}
	ImGui::Text(skt.name);
	if (isInput) {
		ImNodes::EndInputAttribute();
	} else {
		ImNodes::EndOutputAttribute();
	}
	ImNodes::PopColorStyle();
}

void NodeEditor::drawNode(
	const Style& style, const FilterNode& node, const NodeId& id) {
	using namespace ImGui;

	PushID(&node);

	const auto nodeId = id.val;
	ImNodes::BeginNode(nodeId);

	BeginVertical(&node);

	{
		// Suspend and resume needed as title
		// bar adds to layout width
		SuspendLayout();
		ImNodes::BeginNodeTitleBar();
		ResumeLayout();
		BeginHorizontal(&nodeId);
		Spring();
		Text(node.name);
		Spring();
		EndHorizontal();
		SuspendLayout();
		ImNodes::EndNodeTitleBar();
		ResumeLayout();
	}

	std::vector<std::pair<ImVec2, ImColor>> pins;

	{
		// Both
		const auto limit = std::min(node.input().size(), node.output().size());
		for (auto i = 0u; i < limit; ++i) {
			const auto& in = node.input()[i];
			const auto& inID = node.inputSocketIds[i];
			const auto& out = node.output()[i];
			const auto& outID = node.outputSocketIds[i];
			BeginHorizontal(&inID.val);
			drawAttribute(style, true, in, inID.val);
			Spring();
			drawAttribute(style, false, out, outID.val);
			EndHorizontal();
		}
		// Only input
		for (auto i = limit; i < node.input().size(); ++i) {
			const auto& in = node.input()[i];
			const auto& inID = node.inputSocketIds[i];
			drawAttribute(style, true, in, inID.val);
		}
		// Only output
		for (auto i = limit; i < node.output().size(); ++i) {
			const auto& out = node.output()[i];
			const auto& outID = node.outputSocketIds[i];
			BeginHorizontal(&outID.val);
			Spring();
			drawAttribute(style, false, out, outID.val);
			EndHorizontal();
		}
	}

	if (node.option.size() > 0) {
		PushID(&node.option);
		{
			const auto& options = node.base().options;
			auto maxTextWidth = ImGui::CalcTextSize(node.name.c_str()).x;
			for (const auto& [idx, _] : node.option) {
				maxTextWidth = std::max(
					maxTextWidth,
					ImGui::CalcTextSize(options[idx].name.c_str()).x);
			}
			for (auto& [optIdx, optValue] : g.getNode(id).option) {
				if (drawOption(
						id, options[optIdx], optIdx, optValue, maxTextWidth)) {
					g.optHook(id, optIdx, optValue);
				}
			}
		}
		PopID();
	}

	EndVertical();
	ImNodes::EndNode();

	PopID();
}
constexpr auto SEARCH_LIMIT = 5;

void handleNodeAddition(
	FilterGraph& g, bool& searchStarted, ImGuiTextFilter& searchFilter) {
	constexpr auto POP_UP_ID = "add_node_popup";
	constexpr auto PADDING = 8.0f;

	const auto& io = ImGui::GetIO();

	const bool open_popup =
		ImGui::IsKeyReleased(ImGuiKey_Space) && !io.WantTextInput;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(PADDING, PADDING));
	if (!ImGui::IsAnyItemHovered() && open_popup) {
		ImGui::OpenPopup(POP_UP_ID);
		searchStarted = true;
	}
	if (ImGui::BeginPopup(POP_UP_ID)) {
		int count = 0;
		if (searchStarted) {
			searchFilter.Clear();
			searchStarted = false;
			ImGui::SetKeyboardFocusHere();
		}
		searchFilter.Draw(" ");
		for (const auto& f : g.allFilters()) {
			if (searchFilter.PassFilter(f.name.c_str())) {
				if (ImGui::Selectable(f.name.c_str())) {
					g.addNode(f);
					searchFilter.Clear();
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				ImGui::Text("- %s", f.desc.c_str());
				if (count++ == SEARCH_LIMIT) { break; }
			}
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();
}

void handleNodeDeletion(FilterGraph& g) {
	const auto& io = ImGui::GetIO();
	auto deleteSomething =
		ImGui::IsKeyReleased(ImGuiKey_X) && !io.WantTextInput;
	if (!deleteSomething) { return; }

	std::vector<int> selected;
	selected.resize(ImNodes::NumSelectedNodes(), 0);
	if (!selected.empty()) {
		ImNodes::GetSelectedNodes(selected.data());
		for (auto& id : selected) { g.deleteNode({id}); }
	}

	selected.resize(ImNodes::NumSelectedLinks(), 0);
	if (!selected.empty()) {
		ImNodes::GetSelectedLinks(selected.data());
		for (auto& id : selected) { g.deleteLink({id}); }
	}
}

void drawNodeOptions(
	FilterGraph& g, FilterNode& node, bool& searchStarted,
	ImGuiTextFilter& searchFilter, NodeId& selectedNodeId) {
	if (ImGui::BeginMenu("Add options")) {
		if (searchStarted) {
			searchFilter.Clear();
			searchStarted = false;
			ImGui::SetKeyboardFocusHere();
		}
		searchFilter.Draw(" ");
		int count = 0;
		int idx = -1;
		for (const auto& opt : node.base().options) {
			idx++;
			if (contains(node.option, idx)) { continue; }
			if (searchFilter.PassFilter(opt.name.c_str()) ||
				searchFilter.PassFilter(opt.desc.c_str())) {
				count++;
				if (ImGui::MenuItem(opt.name.c_str())) {
					node.option[idx] = opt.defaultValue;
					g.optHook(selectedNodeId, idx, opt.defaultValue);
					searchFilter.Clear();
					selectedNodeId = INVALID_NODE;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				ImGui::Text(opt.desc);
			}
			if (count >= SEARCH_LIMIT) { break; }
		}
		ImGui::EndMenu();
	}
}

void handleNodeOptions(
	FilterGraph& g, NodeId& selectedNodeId, const Preference& pref,
	bool& searchStarted, ImGuiTextFilter& searchFilter) {
	constexpr auto POPUP_NODE_OPTIONS = "Node Options";
	int hoveredId = INVALID_NODE.val;
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
		ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
		ImNodes::IsNodeHovered(&hoveredId)) {
		selectedNodeId = {hoveredId};
		ImGui::OpenPopup(POPUP_NODE_OPTIONS);
	}

	if (ImGui::BeginPopup(POPUP_NODE_OPTIONS)) {
		auto& node = g.getNode(selectedNodeId);
		if (ImGui::Selectable("Play till this node")) {
			// selectedNodeId = INVALID_NODE;
			ImGui::CloseCurrentPopup();
			auto err = g.play(pref, selectedNodeId);
			if (err.code == FilterGraphErrorCode::PLAYER_MISSING_INPUT) {
				showErrorMessage("Missing Input", err.message);
			} else if (err.code == FilterGraphErrorCode::PLAYER_RUNTIME) {
				showErrorMessage("ffmpeg Error", err.message);
			}
		}

		if (node.option.size() < node.base().options.size()) {
			drawNodeOptions(
				g, node, searchStarted, searchFilter, selectedNodeId);
		}
		ImGui::EndPopup();
	}
}

void handleLinks(FilterGraph& g) {
	int pin1 = 0, pin2 = 0;
	if (ImNodes::IsLinkCreated(&pin1, &pin2)) {
		if (g.canAddLink(
				{static_cast<IdBaseType>(pin1)},
				{static_cast<IdBaseType>(pin2)})) {
			g.addLink(
				{static_cast<IdBaseType>(pin1)},
				{static_cast<IdBaseType>(pin2)});
		}
	}

	int deletedLinkId = 0;
	if (ImNodes::IsLinkDestroyed(&deletedLinkId)) {
		g.deleteLink(LinkId{deletedLinkId});
	}
}

void NodeEditor::handleEdits(const Preference& pref) {
	handleNodeAddition(g, searchStarted, searchFilter);
	handleNodeDeletion(g);
	handleNodeOptions(g, selectedNodeId, pref, searchStarted, searchFilter);
	handleLinks(g);
}

bool NodeEditor::draw(const Preference& pref) {
	auto focused = false;
	if (ImGui::Begin(getName().c_str())) {
		focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
		ImNodes::EditorContextSet(context.get());
		ImNodes::BeginNodeEditor();

		g.iterateNodes([&](const FilterNode& node, const NodeId& id) {
			drawNode(pref.style, node, id);
		});

		g.iterateLinks([](const LinkId& id, const NodeId& s, const NodeId& d) {
			ImNodes::Link(id.val, s.val, d.val);
		});

		ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomRight);
		ImNodes::EndNodeEditor();

		if (focused) { handleEdits(pref); }

		ImNodes::EditorContextSet(nullptr);
	}
	ImGui::End();
	return focused;
}

namespace nlohmann {
	template <> struct adl_serializer<NodeId> {
		static void to_json(nlohmann::json& j, const NodeId& id) { j = id.val; }
	};
}  // namespace nlohmann

bool NodeEditor::save(const std::filesystem::path& path) const {
	nlohmann::json obj;
	obj["nodes"] = nlohmann::json::array();
	g.iterateNodes([&](const FilterNode& node, const NodeId& id) {
		nlohmann::json elem;
		elem["id"] = id;
		elem["name"] = node.name;
		const auto& options = node.base().options;
		for (const auto& [optIdx, optValue] : node.option) {
			nlohmann::json opt;
			opt["key"] = options[optIdx].name;
			opt["value"] = optValue;
			elem["option"].push_back(opt);
		}
		elem["inputs"] = node.inputSocketIds;
		elem["outputs"] = node.outputSocketIds;
		g.inputSockets(
			id, [&](const Socket&, const NodeId& dest, const NodeId& src) {
				elem["edges"].push_back({{"src", src}, {"dest", dest}});
			});
		obj["nodes"].push_back(elem);
	});
	std::ofstream o(path, std::ios_base::binary);
	o << std::setw(4) << obj;
	return true;
}

bool NodeEditor::load(const std::filesystem::path& path) {
	nlohmann::json json;
	try {
		json = nlohmann::json::parse(std::ifstream(path));
	} catch (nlohmann::json::exception&) { return false; }
	g.clear();
	std::map<int, NodeId> mapping;
	for (const auto& elem : json["nodes"]) {
		auto id = elem["id"].template get<int>();
		auto name = elem["name"].template get<std::string>();
		const auto& base = std::find_if(
			g.allFilters().begin(), g.allFilters().end(),
			[&](const Filter& filter) { return filter.name == name; });
		auto nId = g.addNode(*base);
		mapping[id] = nId;

		{
			const auto& sockets = g.getNode(nId).inputSocketIds;
			const auto& ints = elem["inputs"].template get<std::vector<int>>();
			const auto limit = std::min(sockets.size(), ints.size());
			for (auto i = 0u; i < limit; ++i) { mapping[ints[i]] = sockets[i]; }
		}
		{
			const auto& sockets = g.getNode(nId).outputSocketIds;
			const auto& ints = elem["outputs"].template get<std::vector<int>>();
			const auto limit = std::min(sockets.size(), ints.size());
			for (auto i = 0u; i < limit; ++i) { mapping[ints[i]] = sockets[i]; }
		}
		if (elem.find("edges") != elem.end()) {
			for (const auto& edge : elem["edges"]) {
				const auto& src = edge["src"].template get<int>();
				const auto& dest = edge["dest"].template get<int>();
				if (src != INVALID_NODE.val) {
					g.addLink(mapping[src], mapping[dest]);
				}
			}
		}
	}
	this->path = std::filesystem::absolute(path);
	name = path.filename().string();
	return true;
}

std::string NodeEditor::getName() const {
	if (!path.empty()) { return path.filename().string(); }
	return name;
}
