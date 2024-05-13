#include "ffmpeg/filter_graph.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "ffmpeg/filter.hpp"
#include "ffmpeg/profile.hpp"
#include "ffmpeg/runner.hpp"
#include "node_editor.hpp"
#include "string_utils.hpp"
#include "util.hpp"

const int LINK_ID_SHIFT = std::numeric_limits<IdBaseType>::digits / 2;

namespace {
	LinkId getLinkId(IdBaseType u, IdBaseType v) {
		return {(u << LINK_ID_SHIFT) + v + 1};
	}

	void getUV(const LinkId& id, IdBaseType& u, IdBaseType& v) {
		auto val = id.val - 1;
		v = val & ((IdBaseType(1) << LINK_ID_SHIFT) - 1);
		u = val >> LINK_ID_SHIFT;
	}

	NodeId getNodeId(const IdBaseType& u) { return {u + 1}; };
	IdBaseType getU(const NodeId& id) { return id.val - 1; }

	bool getSocket(
		const GraphState& state, const std::vector<FilterNode>& nodes,
		IdBaseType u, Socket& s) {
		if (!state.valid[u]) { return false; }
		if (!state.isSocket[u]) { return false; }

		auto i = state.vertIdToNodeIndex[u];

		if (state.isInput[u]) {
			s = nodes[i].input()[state.vertIdToSocketIndex[u]];
		} else {
			s = nodes[i].output()[state.vertIdToSocketIndex[u]];
		}
		return true;
	}

	IdBaseType addVertex(
		GraphState& state, size_t nodeIndex, bool isSocket, size_t socketIndex,
		bool isInput) {
		auto id = state.revAdjList.size();
		state.adjList.emplace_back(0);
		state.revAdjList.emplace_back(0);
		state.vertIdToNodeIndex.emplace_back(nodeIndex);
		state.valid.emplace_back(true);
		state.isInput.emplace_back(isInput);
		state.isSocket.emplace_back(isSocket);
		state.vertIdToSocketIndex.emplace_back(socketIndex);
		return id;
	}

	bool canAddEdge(
		const GraphState& state, const std::vector<FilterNode>& nodes,
		IdBaseType u, IdBaseType v) {
		Socket us, vs;
		if (state.vertIdToNodeIndex[u] == state.vertIdToNodeIndex[v]) {
			return false;
		}
		if (!getSocket(state, nodes, u, us) ||
			!getSocket(state, nodes, v, vs)) {
			return false;
		}
		if (state.isInput[u]) { std::swap(u, v); }
		if (state.isInput[u] == state.isInput[v]) { return false; }
		if (us.type != vs.type) { return false; }
		if (state.revAdjList[v].size() > 0) { return false; }
		const auto& adj = state.revAdjList[v];
		return !contains(adj, u);
	}

	bool addEdge(GraphState& state, IdBaseType u, IdBaseType v) {
		if (contains(state.revAdjList[v], u)) { return false; }
		state.revAdjList[v].push_back(u);
		state.adjList[u].push_back(v);
		return true;
	}

	void deleteEdge(GraphState& state, IdBaseType u, IdBaseType v) {
		erase(state.revAdjList[v], u);
		erase(state.adjList[u], v);
	}

	void deleteVertex(GraphState& state, IdBaseType u) {
		if (state.vertIdToNodeIndex[u] == u) {
			for (auto& v : state.adjList[u]) { deleteVertex(state, v); }
			for (auto& v : state.revAdjList[u]) { deleteVertex(state, v); }
		} else {
			for (auto& v : state.adjList[u]) { deleteEdge(state, u, v); }
			for (auto& v : state.revAdjList[u]) { deleteEdge(state, v, u); }
		}
		state.valid[u] = false;
	}
	std::vector<Socket> getMediaInfo(const Runner& r, const std::string& path) {
		std::vector<Socket> sockets;
		bool inputStarted = false;
		r.lineScanner(
			{"-i", path},
			[&](std::string_view line) {
				if (!inputStarted) {
					inputStarted = str::starts_with(line, "Input #0");
					return true;
				}
				if (str::starts_with(line, "  Stream #0")) {
					if (str::contains(line, "Video:")) {
						sockets.push_back({"", SocketType::Video});
					} else if (str::contains(line, "Audio:")) {
						sockets.push_back({"", SocketType::Audio});
					} else if (str::contains(line, "Subtitle:")) {
						sockets.push_back({"", SocketType::Subtitle});
					}
				}
				return true;
			},
			true);
		return sockets;
	}

