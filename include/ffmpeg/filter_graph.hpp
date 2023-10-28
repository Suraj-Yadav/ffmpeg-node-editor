#pragma once

#include <absl/strings/string_view.h>
#include <vcruntime.h>

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ffmpeg/filter.hpp"
#include "ffmpeg/filter_node.hpp"
#include "filter_node.hpp"

enum NodeIterOrder { Default, Topological };

using EdgeIterCallback =
	std::function<void(const LinkId& id, const size_t& u, const size_t& v)>;

using NodeIterCallback =
	std::function<void(const FilterNode&, const size_t& u)>;

struct GraphState {
	std::vector<bool> valid;
	std::vector<bool> isSocket;
	std::vector<bool> isInput;
	std::vector<size_t> vertIdToNodeIndex;
	std::vector<size_t> vertIdToSocketIndex;
	std::vector<std::vector<size_t>> adjList;
};

class FilterGraph {
	std::vector<FilterNode> nodes;

	GraphState state;

	std::string name;

	int nextNodeId;
	size_t nextLinkId;

   public:
	FilterGraph(const std::string& n = "Untitled")
		: name(n), nextNodeId(1), nextLinkId(1) {}

	void addNode(const Filter& filter);
	void deleteNode(NodeId id);
	void deleteLink(LinkId id);
	bool canAddLink(size_t u, size_t v);
	const LinkId addLink(size_t u, size_t v);

	void iterateNodes(
		NodeIterCallback cb, NodeIterOrder order = NodeIterOrder::Default);

	void iterateLinks(
		EdgeIterCallback cb, size_t u = std::numeric_limits<size_t>::max());

	inline const std::string& getName() const { return name; }
};
