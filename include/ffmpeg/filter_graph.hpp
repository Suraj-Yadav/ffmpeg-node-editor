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

class FilterGraph {
	std::vector<FilterNode> nodes;
	std::vector<Link> links;

	std::unordered_map<int, size_t> socketIdToNode;
	std::unordered_map<size_t, size_t> linkIdToLink;

	std::string name;

	int nextNodeId;
	size_t nextLinkId;

	bool getSocket(int socketId, Socket& s, bool& isInput) const;

	void deleteLink(size_t index);
	void deleteNode(size_t index);

   public:
	FilterGraph(const std::string& n = "Untitled")
		: name(n), nextNodeId(1), nextLinkId(1) {}

	void addNode(const Filter& filter);
	void deleteNode(NodeId id);
	void deleteLink(LinkId id);
	bool canAddLink(int id1, int id2);
	void addLink(int id1, int id2);

	inline const std::string& getName() const { return name; }
	inline const std::vector<FilterNode>& getNodes() const { return nodes; }
	inline const std::vector<Link>& getLinks() const { return links; }
};