	void updateSocketsIds(
		GraphState& state, size_t nodeIndex, IdBaseType nodeVertexId,
		const std::vector<Socket>& newSockets,
		const std::vector<Socket>& oldSockets, std::vector<NodeId>& oldIds,
		bool isInput) {
		std::vector<NodeId> newIds;

		auto l = std::max(newSockets.size(), oldSockets.size());
		for (auto i = 0U; i < l; ++i) {
			auto socketMatch =
				(i < newSockets.size() && i < oldSockets.size() &&
				 newSockets[i].type == oldSockets[i].type);
			if (socketMatch) {
				newIds.push_back(oldIds[i]);
				continue;
			}
			auto needNewSocket = i < newSockets.size();
			auto deleteOldSocket = i < oldSockets.size();
			if (needNewSocket) {
				auto sVertexId = addVertex(state, nodeIndex, true, i, isInput);
				if (isInput) {
					addEdge(state, sVertexId, nodeVertexId);
				} else {
					addEdge(state, nodeVertexId, sVertexId);
				}
				newIds.push_back(getNodeId(sVertexId));
			}
			if (deleteOldSocket) { deleteVertex(state, getU(oldIds[i])); }
		}
		oldIds = newIds;
	}

	void dfs(
		const GraphState& state, std::vector<bool>& marked,
		const std::vector<FilterNode>& nodes, IdBaseType v,
		const NodeIterCallback& cb) {
		marked[v] = true;
		if (!state.valid[v]) { return; }
		for (const auto& u : state.revAdjList[v]) {
			if (marked[u]) { continue; }
			dfs(state, marked, nodes, u, cb);
		}
		if (state.isSocket[v]) { return; }
		cb(nodes[state.vertIdToNodeIndex[v]], getNodeId(v));
	}

	std::vector<Socket> getNewSockets(unsigned int count, SocketType type) {
		return std::vector<Socket>(count, {"", type});
	}

	std::vector<Socket> getNewSockets(
		long long count, std::string_view filterName) {
		if (str::starts_with(filterName, "a", true)) {
			return std::vector<Socket>(count, {"", SocketType::Audio});
		}
		return std::vector<Socket>(count, {"", SocketType::Video});
	}
	int getOptIndex(const Filter& base, std::string_view name) {
		for (int i = 0; i < base.options.size(); ++i) {
			if (base.options[i].name == name) { return i; }
		}
		return -1;
	}
}  // namespace

