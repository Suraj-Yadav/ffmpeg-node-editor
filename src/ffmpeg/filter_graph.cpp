#include "ffmpeg/filter_graph.hpp"

#include <absl/strings/match.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "ffmpeg/profile.hpp"
#include "ffmpeg/runner.hpp"
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
		for (auto& v : state.adjList[u]) { deleteEdge(state, u, v); }
		for (auto& v : state.revAdjList[u]) { deleteEdge(state, v, u); }
		state.valid[u] = false;
	}
	std::vector<Socket> getMediaInfo(const Runner& r, const std::string& path) {
		std::vector<Socket> sockets;
		bool inputStarted = false;
		r.lineScanner(
			{"-i", path},
			[&](absl::string_view line) {
				if (!inputStarted) {
					inputStarted = absl::StartsWith(line, "Input #0");
					return true;
				}
				if (absl::StartsWith(line, "  Stream #0")) {
					if (absl::StrContains(line, "Video:")) {
						sockets.push_back({"", SocketType::Video});
					} else if (absl::StrContains(line, "Audio:")) {
						sockets.push_back({"", SocketType::Audio});
					} else if (absl::StrContains(line, "Subtitle:")) {
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
		for (auto i = 0u; i < l; ++i) {
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
		NodeIterCallback cb) {
		marked[v] = true;
		if (!state.valid[v]) { return; }
		for (auto& u : state.revAdjList[v]) {
			if (marked[u]) { continue; }
			dfs(state, marked, nodes, u, cb);
		}
		if (state.isSocket[v]) { return; }
		cb(nodes[state.vertIdToNodeIndex[v]], getNodeId(v));
	}
}  // namespace

void FilterGraph::optHook(
	const NodeId& id, const int& optId, const std::string& value) {
	auto& node = getNode(id);
	const auto& base = node.base();
	if (base.name == INPUT_FILTER_NAME && base.options[optId].name == "path") {
		auto newSockets = getMediaInfo(profile.runner, value);

		auto nodeVertexId = getU(id);
		auto nodeIndex = state.vertIdToNodeIndex[nodeVertexId];

		updateSocketsIds(
			state, nodeIndex, nodeVertexId, newSockets, node.output(),
			node.outputSocketIds, false);
		node.outputSockets = newSockets;
	}
}

const NodeId FilterGraph::addNode(const Filter& filter) {
	auto nodeIndex = nodes.size();
	nodes.emplace_back(filter);

	auto nodeVertexId = addVertex(state, nodeIndex, false, 0, false);

	updateSocketsIds(
		state, nodeIndex, nodeVertexId, filter.input, {},
		nodes.back().inputSocketIds, true);

	updateSocketsIds(
		state, nodeIndex, nodeVertexId, filter.output, {},
		nodes.back().outputSocketIds, false);

	for (int i = 0; i < filter.options.size(); ++i) {
		if (filter.options[i].name == filter.name) {
			nodes.back().option[i] = filter.options[i].defaultValue;
			optHook(getNodeId(nodeVertexId), i, filter.options[i].defaultValue);
			break;
		}
	}
	return getNodeId(nodeVertexId);
}

const LinkId FilterGraph::addLink(NodeId uu, NodeId vv) {
	auto u = getU(uu), v = getU(vv);
	if (!canAddEdge(state, nodes, u, v)) { return INVALID_LINK; }
	if (state.isInput[u]) { std::swap(u, v); }
	if (addEdge(state, u, v)) { return getLinkId(u, v); }
	return INVALID_LINK;
}

void FilterGraph::deleteNode(NodeId id) { deleteVertex(state, getU(id)); };

void FilterGraph::deleteLink(LinkId id) {
	IdBaseType u, v;
	getUV(id, u, v);
	deleteEdge(state, u, v);
}

bool FilterGraph::canAddLink(NodeId uu, NodeId vv) const {
	auto u = getU(uu), v = getU(vv);
	return canAddEdge(state, nodes, u, v);
}

void FilterGraph::iterateLinks(EdgeIterCallback cb) const {
	for (IdBaseType v = 0; v < state.revAdjList.size(); ++v) {
		for (auto& u : state.revAdjList[v]) {
			if (state.isSocket[u] && state.isSocket[v]) {
				cb(getLinkId(u, v), getNodeId(u), getNodeId(v));
			}
		}
	}
}

void FilterGraph::iterateNodes(
	NodeIterCallback cb, NodeIterOrder order, NodeId u) const {
	if (order == NodeIterOrder::Default) {
		for (auto i = 0u; i < state.revAdjList.size(); ++i) {
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

void FilterGraph::inputSockets(NodeId uu, InputSocketCallback cb) const {
	auto u = getU(uu);
	if (state.isSocket[u]) { return; }
	const auto& node = nodes[state.vertIdToNodeIndex[u]];
	for (auto i = 0u; i < node.input().size(); ++i) {
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
void FilterGraph::outputSockets(NodeId uu, OutputSocketCallback cb) const {
	auto u = getU(uu);
	if (state.isSocket[u]) { return; }
	const auto& node = nodes[state.vertIdToNodeIndex[u]];
	for (auto i = 0u; i < node.output().size(); ++i) {
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

FilterGraphError FilterGraph::play(const NodeId& id) {
	FilterGraphError err{NO_ERROR};

	fmt::memory_buffer buff;
	std::vector<std::string> inputs;
	std::map<IdBaseType, std::string> inputSocketNames;
	std::map<IdBaseType, std::string> outputSocketNames;
	iterateNodes(
		[&](const FilterNode& node, const NodeId& id) {
			auto idx = inputs.size();
			if (err.code != NO_ERROR) { return; }
			auto isInput = node.base().name == INPUT_FILTER_NAME;
			inputSockets(
				id, [&](const Socket& s, const NodeId& sId,
						const NodeId& parentSocketId) {
					if (err.code != NO_ERROR) { return; }
					if (parentSocketId == INVALID_NODE) {
						err.code = PLAYER_MISSING_INPUT;
						err.message = fmt::format(
							"Socket {} of node {} needs an input", s.name,
							node.name);
						return;
					}
					outputSocketNames.erase(parentSocketId.val);
					fmt::format_to(
						std::back_inserter(buff), "{}",
						inputSocketNames[parentSocketId.val]);
				});
			if (err.code != NO_ERROR) { return; }
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
					fmt::format_to(
						std::back_inserter(buff), "{}={}",
						options[optId].name.data(), optValue);
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
	if (err.code != NO_ERROR) { return err; }
	std::vector<std::string> out;
	for (auto& e : outputSocketNames) {
		out.push_back(fmt::format("{}", e.second));
	}
	SPDLOG_INFO(fmt::to_string(buff));
	int status = 0;
	std::tie(status, err.message) =
		profile.runner.play(inputs, fmt::to_string(buff), out);
	if (status != 0) { err.code = PLAYER_RUNTIME; }
	return err;
}

const std::vector<Filter>& FilterGraph::allFilters() const {
	return profile.filters;
}

void FilterGraph::clear() {
	nodes.clear();
	state = GraphState{};
}
