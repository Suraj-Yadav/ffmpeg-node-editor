#include "node_editor.hpp"

#include <fmt/core.h>
#include <fmt/format.h>
#include <imgui-node-editor/imgui_node_editor.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <spdlog/spdlog.h>
#include <vcruntime.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <ostream>
#include <ranges>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "ffmpeg/filter_graph.hpp"
#include "ffmpeg/filter_node.hpp"
#include "imgui_extras.hpp"
#include "util.hpp"

#define IM_COL(R, G, B) IM_COL32(R, G, B, 255)

using namespace ImGui;
namespace ed = ax::NodeEditor;

NodeEditor::NodeEditor(const Profile* p, const std::string& n)
	: profile(p), popup(""), name(n) {
	ed::Config config;
	config.SettingsFile = nullptr;
	context = std::shared_ptr<ed::EditorContext>(
		ed::CreateEditor(&config), ed::DestroyEditor);
}

NodeEditor::NodeEditor(
	FilterGraph& g, const std::string& n = "Untitled", const Profile* p)
	: g(g), profile(p), popup(""), name(n) {
	ed::Config config;
	config.SettingsFile = nullptr;
	context = std::shared_ptr<ed::EditorContext>(
		ed::CreateEditor(&config), ed::DestroyEditor);
}

NodeEditor::~NodeEditor() {}

const auto POPUP_MISSING_INPUT = "Missing Input";
const auto POPUP_ADD_OPT = "Select Option";

#define PLAY_BUTTON_ID(X) int(((X) << 4) + 1)
#define OPT_BUTTON_ID(X)  int(((X) << 4) + 2)
#define OPT_ID(X)		  int(((X) << 4) + 3)

void NodeEditor::play(const NodeId& id) {
	bool invalid = false;
	fmt::memory_buffer buff;
	std::set<IdBaseType> outputs;
	g.iterateNodes(
		[&](const FilterNode& node, const NodeId& id) {
			if (invalid) { return; }
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
					outputs.erase(parentSocketId.val);
					fmt::format_to(
						std::back_inserter(buff), "[s{}]", parentSocketId.val);
				});
			if (invalid) { return; }
			fmt::format_to(
				std::back_inserter(buff), "{}@{}{}", node.base().name,
				node.name, id.val);
			if (!node.option.empty()) {
				buff.push_back('=');
				bool first = true;
				const auto& options = node.base().options;
				for (const auto& [optId, value] : node.option) {
					if (!first) { buff.push_back(':'); }
					fmt::format_to(
						std::back_inserter(buff), "{}={}",
						options[optId].name.data(), value);
					first = false;
				}
			}
			g.outputSockets(id, [&](const Socket&, const NodeId& socketId) {
				outputs.insert(socketId.val);
				fmt::format_to(std::back_inserter(buff), "[s{}]", socketId.val);
			});
			fmt::format_to(std::back_inserter(buff), ";");
		},
		NodeIterOrder::Topological, id);
	if (!invalid) {
		std::vector<std::string> out;
		for (auto& e : outputs) { out.push_back(std::format("[s{}]", e)); }
		spdlog::info(fmt::to_string(buff));
		profile->runner.play(fmt::to_string(buff), out);
	}
}

void NodeEditor::drawNode(const FilterNode& node, const NodeId& id) {
	const auto nodeId = id.val;
	auto maxTextWidth = 0.0f;
	ed::BeginNode(nodeId);

	const auto& style = ed::GetStyle();
	const auto lPad = style.NodePadding.x;
	const auto rPad = style.NodePadding.z;
	const auto borderW = style.NodeBorderWidth;
	const float pinRadius = 5;
	const auto nSize = ed::GetNodeSize(nodeId);
	const auto nPos = ed::GetNodePosition(nodeId);

	PushRightEdge(nSize.x - lPad - rPad - 2 * borderW);

	PushStyleColor(ImGuiCol_Text, IM_COL(0, 255, 0));
	PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);
	PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32_BLACK_TRANS);
	PushStyleColor(ImGuiCol_ButtonActive, IM_COL32_BLACK_TRANS);
	PushID(PLAY_BUTTON_ID(nodeId));
	if (ArrowButton("", ImGuiDir_Right)) { play(id); }
	PopID();
	PopStyleColor(4);
	SameLine();
	AlignedText(node.name, TextAlign::AlignCenter, &maxTextWidth);

	std::vector<std::pair<ImVec2, ImColor>> pins;

#define DRAW_INPUT_SOCKET(SOCKET, ID)                             \
	ed::BeginPin((ID), ed::PinKind::Input);                       \
	AlignedText((SOCKET).name, &maxTextWidth);                    \
	pins.emplace_back(                                            \
		GetItemRectPoint(0, 0.5) - ImVec2(lPad - borderW / 2, 0), \
		IM_COL(255, 0, 0));                                       \
	ed::PinRect(                                                  \
		pins.back().first - ImVec2(pinRadius, pinRadius),         \
		pins.back().first + ImVec2(pinRadius, pinRadius));        \
	ed::EndPin();