void FilterGraph::optHook(
	const NodeId& id, const int& optId, const std::string& value) {
	auto& node = getNode(id);
	const auto& base = node.base();
	const auto& option = base.options[optId];
	auto nodeVertexId = getU(id);
	auto nodeIndex = state.vertIdToNodeIndex[nodeVertexId];

	std::optional<std::vector<Socket>> newInputs, newOutputs;

	struct Counts {
		std::set<std::string_view> fNames;
		std::set<std::string_view> oNames;
		bool isInput;
		std::string_view selector;
	};

	std::vector<Counts> count{
		{{"ainterleave", "interleave"}, {"nb_inputs", "n"}, true},
		{{"amerge"}, {"inputs"}, true},
		{{"amix", "mix"}, {"inputs"}, true},
		{{"aselect", "select"}, {"outputs", "n"}, false},
		{{"asplit", "split"}, {"outputs", "n"}, false},
		{{"join"}, {"inputs"}, true, "a"},
		{{"hstack"}, {"inputs"}, true},
		{{"libplacebo"}, {"inputs"}, true},
		{{"program_opencl"}, {"inputs"}, true},
		{{"signature"}, {"nb_inputs"}, true},
		{{"vstack"}, {"inputs"}, true},
		{{"xmedian"}, {"inputs"}, true},
		{{"xstack"}, {"inputs"}, true},
		{{"hstack_qsv"}, {"inputs"}, true},
		{{"vstack_qsv"}, {"inputs"}, true},
		{{"xstack_qsv"}, {"inputs"}, true},
	};

	if (auto itr = std::find_if(
			count.begin(), count.end(),
			[&base, &option](const Counts& c) {
				return contains(c.fNames, base.name) &&
					   contains(c.oNames, option.name);
			});
		itr != count.end()) {
		if (auto count = std::stoi(value)) {
			auto selector = itr->selector.empty() ? base.name : itr->selector;
			if (itr->isInput) {
				newInputs = getNewSockets(count, selector);
			} else {
				newOutputs = getNewSockets(count, selector);
			}
		}
	} else if (
		(base.name == INPUT_FILTER_NAME || base.name == "movie" ||
		 base.name == "amovie") &&
		option.name == "filename") {
		newOutputs = getMediaInfo(profile->runner, value);
	} else if (base.name == "acrossover" && option.name == "split") {
		auto count = 1U;
		char last = '\0';
		for (const auto& ch : str::strip(value)) {
			if (str::isspace(ch) && !str::isspace(last)) { count++; }
			last = ch;
		}
		if (count != node.output().size()) {
			newOutputs = getNewSockets(count, SocketType::Audio);
		}
	} else if (
		(base.name == "asegment" || base.name == "segment") &&
		(option.name == "timestamps")) {
		auto count = std::count(value.begin(), value.end(), '|') + 1;
		if (count != node.output().size()) {
			newOutputs = getNewSockets(count, base.name);
		}
	} else if (base.name == "concat") {
		int n = 2, v = 1, a = 0;
		auto ni = getOptIndex(base, "n");
		auto vi = getOptIndex(base, "v");
		auto ai = getOptIndex(base, "a");
		if (contains(node.option, ni)) { n = std::stoi(node.option[ni]); }
		if (contains(node.option, vi)) { v = std::stoi(node.option[vi]); }
		if (contains(node.option, ai)) { a = std::stoi(node.option[ai]); }
		newOutputs = getNewSockets(v, SocketType::Video);
		newOutputs->insert(newOutputs->end(), a, {"", SocketType::Audio});
		newInputs = getNewSockets(0, "");
		for (int i = 0; i < n; ++i) {
			newInputs->insert(
				newInputs->end(), newOutputs->begin(), newOutputs->end());
		}
	}
	if (newInputs.has_value()) {
		updateSocketsIds(
			state, nodeIndex, nodeVertexId, newInputs.value(), node.input(),
			node.inputSocketIds, true);
		node.inputSockets = newInputs.value();
	}
	if (newOutputs.has_value()) {
		updateSocketsIds(
			state, nodeIndex, nodeVertexId, newOutputs.value(), node.output(),
			node.outputSocketIds, false);
		node.outputSockets = newOutputs.value();
	}
}

NodeId FilterGraph::addNode(const Filter& filter) {
	auto nodeIndex = nodes.size();
	nodes.emplace_back(filter);

	auto nodeVertexId = addVertex(state, nodeIndex, false, 0, false);

	updateSocketsIds(
		state, nodeIndex, nodeVertexId, filter.input, {},
		nodes.back().inputSocketIds, true);

	updateSocketsIds(
		state, nodeIndex, nodeVertexId, filter.output, {},
		nodes.back().outputSocketIds, false);

	if (auto i = getOptIndex(filter, filter.name); i != -1) {
		nodes.back().option[i] = filter.options[i].defaultValue;
		optHook(getNodeId(nodeVertexId), i, filter.options[i].defaultValue);
	}
	return getNodeId(nodeVertexId);
}

