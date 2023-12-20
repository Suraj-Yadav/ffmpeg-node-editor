#include "node_editor.hpp"

#include <fmt/format.h>
#include <imgui-node-editor/imgui_node_editor.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include <range/v3/view.hpp>

#include "ffmpeg/profile.hpp"
#include "imgui_extras.hpp"
#include "util.hpp"

#define IM_COL(R, G, B) IM_COL32(R, G, B, 255)

namespace views = ranges::views;

using namespace ImGui;
namespace ed = ax::NodeEditor;

NodeEditor::NodeEditor(const Profile* p, const std::string& n)
	: profile(p), popup(""), name(n) {
	ed::Config config;
	config.SettingsFile = nullptr;
	context = std::shared_ptr<ed::EditorContext>(
		ed::CreateEditor(&config), ed::DestroyEditor);
}

NodeEditor::~NodeEditor() {}

const auto POPUP_MISSING_INPUT = "Missing Input";
const auto POPUP_PREVIEW_ERROR = "ffmpeg Error";
const auto POPUP_NODE_OPTIONS = "Node Options";

#define PLAY_BUTTON_ID(X) int(((X) << 4) + 1)
#define OPT_BUTTON_ID(X)  int(((X) << 4) + 2)
#define OPT_ID(X)		  int(((X) << 4) + 3)

void NodeEditor::play(const NodeId& id) {
	bool invalid = false;
	fmt::memory_buffer buff;
	std::vector<std::string> inputs;
	std::map<IdBaseType, std::string> inputSocketNames;
	std::map<IdBaseType, std::string> outputSocketNames;
	g.iterateNodes(
		[&](const FilterNode& node, const NodeId& id) {
			auto idx = inputs.size();
			if (invalid) { return; }
			auto isInput = node.base().name == INPUT_FILTER_NAME;
			g.inputSockets(
				id, [&](const Socket& s, const NodeId& sId,
						const NodeId& parentSocketId) {
					if (invalid) { return; }
					if (parentSocketId == INVALID_NODE) {
						invalid = true;
						popup = POPUP_MISSING_INPUT;
						popupString = fmt::format(
							"Socket {} of node {} needs an input", s.name,
							node.name);
						return;
					}
					outputSocketNames.erase(parentSocketId.val);
					fmt::format_to(
						std::back_inserter(buff), "{}",
						inputSocketNames[parentSocketId.val]);
				});
			if (invalid) { return; }
			if (isInput) {
				inputs.push_back(node.option.at(0));
			} else {
				fmt::format_to(
					std::back_inserter(buff), "{}@{}{}", node.name, node.name,
					id.val);

				const auto& options = node.base().options;
				for (const auto& [i, opt] : views::enumerate(node.option)) {
					const auto& optId = opt.first;
					const auto& optValue = opt.second;
					if (i == 0) {
						buff.push_back('=');
					} else {
						buff.push_back(':');
					}
					fmt::format_to(
						std::back_inserter(buff), "{}={}",
						options[optId].name.data(), optValue);
				}
			}

			auto socketIdx = 0;
			g.outputSockets(id, [&](const Socket&, const NodeId& socketId) {
				if (isInput) {
					// outputSocketNames[socketId.val] =
					// 	fmt::format("{}:{}", idx, socketIdx);
					inputSocketNames[socketId.val] =
						fmt::format("[{}:{}]", idx, socketIdx);
					socketIdx++;
					return;
				} else {
					inputSocketNames[socketId.val] =
						fmt::format("[s{}]", socketId.val);
					outputSocketNames[socketId.val] =
						fmt::format("[s{}]", socketId.val);
					fmt::format_to(
						std::back_inserter(buff),
						inputSocketNames[socketId.val]);
				}
			});
			if (!isInput) { fmt::format_to(std::back_inserter(buff), ";"); }
		},
		NodeIterOrder::Topological, id);
	if (!invalid) {
		std::vector<std::string> out;
		for (auto& e : outputSocketNames) {
			out.push_back(fmt::format("{}", e.second));
		}
		SPDLOG_INFO(fmt::to_string(buff));
		int status = 0;
		std::tie(status, popupString) =
			profile->runner.play(inputs, fmt::to_string(buff), out);
		if (status != 0) { popup = POPUP_PREVIEW_ERROR; }
	}
}