#define DRAW_OUTPUT_SOCKET(SOCKET, ID)                            \
	ed::BeginPin((ID), ed::PinKind::Output);                      \
	AlignedText((SOCKET).name, ImGui::AlignRight, &maxTextWidth); \
	pins.emplace_back(                                            \
		GetItemRectPoint(1, 0.5) + ImVec2(rPad + borderW / 2, 0), \
		IM_COL(0, 255, 0));                                       \
	ed::PinRect(                                                  \
		pins.back().first - ImVec2(pinRadius, pinRadius),         \
		pins.back().first + ImVec2(pinRadius, pinRadius));        \
	ed::EndPin();

	{
		// Both
		for (const auto& [in, inID, out, outID] : std::views::zip(
				 node.input(), node.inputSocketIds, node.output(),
				 node.outputSocketIds)) {
			DRAW_INPUT_SOCKET(in, inID.val);
			ImGui::SameLine();
			DRAW_OUTPUT_SOCKET(out, outID.val);
		}
		const auto commonCnt =
			std::min(node.input().size(), node.output().size());
		// Only input
		for (const auto& [in, inID] :
			 std::views::zip(node.input(), node.inputSocketIds) |
				 std::views::drop(commonCnt)) {
			DRAW_INPUT_SOCKET(in, inID.val);
		}
		// Only output
		for (const auto& [out, outID] :
			 std::views::zip(node.output(), node.outputSocketIds) |
				 std::views::drop(commonCnt)) {
			DRAW_OUTPUT_SOCKET(out, outID.val);
		}
	}

	if (node.option.size() > 0) {
		PushID(OPT_ID(nodeId));
		{
			const auto& options = node.base().options;
			for (const auto& [idx, _] : node.option) {
				maxTextWidth = std::max(
					maxTextWidth,
					ImGui::CalcTextSize(options[idx].name.c_str()).x);
			}
			for (auto& [optIdx, optValue] : g.getNode(id).option) {
				AlignedText(options[optIdx].name, &maxTextWidth);
				ed::Suspend();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
					ImGui::SetTooltip("%s", options[optIdx].desc.data());
				}
				ed::Resume();
				SameLine(maxTextWidth);
				PushItemWidth(maxTextWidth);
				PushID(optIdx);
				ImGui::InputText("", &optValue);
				PopID();
				PopItemWidth();
			}
		}
		PopID();
	}

	if (node.option.size() < node.base().options.size()) {
		PushID(OPT_BUTTON_ID(nodeId));
		{
			if (ImGui::Button("+")) {
				popup = POPUP_ADD_OPT;
				activeNode = id;
			}
		}
		PopID();
	}

	PopRightEdge();
	ed::EndNode();

	const auto list = ed::GetNodeBackgroundDrawList(nodeId);
	ImGui::AddRoundedFilledRect(
		list, nPos + ImVec2(borderW / 2, borderW / 2),
		nPos + ImVec2(nSize.x - borderW / 2, GetFrameHeightWithSpacing()),
		style.NodeRounding, ImColor(255, 0, 0),
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
	if (ImGui::BeginPopupModal(
			POPUP_MISSING_INPUT, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("%s", popupString.c_str());
		if (ImGui::Button("Ok")) { ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopup(POPUP_ADD_OPT)) {
		if (searchStarted) {
			searchStarted = false;
			ImGui::SetKeyboardFocusHere();
		}

		searchFilter.Draw(" ");
		auto& node = g.getNode(activeNode);
		for (const auto& [index, opt] :
			 std::views::enumerate(node.base().options)) {
			const auto idx = int(index);
			if (contains(node.option, idx)) { continue; }
			if (searchFilter.PassFilter(opt.name.c_str()) ||
				searchFilter.PassFilter(opt.desc.c_str())) {
				if (ImGui::Selectable(opt.name.c_str())) {
					node.option[idx] = opt.defaultValue;
					searchFilter.Clear();
					CloseCurrentPopup();
				}
				ImGui::SameLine();
				ImGui::Text("- %s", opt.desc.c_str());
			}
		}
		ImGui::EndPopup();
	} else {
		activeNode = INVALID_NODE;
	}
}

void NodeEditor::searchBar() {
	const auto POP_UP_ID = "add_node_popup";
	if (!ImGui::IsPopupOpen(POP_UP_ID) &&
		ImGui::IsKeyPressed(ImGuiKey_Space, false) && profile != nullptr) {
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
	if (ImGui::Begin(name.c_str())) {
		ed::SetCurrentEditor(context.get());
		ed::Begin(name.c_str());

		g.iterateNodes([this](const FilterNode& node, const NodeId& id) {
			drawNode(node, id);
		});

		g.iterateLinks([](const LinkId& id, const NodeId& s, const NodeId& d) {
			ed::Link(id.val, s.val, d.val);
		});

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

void to_json(nlohmann::json& j, const NodeId& id) { j = id.val; }

bool NodeEditor::save(const std::filesystem::path& path) const {
	nlohmann::json obj;
	g.iterateNodes([&](const FilterNode& node, const NodeId& id) {
		nlohmann::json elem;
		elem["id"] = id;
		elem["name"] = node.base().name;
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

bool load(
	const std::filesystem::path& path, const Profile& profile,
	std::vector<NodeEditor>& editors) {
	nlohmann::json json;
	try {
		json = nlohmann::json::parse(std::ifstream(path));
	} catch (nlohmann::json::exception&) { return false; }
	FilterGraph g;
	std::map<int, NodeId> mapping;
	for (const auto& elem : json["nodes"]) {
		auto id = elem["id"].template get<int>();
		auto name = elem["name"].template get<std::string>();
		const auto& base = std::find_if(
			profile.filters.begin(), profile.filters.end(),
			[&](const Filter& filter) { return filter.name == name; });
		auto nId = g.addNode(*base);
		mapping[id] = nId;
		for (const auto& [sNid, sId] : std::views::zip(
				 g.getNode(nId).inputSocketIds,
				 elem["inputs"].template get<std::vector<int>>())) {
			mapping[sId] = sNid;
		}
		for (const auto& [sNid, sId] : std::views::zip(
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
	editors.emplace_back(g, path.filename().string(), &profile);
	NodeEditor e;
	// editors.
	// std::swap(*this, g);
	return true;
}