LinkId FilterGraph::addLink(NodeId uu, NodeId vv) {
	auto u = getU(uu), v = getU(vv);
	if (!canAddEdge(state, nodes, u, v)) { return INVALID_LINK; }
	if (state.isInput[u]) { std::swap(u, v); }
	if (addEdge(state, u, v)) { return getLinkId(u, v); }
	return INVALID_LINK;
}

void FilterGraph::deleteNode(NodeId id) { deleteVertex(state, getU(id)); };

void FilterGraph::deleteLink(LinkId id) {
	IdBaseType u = 0, v = 0;
	getUV(id, u, v);
	deleteEdge(state, u, v);
}

bool FilterGraph::canAddLink(NodeId uu, NodeId vv) const {
	auto u = getU(uu), v = getU(vv);
	return canAddEdge(state, nodes, u, v);
}

void FilterGraph::iterateLinks(const EdgeIterCallback& cb) const {
	for (IdBaseType v = 0; v < state.revAdjList.size(); ++v) {
		for (const auto& u : state.revAdjList[v]) {
			if (state.isSocket[u] && state.isSocket[v]) {
				cb(getLinkId(u, v), getNodeId(u), getNodeId(v));
			}
		}
	}
}

void FilterGraph::iterateNodes(
	const NodeIterCallback& cb, NodeIterOrder order, NodeId u) const {
	if (order == NodeIterOrder::Default) {
		for (auto i = 0U; i < state.revAdjList.size(); ++i) {
			if (state.valid[i] && !state.isSocket[i]) {
				cb(nodes[state.vertIdToNodeIndex[i]], getNodeId(i));
			}
		}
		return;
	}

	IdBaseType b = 0, e = state.revAdjList.size();
	if (u != INVALID_NODE) {
		b = getU(u);
		e = b + 1;
	}

	// Topological Sort
	std::vector<bool> marked(state.revAdjList.size(), false);
	for (auto v = b; v < e; ++v) {
		if (!marked[v]) { dfs(state, marked, nodes, v, cb); }
	}
}

void FilterGraph::inputSockets(NodeId uu, const InputSocketCallback& cb) const {
	auto u = getU(uu);
	if (state.isSocket[u]) { return; }
	const auto& node = nodes[state.vertIdToNodeIndex[u]];
	for (auto i = 0U; i < node.input().size(); ++i) {
		const auto& socket = node.input()[i];
		const auto& socketId = node.inputSocketIds[i];
		const auto& v = getU(socketId);
		if (state.revAdjList[v].empty()) {
			cb(socket, socketId, INVALID_NODE);
		} else {
			cb(socket, socketId, getNodeId(state.revAdjList[v][0]));
		}
	}
}
void FilterGraph::outputSockets(
	NodeId uu, const OutputSocketCallback& cb) const {
	auto u = getU(uu);
	if (state.isSocket[u]) { return; }
	const auto& node = nodes[state.vertIdToNodeIndex[u]];
	for (auto i = 0U; i < node.output().size(); ++i) {
		const auto& socket = node.output()[i];
		const auto& socketId = node.outputSocketIds[i];
		cb(socket, socketId);
	}
}

const FilterNode& FilterGraph::getNode(NodeId id) const {
	return nodes[state.vertIdToNodeIndex[getU(id)]];
}
FilterNode& FilterGraph::getNode(NodeId id) {
	return nodes[state.vertIdToNodeIndex[getU(id)]];
}

