#pragma once

#include <map>
#include <vector>

#include "ffmpeg/filter.hpp"

#define STRONG_TYPEDEF(NEW_TYPE, ORIG_TYPE)                                   \
	struct NEW_TYPE {                                                         \
		ORIG_TYPE val;                                                        \
		bool operator==(const NEW_TYPE& rhs) const { return val == rhs.val; } \
		bool operator!=(const NEW_TYPE& rhs) const { return val != rhs.val; } \
	};

using IdBaseType = size_t;

STRONG_TYPEDEF(NodeId, IdBaseType);
STRONG_TYPEDEF(LinkId, IdBaseType);

const NodeId INVALID_NODE{0};
const LinkId INVALID_LINK{0};

class FilterNode {
	std::reference_wrapper<const Filter> ref;

   public:
	std::string name;
	std::map<int, std::string> option;
	std::vector<NodeId> inputSocketIds;
	std::vector<NodeId> outputSocketIds;
	std::vector<Socket> inputSockets;
	std::vector<Socket> outputSockets;

	FilterNode(const Filter& f) : ref(f), name(f.name) {}

	const std::vector<Socket>& input() const {
		if (base().dynamicInput) { return inputSockets; }
		return ref.get().input;
	}
	const std::vector<Socket>& output() const {
		if (base().dynamicOutput) { return outputSockets; }
		return ref.get().output;
	}
	const Filter& base() const { return ref; }
};

struct Link {
	LinkId id;
	int src;
	int dest;
};
