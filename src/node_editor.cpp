#include "node_editor.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "ffmpeg/filter.hpp"
#include "ffmpeg/filter_graph.hpp"
#include "ffmpeg/filter_node.hpp"
#include "ffmpeg/profile.hpp"
#include "imgui_extras.hpp"
#include "string_utils.hpp"
#include "util.hpp"

NodeEditor::NodeEditor(const Profile& p, const std::string& n)
	: g(p), popup({""}), name(n) {
	context = std::shared_ptr<ImNodesEditorContext>(
		ImNodes::EditorContextCreate(), ImNodes::EditorContextFree);
}

NodeEditor::~NodeEditor() {}

const auto POPUP_MISSING_INPUT = "Missing Input";
const auto POPUP_PREVIEW_ERROR = "ffmpeg Error";
const auto POPUP_NODE_OPTIONS = "Node Options";
const auto POPUP_OPTION_COMPLETION = "Completion";
const auto POPUP_OPTION_COLOR = "Color";

#define PLAY_BUTTON_ID(X) int(((X) << 4) + 1)
#define OPT_BUTTON_ID(X)  int(((X) << 4) + 2)
#define OPT_ID(X)		  int(((X) << 4) + 3)

bool drawOption(
	const NodeId& id, const Option& option, int optId, std::string& value,
	const float& width, Popup& popup) {
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
		changed = InputText("", &value, ImGuiInputTextFlags_EnterReturnsTrue);
		if (IsItemActivated()) {
			popup.type = POPUP_OPTION_COMPLETION;
			popup.nodeId = id;
			popup.optId = optId;
			popup.isInputTextActive = IsItemActive();
			popup.isInputTextEnterPressed = changed;
		}
	} else {
		if (str::ends_with(option.name, "fontfile")) {
			changed = InputFont("", value);
		} else if (
			str::ends_with(option.name, "path", true) ||
			str::ends_with(option.name, "file", true) ||
			str::ends_with(option.name, "filename", true)) {
			changed = InputFile("", value);
		} else if (option.type == "boolean") {
			changed = InputCheckbox("", value);
		} else if (option.type == "color") {
			changed = InputColor("", value);
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
	ImU32 col;
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
		PushID(OPT_ID(nodeId));
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
						id, options[optIdx], optIdx, optValue, maxTextWidth,
						popup)) {
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

void NodeEditor::popups(const Preference& pref) {
	using namespace ImGui;

	if (!popup.type.empty()) {
		ImGui::OpenPopup(popup.type.data());
		popup.type = {};
	}
	for (auto& p : {POPUP_MISSING_INPUT, POPUP_PREVIEW_ERROR}) {
		if (BeginPopupModal(p, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			Text(popup.msg);
			if (Button("Ok")) { CloseCurrentPopup(); }
			EndPopup();
		}
	}
	if (BeginPopup(POPUP_NODE_OPTIONS)) {
		auto& node = g.getNode(popup.nodeId);
		if (Selectable("Play till this node")) {
			CloseCurrentPopup();
			auto err = g.play(pref, popup.nodeId);
			if (err.code == FilterGraphErrorCode::PLAYER_MISSING_INPUT) {
				popup.type = POPUP_MISSING_INPUT;
				popup.msg = err.message;
			} else if (err.code == FilterGraphErrorCode::PLAYER_RUNTIME) {
				popup.type = POPUP_PREVIEW_ERROR;
				popup.msg = err.message;
			}
		}
		// SameLine();
		// ImGui::Text("Play till this node");
		// PushStyleColor(ImGuiCol_Text, IM_COL(0, 255, 0));
		// PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);
		// PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32_BLACK_TRANS);
		// PushStyleColor(ImGuiCol_ButtonActive, IM_COL32_BLACK_TRANS);
		// SPDLOG_INFO("GetStyle().ItemSpacing.x: {}",
		// GetStyle().ItemSpacing.x); SameLine(); ArrowButton("",
		// ImGuiDir_Right); PopID(); PopStyleColor(4);

		if (node.option.size() < node.base().options.size()) {
			if (BeginMenu("Add options")) {
				if (searchStarted) {
					searchStarted = false;
					SetKeyboardFocusHere();
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
						if (MenuItem(opt.name.c_str())) {
							node.option[idx] = opt.defaultValue;
							g.optHook(popup.nodeId, idx, opt.defaultValue);
							searchFilter.Clear();
							CloseCurrentPopup();
						}
						SameLine();
						Text(opt.desc);
					}
					if (count >= 5) { break; }
				}
				EndMenu();
			}
		}
		EndPopup();
	} else if (
		SetNextWindowPos(popup.position),
		SetNextWindowSize({0, GetFrameHeightWithSpacing() * 5}),
		BeginPopup(
			POPUP_OPTION_COMPLETION,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize | ImGuiWindowFlags_ChildWindow)) {
		auto& node = g.getNode(popup.nodeId);
		const auto& option = node.base().options[popup.optId];
		auto& value = node.option[popup.optId];

		for (auto& elem : option.allowed) {
			if (str::contains(elem.value, value, true) ||
				str::contains(elem.desc, value, true)) {
				if (Selectable(elem.value.c_str())) {
					node.option[popup.optId] = elem.value;
				}
				SameLine();
				Text(elem.desc);
			}
		}

		if (popup.isInputTextEnterPressed ||
			(!popup.isInputTextActive && !IsWindowFocused()))
			CloseCurrentPopup();

		EndPopup();

		popup.isInputTextActive = false;
		popup.isInputTextEnterPressed = false;
	} else if (BeginPopup(POPUP_OPTION_COLOR)) {
		auto& node = g.getNode(popup.nodeId);
		auto& value = node.option[popup.optId];
		auto color = ColorConvertU32ToFloat4(ColorConvertHexToU32(value));
		if (ColorPicker4(
				"##col", &color.x,
				(pref.style.colorPicker == 0
					 ? ImGuiColorEditFlags_PickerHueBar
					 : ImGuiColorEditFlags_PickerHueWheel) |
					ImGuiColorEditFlags_AlphaBar |
					ImGuiColorEditFlags_AlphaPreviewHalf)) {
			value = ColorConvertU32ToHex(ColorConvertFloat4ToU32(color));
		}
		EndPopup();
	} else {
		popup.nodeId = INVALID_NODE;
	}
}

void NodeEditor::searchBar() {
	using namespace ImGui;

	const auto POP_UP_ID = "add_node_popup";
	if (ImGui::IsWindowHovered() && !ImGui::IsPopupOpen(POP_UP_ID) &&
		ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
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
		for (auto& f : g.allFilters()) {
			if (searchFilter.PassFilter(f.name.c_str())) {
				if (ImGui::Selectable(f.name.c_str())) {
					g.addNode(f);
					searchFilter.Clear();
					CloseCurrentPopup();
				}
				ImGui::SameLine();
				ImGui::Text("- %s", f.desc.c_str());
				if (count++ == 5) { break; }
			}
		}
		ImGui::EndPopup();
	}
}

void NodeEditor::handleNodeAddition() {
	const auto POP_UP_ID = "add_node_popup";

	const bool open_popup =
		ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
		ImNodes::IsEditorHovered() && ImGui::IsKeyReleased(ImGuiKey_Space);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
	if (!ImGui::IsAnyItemHovered() && open_popup) {
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
		for (auto& f : g.allFilters()) {
			if (searchFilter.PassFilter(f.name.c_str())) {
				if (ImGui::Selectable(f.name.c_str())) {
					g.addNode(f);
					searchFilter.Clear();
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				ImGui::Text("- %s", f.desc.c_str());
				if (count++ == 5) { break; }
			}
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();
}

bool NodeEditor::draw(const Preference& pref) {
	auto focused = false;
	if (ImGui::Begin(getName().c_str())) {
		focused = ImGui::IsWindowFocused();
		ImNodes::EditorContextSet(context.get());
		ImNodes::BeginNodeEditor();

		handleNodeAddition();

		ImNodesStyle& style = ImNodes::GetStyle();
		style.Colors[ImNodesCol_TitleBar] =
			pref.style.colors.at(StyleColor::NodeHeader);
		style.PinCircleRadius = 5;

		g.iterateNodes([&](const FilterNode& node, const NodeId& id) {
			drawNode(pref.style, node, id);
		});

		g.iterateLinks([](const LinkId& id, const NodeId& s, const NodeId& d) {
			ImNodes::Link(id.val, s.val, d.val);
		});

		// // Handle deletion action
		// if (ed::BeginDelete()) {
		// 	// There may be many links marked for deletion, let's loop over
		// 	// them.
		// 	ed::NodeId deletedNodeId;
		// 	while (ed::QueryDeletedNode(&deletedNodeId)) {
		// 		if (ed::AcceptDeletedItem(false)) {
		// 			g.deleteNode(NodeId{deletedNodeId.Get()});
		// 		}
		// 	}
		// }
		// ed::EndDelete();  // Wrap up deletion action
		ImNodes::MiniMap(0.2, ImNodesMiniMapLocation_BottomRight);
		ImNodes::EndNodeEditor();

		int hoveredId;
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
			ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
			ImNodes::IsNodeHovered(&hoveredId)) {
			popup.type = POPUP_NODE_OPTIONS;
			popup.nodeId = {static_cast<IdBaseType>(hoveredId)};
		}

		{
			int pin1, pin2;
			if (ImNodes::IsLinkCreated(&pin1, &pin2)) {
				if (g.canAddLink(
						{static_cast<IdBaseType>(pin1)},
						{static_cast<IdBaseType>(pin2)})) {
					g.addLink(
						{static_cast<IdBaseType>(pin1)},
						{static_cast<IdBaseType>(pin2)});
				}
			}
		}

		{
			int deletedLinkId;
			if (ImNodes::IsLinkDestroyed(&deletedLinkId)) {
				g.deleteLink(LinkId{deletedLinkId});
			}
		}

		// ed::End();
		ImNodes::EditorContextSet(nullptr);
		// ed::SetCurrentEditor(nullptr);

		// searchBar();
		popups(pref);
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
		for (auto& [optIdx, optValue] : node.option) {
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
			const auto& skts = g.getNode(nId).inputSocketIds;
			const auto& ints = elem["inputs"].template get<std::vector<int>>();
			const auto limit = std::min(skts.size(), ints.size());
			for (auto i = 0u; i < limit; ++i) { mapping[ints[i]] = skts[i]; }
		}
		{
			const auto& skts = g.getNode(nId).outputSocketIds;
			const auto& ints = elem["outputs"].template get<std::vector<int>>();
			const auto limit = std::min(skts.size(), ints.size());
			for (auto i = 0u; i < limit; ++i) { mapping[ints[i]] = skts[i]; }
		}
		if (elem.find("edges") != elem.end()) {
			for (const auto& edge : elem["edges"]) {
				const auto& src = edge["src"].template get<int>();
				const auto& dest = edge["dest"].template get<int>();
				g.addLink(mapping[src], mapping[dest]);
			}
		}
	}
	this->path = std::filesystem::absolute(path);
	name = path.filename().string();
	return true;
}

const std::string NodeEditor::getName() const {
	if (!path.empty()) { return path.filename().string(); }
	return name;
}