FilterGraphError FilterGraph::play(const Preference& pref, const NodeId& id) {
	FilterGraphError err{FilterGraphErrorCode::PLAYER_NO_ERROR};

	std::string buff;
	std::vector<std::string> inputs;
	std::map<IdBaseType, std::string> inputSocketNames;
	std::map<IdBaseType, std::string> outputSocketNames;
	iterateNodes(
		[&](const FilterNode& node, const NodeId& id) {
			auto idx = inputs.size();
			if (err.code != FilterGraphErrorCode::PLAYER_NO_ERROR) { return; }
			auto isInput = node.base().name == INPUT_FILTER_NAME;
			inputSockets(
				id, [&](const Socket& s, const NodeId& sId,
						const NodeId& parentSocketId) {
					if (err.code != FilterGraphErrorCode::PLAYER_NO_ERROR) {
						return;
					}
					if (parentSocketId == INVALID_NODE) {
						err.code = FilterGraphErrorCode::PLAYER_MISSING_INPUT;
						err.message = fmt::format(
							"Socket \"{}\" of node \"{}\" needs an input",
							s.name, node.name);
						return;
					}
					outputSocketNames.erase(parentSocketId.val);
					fmt::format_to(
						std::back_inserter(buff), "{}",
						inputSocketNames[parentSocketId.val]);
				});
			if (err.code != FilterGraphErrorCode::PLAYER_NO_ERROR) { return; }
			if (isInput) {
				inputs.push_back(node.option.at(0));
			} else {
				fmt::format_to(
					std::back_inserter(buff), "{}@{}{}", node.name, node.name,
					id.val);

				const auto& options = node.base().options;
				bool first = true;
				for (const auto& [optId, optValue] : node.option) {
					if (first) {
						buff.push_back('=');
					} else {
						buff.push_back(':');
					}
					first = false;

					const auto* const VALUE_FORMAT = "{}=\"{}\"";

#ifdef _WIN32
					// Hack for fontconfig in windows
					if (str::ends_with(options[optId].name, "fontfile")) {
						auto v = optValue;
						std::replace(v.begin(), v.end(), '\\', '/');
						v.insert(v.find(':'), "\\\\");
						fmt::format_to(
							std::back_inserter(buff), VALUE_FORMAT,
							options[optId].name.data(), v);
					} else {
						fmt::format_to(
							std::back_inserter(buff), VALUE_FORMAT,
							options[optId].name.data(), optValue);
					}
#else
					fmt::format_to(
						std::back_inserter(buff), VALUE_FORMAT,
						options[optId].name.data(), optValue);
#endif
				}
			}

			auto socketIdx = 0;
			outputSockets(id, [&](const Socket&, const NodeId& socketId) {
				if (isInput) {
					// outputSocketNames[socketId.val] =
					// 	fmt::format("{}:{}", idx, socketIdx);
					inputSocketNames[socketId.val] =
						fmt::format("[{}:{}]", idx, socketIdx);
					socketIdx++;
					return;
				}
				inputSocketNames[socketId.val] =
					fmt::format("[s{}]", socketId.val);
				outputSocketNames[socketId.val] =
					fmt::format("[s{}]", socketId.val);
				fmt::format_to(
					std::back_inserter(buff), inputSocketNames[socketId.val]);
			});
			if (!isInput) { fmt::format_to(std::back_inserter(buff), ";"); }
		},
		NodeIterOrder::Topological, id);
	if (err.code != FilterGraphErrorCode::PLAYER_NO_ERROR) { return err; }
	std::vector<std::string> out;
	out.reserve(outputSocketNames.size());
	for (auto& e : outputSocketNames) {
		out.push_back(fmt::format("{}", e.second));
	}
	int status = 0;
	// this is needed
	auto filterString = fmt::to_string(buff);
	if (filterString.back() == ';') { filterString.pop_back(); }
	std::tie(status, err.message) =
		profile->runner.play(inputs, filterString, out, pref.player);
	if (status != 0) { err.code = FilterGraphErrorCode::PLAYER_RUNTIME; }
	return err;
}

const std::vector<Filter>& FilterGraph::allFilters() const {
	return profile->filters;
}

void FilterGraph::clear() {
	nodes.clear();
	state = GraphState{};
}