void NodeEditor::optHook(
	const NodeId& id, const int& optId, const std::string& value) {
	g.optHook(profile, id, optId, value);
}

void NodeEditor::drawNode(const FilterNode& node, const NodeId& id) {
	const auto nodeId = id.val;
	ed::BeginNode(nodeId);

	const auto& style = ed::GetStyle();
	const auto lPad = style.NodePadding.x;
	const auto rPad = style.NodePadding.z;
	const auto borderW = style.NodeBorderWidth;
	const float pinRadius = 5;
	const auto nSize = ed::GetNodeSize(nodeId);
	const auto nPos = ed::GetNodePosition(nodeId);

	BeginVertical(&node);

	BeginHorizontal(&nodeId);
	Spring();
	Text("%s", node.name.data());
	Spring();
	EndHorizontal();

	std::vector<std::pair<ImVec2, ImColor>> pins;

#define DRAW_INPUT_SOCKET(SOCKET, ID)                             \
	ed::BeginPin((ID), ed::PinKind::Input);                       \
	Text("%s", (SOCKET).name.data());                             \
	pins.emplace_back(                                            \
		GetItemRectPoint(0, 0.5) - ImVec2(lPad - borderW / 2, 0), \
		IM_COL(255, 0, 0));                                       \
	ed::PinRect(                                                  \
		pins.back().first - ImVec2(pinRadius, pinRadius),         \
		pins.back().first + ImVec2(pinRadius, pinRadius));        \
	ed::EndPin();

#define DRAW_OUTPUT_SOCKET(SOCKET, ID)                            \
	ed::BeginPin((ID), ed::PinKind::Output);                      \
	Text("%s", (SOCKET).name.data());                             \
	pins.emplace_back(                                            \
		GetItemRectPoint(1, 0.5) + ImVec2(rPad + borderW / 2, 0), \
		IM_COL(0, 255, 0));                                       \
	ed::PinRect(                                                  \
		pins.back().first - ImVec2(pinRadius, pinRadius),         \
		pins.back().first + ImVec2(pinRadius, pinRadius));        \
	ed::EndPin();

	{
		// Both
		for (const auto& [in, inID, out, outID] : views::zip(
				 node.input(), node.inputSocketIds, node.output(),
				 node.outputSocketIds)) {
			BeginHorizontal(&inID.val);
			DRAW_INPUT_SOCKET(in, inID.val);
			Spring();
			DRAW_OUTPUT_SOCKET(out, outID.val);
			EndHorizontal();
		}
		const auto commonCnt =
			std::min(node.input().size(), node.output().size());
		// Only input
		for (const auto& [in, inID] :
			 views::zip(node.input(), node.inputSocketIds) |
				 views::drop(commonCnt)) {
			DRAW_INPUT_SOCKET(in, inID.val);
		}
		// Only output
		for (const auto& [out, outID] :
			 views::zip(node.output(), node.outputSocketIds) |
				 views::drop(commonCnt)) {
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
				BeginHorizontal(optIdx);
				Text("%s", options[optIdx].name.data());
				Spring();
				ed::Suspend();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
					ImGui::SetTooltip("%s", options[optIdx].desc.data());
				}
				ed::Resume();
				PushItemWidth(maxTextWidth);
				PushID(optIdx);
				if (ImGui::InputText(
						"", &optValue, ImGuiInputTextFlags_EnterReturnsTrue)) {
					optHook(id, optIdx, optValue);
				}
				PopID();
				PopItemWidth();
				EndHorizontal();
			}
		}
		PopID();
	}

	EndVertical();
	ed::EndNode();

	const auto list = ed::GetNodeBackgroundDrawList(nodeId);
	ImGui::AddRoundedFilledRect(
		list, nPos + ImVec2(borderW / 2, borderW / 2),
		nPos + ImVec2(nSize.x - borderW / 2, GetFrameHeightWithSpacing()),
		style.NodeRounding, IM_COL(255, 0, 0),
		Corner_TopLeft | Corner_TopRight);

	for (auto& pin : pins) {
		list->AddCircleFilled(pin.first, pinRadius, pin.second);
	}
}

