#include "ffmpeg/filter_graph.hpp"

#include <fmt/core.h>
#include <vcruntime.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "ffmpeg/filter.hpp"
#include "ffmpeg/filter_node.hpp"

#define contains(X, Y) (std::find((X).cbegin(), (X).cend(), (Y)) != (X).cend())

const int LINK_ID_SHIFT = std::numeric_limits<IdBaseType>::digits / 2;

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
	const GraphState& state, const std::vector<FilterNode>& nodes, IdBaseType u,
	Socket& s) {
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
	// state.adjList.emplace_back();
	state.revAdjList.emplace_back(0);
	state.vertIdToNodeIndex.emplace_back(nodeIndex);
	state.valid.emplace_back(true);
	state.isInput.emplace_back(isInput);
	state.isSocket.emplace_back(isSocket);
	state.vertIdToSocketIndex.emplace_back(socketIndex);
	return id;
}

bool canAddEdge(
	GraphState& state, const std::vector<FilterNode>& nodes, IdBaseType u,
	IdBaseType v) {
	Socket us, vs;
	if (state.vertIdToNodeIndex[u] == state.vertIdToNodeIndex[v]) {
		return false;
	}
	if (!getSocket(state, nodes, u, us) || !getSocket(state, nodes, v, vs)) {
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
	return true;
}

void deleteEdge(GraphState& state, IdBaseType u, IdBaseType v) {
	std::erase(state.revAdjList[v], u);
}

void deleteVertex(GraphState& state, IdBaseType u) { state.valid[u] = false; }

void FilterGraph::addNode(const Filter& filter) {
	auto nodeIndex = nodes.size();
	nodes.emplace_back(filter);

	auto nodeVertexId = addVertex(state, nodeIndex, false, 0, false);

	for (auto i = 0u; i < filter.input.size(); ++i) {
		auto iVertexId = addVertex(state, nodeIndex, true, i, true);
		addEdge(state, iVertexId, nodeVertexId);
		nodes.back().inputSocketIds.push_back(getNodeId(iVertexId));
	}
	for (auto i = 0u; i < filter.output.size(); ++i) {
		auto oVertexId = addVertex(state, nodeIndex, true, i, false);
		addEdge(state, nodeVertexId, oVertexId);
		nodes.back().outputSocketIds.push_back(getNodeId(oVertexId));
	}
}

const LinkId FilterGraph::addLink(NodeId uu, NodeId vv) {
	auto u = getU(uu), v = getU(vv);
	if (!canAddEdge(state, nodes, u, v)) { return INVALID_LINK; }
	if (state.isInput[u]) { std::swap(u, v); }
	if (addEdge(state, u, v)) {
		fmt::println("Added link {}->{}", uu.val, vv.val);
		return getLinkId(u, v);
	}
	return INVALID_LINK;
}

void FilterGraph::deleteNode(NodeId id) { deleteVertex(state, getU(id)); };

void FilterGraph::deleteLink(LinkId id) {
	IdBaseType u, v;
	getUV(id, u, v);
	deleteEdge(state, u, v);
}

bool FilterGraph::canAddLink(NodeId uu, NodeId vv) {
	auto u = getU(uu), v = getU(vv);
	return canAddEdge(state, nodes, u, v);
}

void FilterGraph::iterateLinks(EdgeIterCallback cb) {
	for (IdBaseType v = 0; v < state.revAdjList.size(); ++v) {
		for (auto& u : state.revAdjList[v]) {
			if (state.isSocket[u] && state.isSocket[v]) {
				cb(getLinkId(u, v), getNodeId(u), getNodeId(v));
			}
		}
	}
}

void dfs(
	const GraphState& state, std::vector<bool>& marked,
	const std::vector<FilterNode>& nodes, IdBaseType v, NodeIterCallback cb) {
	marked[v] = true;
	if (!state.valid[v]) { return; }
	for (auto& u : state.revAdjList[v]) {
		if (marked[u]) { continue; }
		dfs(state, marked, nodes, u, cb);
	}
	if (state.isSocket[v]) { return; }
	cb(nodes[state.vertIdToNodeIndex[v]], getNodeId(v));
}

void FilterGraph::iterateNodes(
	NodeIterCallback cb, NodeIterOrder order, NodeId u) {
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

void FilterGraph::inputSockets(NodeId uu, InputSocketCallback cb) {
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
void FilterGraph::outputSockets(NodeId uu, OutputSocketCallback cb) {
	auto u = getU(uu);
	if (state.isSocket[u]) { return; }
	const auto& node = nodes[state.vertIdToNodeIndex[u]];
	for (auto i = 0u; i < node.output().size(); ++i) {
		const auto& socket = node.output()[i];
		const auto& socketId = node.outputSocketIds[i];
		cb(socket, socketId);
	}
}
