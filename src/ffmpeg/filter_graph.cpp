#include "ffmpeg/filter_graph.hpp"

bool FilterGraph::getSocket(int socketId, Socket& s, bool& isInput) const {
	const auto itr = socketIdToNode.find(socketId);
	if (itr == socketIdToNode.end()) { return false; }
	const auto& node = nodes[itr->second];
	int i = 0;
	isInput = true;
	for (auto& elem : node.inputSocketIds) {
		if (elem == socketId) {
			s = node.input()[i];
			return true;
		}
		i++;
	}
	i = 0;
	isInput = false;
	for (auto& elem : node.outputSocketIds) {
		if (elem == socketId) {
			s = node.output()[i];
			return true;
		}
		i++;
	}
	return false;
}

void FilterGraph::deleteLink(size_t index) {
	if (index >= links.size()) { return; }
	std::swap(links[index], links.back());
	links.pop_back();
}

void FilterGraph::deleteNode(size_t index) {
	if (index >= nodes.size()) { return; }
	const auto nodeId = nodes[index].id;
	for (auto i = 0u; i < links.size(); ++i) {
		if (nodes[socketIdToNode[links[i].src]].id == nodeId ||
			nodes[socketIdToNode[links[i].dest]].id == nodeId) {
			deleteLink(i);
		}
	}
	std::swap(nodes[index], nodes.back());
	nodes.pop_back();
}

void FilterGraph::addNode(const Filter& filter) {
	auto index = nodes.size();
	nodes.emplace_back(filter, nextNodeId++);
	for (auto i = 0u; i < filter.input.size(); ++i) {
		socketIdToNode[nextNodeId] = index;
		nodes.back().inputSocketIds.emplace_back(nextNodeId++);
	}
	for (auto i = 0u; i < filter.output.size(); ++i) {
		socketIdToNode[nextNodeId] = index;
		nodes.back().outputSocketIds.emplace_back(nextNodeId++);
	}
}

void FilterGraph::deleteNode(NodeId id) {
	for (auto i = 0u; i < nodes.size(); ++i) {
		if (nodes[i].id == id) {
			deleteNode(i);
			return;
		}
	}
};

void FilterGraph::deleteLink(LinkId id) {
	for (auto i = 0u; i < links.size(); ++i) {
		if (links[i].id == id) {
			deleteLink(i);
			return;
		}
	}
}

bool FilterGraph::canAddLink(int id1, int id2) {
	Socket socket1, socket2;
	bool isInput1, isInput2;
	if (!getSocket(id1, socket1, isInput1) ||
		!getSocket(id2, socket2, isInput2)) {
		return false;
	}
	if (isInput1 == isInput2) { return false; }
	if (socket1.type != socket2.type) { return false; }
	return true;
}

void FilterGraph::addLink(int id1, int id2) {
	linkIdToLink[nextLinkId] = links.size();
	links.push_back({{nextLinkId++}, id1, id2});
}
