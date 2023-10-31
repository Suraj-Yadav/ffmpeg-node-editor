#pragma once

#include <absl/strings/string_view.h>
#include <vcruntime.h>

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ffmpeg/filter.hpp"
#include "ffmpeg/filter_graph.hpp"
#include "ffmpeg/filter_node.hpp"
#include "filter_node.hpp"

enum NodeIterOrder { Default, Topological };

using EdgeIterCallback =
	std::function<void(const LinkId& id, const NodeId& u, const NodeId& v)>;

using NodeIterCallback =
	std::function<void(const FilterNode&, const NodeId& id)>;

using InputSocketCallback = std::function<void(
	const Socket&, const NodeId& id, const NodeId& parentId)>;

using OutputSocketCallback =
	std::function<void(const Socket&, const NodeId& id)>;

struct GraphState {
	std::vector<bool> valid;
	std::vector<bool> isSocket;
	std::vector<bool> isInput;
	std::vector<size_t> vertIdToNodeIndex;
	std::vector<size_t> vertIdToSocketIndex;
	// std::vector<std::vector<size_t>> adjList;
	std::vector<std::vector<size_t>> revAdjList;
};

class FilterGraph {
	std::vector<FilterNode> nodes;

	GraphState state;

	std::string name;

   public:
	FilterGraph(const std::string& n = "Untitled") : name(n) {}

	void addNode(const Filter& filter);
	void deleteNode(NodeId id);
	void deleteLink(LinkId id);
	bool canAddLink(NodeId u, NodeId v);
	const LinkId addLink(NodeId u, NodeId v);

	void iterateNodes(
		NodeIterCallback cb, NodeIterOrder order = NodeIterOrder::Default,
		NodeId u = INVALID_NODE);

	void iterateLinks(EdgeIterCallback cb);

	void inputSockets(NodeId u, InputSocketCallback cb);
	void outputSockets(NodeId u, OutputSocketCallback cb);

	inline const std::string& getName() const { return name; }
};