void NodeEditor::popups() {
	if (popup.size() > 0) {
		ImGui::OpenPopup(popup.data());
		popup = {};
	}
	for (auto& p : {POPUP_MISSING_INPUT, POPUP_PREVIEW_ERROR}) {
		if (ImGui::BeginPopupModal(
				p, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("%s", popupString.c_str());
			if (ImGui::Button("Ok")) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
	}
	if (ImGui::BeginPopup(POPUP_NODE_OPTIONS)) {
		auto& node = g.getNode(activeNode);
		if (Selectable("Play till this node")) {
			CloseCurrentPopup();
			play(activeNode);
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
			if (ImGui::BeginMenu("Add options")) {
				if (searchStarted) {
					searchStarted = false;
					ImGui::SetKeyboardFocusHere();
				}
				searchFilter.Draw(" ");
				int count = 0;
				for (const auto& [index, opt] :
					 views::enumerate(node.base().options)) {
					const auto idx = int(index);
					if (contains(node.option, idx)) { continue; }
					if (searchFilter.PassFilter(opt.name.c_str()) ||
						searchFilter.PassFilter(opt.desc.c_str())) {
						count++;
						if (MenuItem(opt.name.c_str())) {
							node.option[idx] = opt.defaultValue;
							optHook(activeNode, idx, opt.defaultValue);
							searchFilter.Clear();
							CloseCurrentPopup();
						}
						ImGui::SameLine();
						ImGui::Text(": %s", opt.desc.c_str());
					}
					if (count >= 5) { break; }
				}
				ImGui::EndMenu();
			}
		}
		ImGui::EndPopup();
	} else {
		activeNode = INVALID_NODE;
	}
}

void NodeEditor::searchBar() {
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
		for (auto& f : profile->filters) {
			if (searchFilter.PassFilter(f.name.c_str())) {
				if (ImGui::Selectable(f.name.c_str())) {
					addNode(f);
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

void NodeEditor::draw() {
	if (ImGui::Begin(getName().c_str())) {
		ed::SetCurrentEditor(context.get());
		ed::Begin(getName().c_str());

		g.iterateNodes([this](const FilterNode& node, const NodeId& id) {
			drawNode(node, id);
		});

		g.iterateLinks([](const LinkId& id, const NodeId& s, const NodeId& d) {
			ed::Link(id.val, s.val, d.val);
		});

		ed::NodeId contextNodeId;
		if (ed::ShowNodeContextMenu(&contextNodeId)) {
			popup = POPUP_NODE_OPTIONS;
			activeNode = {contextNodeId.Get()};
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

static void to_json(nlohmann::json& j, const NodeId& id) { j = id.val; }

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
	g = FilterGraph();
	std::map<int, NodeId> mapping;
	for (const auto& elem : json["nodes"]) {
		auto id = elem["id"].template get<int>();
		auto name = elem["name"].template get<std::string>();
		const auto& base = std::find_if(
			profile->filters.begin(), profile->filters.end(),
			[&](const Filter& filter) { return filter.name == name; });
		auto nId = g.addNode(*base);
		mapping[id] = nId;
		for (const auto& [sNid, sId] : ranges::zip_view(
				 g.getNode(nId).inputSocketIds,
				 elem["inputs"].template get<std::vector<int>>())) {
			mapping[sId] = sNid;
		}
		for (const auto& [sNid, sId] : ranges::zip_view(
				 g.getNode(nId).outputSocketIds,
				 elem["outputs"].template get<std::vector<int>>())) {
			mapping[sId] = sNid;
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
