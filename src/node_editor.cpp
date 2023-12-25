#include "node_editor.hpp"

#include <absl/strings/match.h>
#include <imgui-node-editor/imgui_node_editor.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "extras/IconsFontAwesome6.h"
#include "ffmpeg/filter.hpp"
#include "ffmpeg/filter_graph.hpp"
#include "ffmpeg/filter_node.hpp"
#include "ffmpeg/profile.hpp"
#include "file_utils.hpp"
#include "imgui_extras.hpp"
#include "util.hpp"

namespace ed = ax::NodeEditor;

Style::Style() {
	using namespace ImGui;

	ed::Style v;
	colors[NodeHeader] = ColorConvertU32ToFloat4(IM_COL32(255, 0, 0, 255));
	colors[NodeBg] = v.Colors[ed::StyleColor_NodeBg];
	colors[Border] = v.Colors[ed::StyleColor_NodeBorder];
	// colors[Wire] = v.Colors[ed::StyleColor_];
	colors[VideoSocket] = ColorConvertU32ToFloat4(IM_COL32(255, 0, 0, 255));
	colors[AudioSocket] = ColorConvertU32ToFloat4(IM_COL32(0, 255, 0, 255));
	colors[SubtitleSocket] = ColorConvertU32ToFloat4(IM_COL32(0, 0, 255, 255));
}

NodeEditor::NodeEditor(const Profile& p, const std::string& n)
	: g(p), popup({""}), name(n) {
	ed::Config config;
	config.SettingsFile = nullptr;
	context = std::shared_ptr<ed::EditorContext>(
		ed::CreateEditor(&config), ed::DestroyEditor);
}

NodeEditor::~NodeEditor() {}

const auto POPUP_MISSING_INPUT = "Missing Input";
const auto POPUP_PREVIEW_ERROR = "ffmpeg Error";
const auto POPUP_NODE_OPTIONS = "Node Options";
const auto POPUP_OPTION_COMPLETION = "Completion";

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
	ed::Suspend();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
		ImGui::SetTooltip("%s", option.desc.data());
	}
	ed::Resume();
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
			popup.position = ed::CanvasToScreen(GetItemRectPoint(0, 1));
		}
	} else {
		changed = ImGui::InputText("", &value);
		if (absl::EndsWith(option.name, "path")) {
			Spring(0, 0);
			if (Button(ICON_FA_FOLDER_OPEN)) {
				auto path = openFile();
				if (path.has_value()) {
					value = path->string();
					changed = true;
				}
			}
		}
	}
	PopID();
	PopItemWidth();
	EndHorizontal();
	return changed;
}

