#pragma once

#include <functional>
#include <vector>

#include "filter_node.hpp"
#include "pref.hpp"

enum class NodeIterOrder { Default, Topological };

struct Profile;

using EdgeIterCallback =
	std::function<void(const LinkId& id, const NodeId& u, const NodeId& v)>;

using NodeIterCallback =
	std::function<void(const FilterNode&, const NodeId& id)>;

using InputSocketCallback = std::function<void(
	const Socket&, const NodeId& id, const NodeId& parentId)>;

using OutputSocketCallback =
	std::function<void(const Socket&, const NodeId& id)>;

struct GraphState {
	bool changed = false;
	std::vector<bool> valid;
	std::vector<bool> isSocket;
	std::vector<bool> isInput;
	std::vector<size_t> vertIdToNodeIndex;
	std::vector<size_t> vertIdToSocketIndex;
	std::vector<std::vector<size_t>> adjList;
	std::vector<std::vector<size_t>> revAdjList;
};

enum class FilterGraphErrorCode {
	PLAYER_NO_ERROR,
	PLAYER_MISSING_INPUT,
	PLAYER_RUNTIME
};
struct FilterGraphError {
	FilterGraphErrorCode code;
	std::string message;
};

class FilterGraph {
	std::vector<FilterNode> nodes;
	GraphState state;
	const Profile* profile;

   public:
	FilterGraph(const Profile& p) : profile(&p) {}

	NodeId addNode(const Filter& filter);
	void deleteNode(NodeId id);
	void deleteLink(LinkId id);
	[[nodiscard]] bool canAddLink(NodeId u, NodeId v) const;
	LinkId addLink(NodeId u, NodeId v);

	void optHook(const NodeId& id, const int& optId, const std::string& value);

	void iterateNodes(
		const NodeIterCallback& cb,
		NodeIterOrder order = NodeIterOrder::Default,
		NodeId u = INVALID_NODE) const;

	void iterateLinks(const EdgeIterCallback& cb) const;

	void inputSockets(NodeId u, const InputSocketCallback& cb) const;
	void outputSockets(NodeId u, const OutputSocketCallback& cb) const;

	[[nodiscard]] const FilterNode& getNode(NodeId id) const;
	FilterNode& getNode(NodeId id);

	[[nodiscard]] const std::vector<Filter>& allFilters() const;

	void clear();

	FilterGraphError play(
		const Preference& pref, const NodeId& id = INVALID_NODE);

	[[nodiscard]] bool changed() const { return state.changed; }
	void resetChanged() { state.changed = false; }
};
