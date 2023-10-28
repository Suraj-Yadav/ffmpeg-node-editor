#pragma once
#include <absl/strings/string_view.h>
#include <vcruntime.h>

#include <map>
#include <vector>

#include "ffmpeg/filter.hpp"
#include "ffmpeg/filter_node.hpp"

#define STRONG_TYPEDEF(NEW_TYPE, ORIG_TYPE)                                   \
	struct NEW_TYPE {                                                         \
		ORIG_TYPE val;                                                        \
		bool operator==(const NEW_TYPE& rhs) const { return val == rhs.val; } \
	};

STRONG_TYPEDEF(NodeId, size_t);
STRONG_TYPEDEF(LinkId, size_t);

const LinkId INVALID_LINK{0};

class FilterNode {
	std::reference_wrapper<const Filter> ref;

   public:
	NodeId id;
	std::string name;
	std::map<absl::string_view, std::string> option;
	std::vector<size_t> inputSocketIds;
	std::vector<size_t> outputSocketIds;

	FilterNode(const Filter& f, int i) : ref(f), id(i), name(f.name) {}

	const std::vector<Socket>& input() const { return ref.get().input; }
	const std::vector<Socket>& output() const { return ref.get().output; }
	const Filter& base() const { return ref; }
};

struct Link {
	LinkId id;
	int src;
	int dest;
};