void NodeEditor::drawNode(
	const Style& style, const FilterNode& node, const NodeId& id) {
	using namespace ImGui;

	const auto nodeId = id.val;
	ed::BeginNode(nodeId);

	const auto& nodeStyle = ed::GetStyle();
	const auto lPad = nodeStyle.NodePadding.x;
	const auto rPad = nodeStyle.NodePadding.z;
	const auto borderW = nodeStyle.NodeBorderWidth;
	const float pinRadius = 5;
	const auto nSize = ed::GetNodeSize(nodeId);
	const auto nPos = ed::GetNodePosition(nodeId);

	BeginVertical(&node);

	BeginHorizontal(&nodeId);
	Spring();
	Text(node.name);
	Spring();
	EndHorizontal();

	std::vector<std::pair<ImVec2, ImColor>> pins;

#define DRAW_INPUT_SOCKET(SOCKET, ID)                             \
	ed::BeginPin((ID), ed::PinKind::Input);                       \
	Text((SOCKET).name);                                          \
	pins.emplace_back(                                            \
		GetItemRectPoint(0, 0.5) - ImVec2(lPad - borderW / 2, 0), \
		style.colors[(SOCKET).type + StyleColor::VideoSocket]);   \
	ed::PinRect(                                                  \
		pins.back().first - ImVec2(pinRadius, pinRadius),         \
		pins.back().first + ImVec2(pinRadius, pinRadius));        \
	ed::EndPin();

#define DRAW_OUTPUT_SOCKET(SOCKET, ID)                            \
	ed::BeginPin((ID), ed::PinKind::Output);                      \
	Text((SOCKET).name);                                          \
	pins.emplace_back(                                            \
		GetItemRectPoint(1, 0.5) + ImVec2(rPad + borderW / 2, 0), \
		style.colors[(SOCKET).type + StyleColor::VideoSocket]);   \
	ed::PinRect(                                                  \
		pins.back().first - ImVec2(pinRadius, pinRadius),         \
		pins.back().first + ImVec2(pinRadius, pinRadius));        \
	ed::EndPin();

	{
		// Both
		const auto limit = std::min(node.input().size(), node.output().size());
		for (auto i = 0u; i < limit; ++i) {
			const auto& in = node.input()[i];
			const auto& inID = node.inputSocketIds[i];
			const auto& out = node.output()[i];
			const auto& outID = node.outputSocketIds[i];
			BeginHorizontal(&inID.val);
			DRAW_INPUT_SOCKET(in, inID.val);
			Spring();
			DRAW_OUTPUT_SOCKET(out, outID.val);
			EndHorizontal();
		}
		// Only input
		for (auto i = limit; i < node.input().size(); ++i) {
			const auto& in = node.input()[i];
			const auto& inID = node.inputSocketIds[i];
			DRAW_INPUT_SOCKET(in, inID.val);
		}
		// Only output
		for (auto i = limit; i < node.output().size(); ++i) {
			const auto& out = node.output()[i];
			const auto& outID = node.outputSocketIds[i];
			BeginHorizontal(&outID.val);
			Spring();
			DRAW_OUTPUT_SOCKET(out, outID.val);
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
	ed::EndNode();

	const auto list = ed::GetNodeBackgroundDrawList(nodeId);
	list->AddRectFilled(
		nPos + ImVec2(borderW / 2, borderW / 2),
		nPos + ImVec2(nSize.x - borderW / 2, GetFrameHeightWithSpacing()),
		ImColor(style.colors[StyleColor::NodeHeader]), nodeStyle.NodeRounding,
		ImDrawFlags_RoundCornersTop);

	for (auto& pin : pins) {
		list->AddCircleFilled(pin.first, pinRadius, pin.second);
	}
}

void NodeEditor::popups() {
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
			auto err = g.play(popup.nodeId);
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
			if (absl::StrContainsIgnoreCase(elem.value, value) ||
				absl::StrContainsIgnoreCase(elem.desc, value)) {
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

void NodeEditor::draw(const Style& style) {
	if (ImGui::Begin(getName().c_str())) {
		ed::SetCurrentEditor(context.get());
		ed::Begin(getName().c_str());

		g.iterateNodes([&](const FilterNode& node, const NodeId& id) {
			drawNode(style, node, id);
		});

		g.iterateLinks([](const LinkId& id, const NodeId& s, const NodeId& d) {
			ed::Link(id.val, s.val, d.val);
		});

		ed::NodeId contextNodeId;
		if (ed::ShowNodeContextMenu(&contextNodeId)) {
			popup.type = POPUP_NODE_OPTIONS;
			popup.nodeId = {contextNodeId.Get()};
		}

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
					auto pin1 = inputPinId.Get();
					auto pin2 = outputPinId.Get();
					if (g.canAddLink({pin1}, {pin2})) {
						// ed::AcceptNewItem() return true when user release
						// mouse button.
						if (ed::AcceptNewItem()) {
							const auto& linkId = g.addLink({pin1}, {pin2});
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
		popups();
	}
	ImGui::End();
}

namespace nlohmann {
	template <> struct adl_serializer<NodeId> {
		static void to_json(nlohmann::json& j, const NodeId& id) { j = id.val; }
	};
}  // namespace nlohmann

bool NodeEditor::save(const std::filesystem::path& path) const {
	nlohmann::json obj;
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
const std::filesystem::path& NodeEditor::getPath() const { return path; }
