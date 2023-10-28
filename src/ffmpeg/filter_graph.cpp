#include "ffmpeg/filter_graph.hpp"

#include <vcruntime.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "ffmpeg/filter.hpp"
#include "ffmpeg/filter_node.hpp"

LinkId getLinkId(const GraphState& state, size_t u, size_t v) {
	return {u * state.valid.size() + v + 1};
}

void getUV(const GraphState& state, const LinkId& id, size_t& u, size_t& v) {
	auto val = id.val - 1;
	u = val / state.valid.size();
	v = val % state.valid.size();
}

bool getSocket(
	const GraphState& state, const std::vector<FilterNode>& nodes, size_t u,
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

size_t addVertex(
	GraphState& state, size_t nodeIndex, bool isSocket, size_t socketIndex,
	bool isInput) {
	auto id = state.adjList.size();
	state.adjList.emplace_back();
	state.vertIdToNodeIndex.emplace_back(nodeIndex);
	state.valid.emplace_back(true);
	state.isInput.emplace_back(isInput);
	state.isSocket.emplace_back(isSocket);
	state.vertIdToSocketIndex.emplace_back(socketIndex);
	return id;
}

bool canAddEdge(
	GraphState& state, const std::vector<FilterNode>& nodes, size_t u,
	size_t v) {
	Socket us, vs;
	if (!getSocket(state, nodes, u, us) || !getSocket(state, nodes, v, vs)) {
		return false;
	}
	if (state.isInput[u]) { std::swap(u, v); }
	if (state.isInput[u] == state.isInput[v]) { return false; }
	if (us.type != vs.type) { return false; }
	return std::find(state.adjList[u].begin(), state.adjList[u].end(), v) ==
		   state.adjList[u].end();
}

bool addEdge(GraphState& state, size_t u, size_t v) {
	auto itr = std::find(state.adjList[u].begin(), state.adjList[u].end(), v);
	if (itr != state.adjList[u].end()) { return false; }
	state.adjList[u].push_back(v);
	return true;
}

void deleteEdge(GraphState& state, size_t u, size_t v) {
	auto& adj = state.adjList[u];
	auto itr = std::find(adj.begin(), adj.end(), v);
	if (itr == adj.end()) { return; }
	std::swap(*itr, adj.back());
	adj.pop_back();
}

void deleteVertex(GraphState& state, size_t u) { state.valid[u] = false; }

void FilterGraph::addNode(const Filter& filter) {
	auto nodeIndex = nodes.size();
	nodes.emplace_back(filter, nextNodeId++);

	auto nodeVertexId = addVertex(state, nodeIndex, false, 0, false);

	for (auto i = 0u; i < filter.input.size(); ++i) {
		auto iVertexId = addVertex(state, nodeIndex, true, i, true);
		addEdge(state, iVertexId, nodeVertexId);
		nodes.back().inputSocketIds.push_back(iVertexId);
	}
	for (auto i = 0u; i < filter.output.size(); ++i) {
		auto oVertexId = addVertex(state, nodeIndex, true, i, false);
		addEdge(state, nodeVertexId, oVertexId);
		nodes.back().outputSocketIds.push_back(oVertexId);
	}
}

const LinkId FilterGraph::addLink(size_t u, size_t v) {
	if (!canAddEdge(state, nodes, u, v)) { return INVALID_LINK; }
	if (state.isInput[u]) { std::swap(u, v); }
	if (addEdge(state, u, v)) { return getLinkId(state, u, v); }
	return INVALID_LINK;
}

void FilterGraph::deleteNode(NodeId id) { deleteVertex(state, id.val); };

void FilterGraph::deleteLink(LinkId id) {
	size_t u, v;
	getUV(state, id, u, v);
	deleteEdge(state, u, v);
}

bool FilterGraph::canAddLink(size_t u, size_t v) {
	return canAddEdge(state, nodes, u, v);
}

void dfs(
	const GraphState& state, std::vector<bool>& marked,
	std::vector<size_t>& postOrder, size_t u) {
	marked[u] = true;
	if (!state.valid[u]) { return; }
	for (auto& v : state.adjList[u]) {
		if (marked[v]) { continue; }
		dfs(state, marked, postOrder, v);
	}
	if (state.isSocket[u]) { return; }
	postOrder.push_back(u);
}

void FilterGraph::iterateLinks(EdgeIterCallback cb, size_t u) {
	size_t b = 0u, e = state.valid.size();
	if (u != std::numeric_limits<size_t>::max()) {
		b = u;
		e = u + 1;
	}
	for (auto u = b; u < e; ++u) {
		if (!state.valid[u]) { return; }
		if (state.isSocket[u]) { return; }
		for (auto& v : state.adjList[u]) {
			for (auto& w : state.adjList[v]) {
				cb(getLinkId(state, v, w), v, w);
			}
		}
	}
}

void FilterGraph::iterateNodes(NodeIterCallback cb, NodeIterOrder order) {
	if (order == NodeIterOrder::Default) {
		for (auto i = 0u; i < state.adjList.size(); ++i) {
			if (state.valid[i] && !state.isSocket[i]) {
				cb(nodes[state.vertIdToNodeIndex[i]], i);
			}
		}
		return;
	}

	// Topological Sort
	std::vector<bool> marked(state.adjList.size(), false);
	std::vector<size_t> postOrder;
	for (auto i = 0u; i < state.adjList.size(); ++i) {
		if (!marked[i]) { dfs(state, marked, postOrder, i); }
	}
	for (auto itr = postOrder.rbegin(); itr != postOrder.rend(); ++itr) {
		cb(nodes[state.vertIdToNodeIndex[*itr]], *itr);
	}
}
