#pragma once
#include <absl/strings/string_view.h>

#include <map>

#include "ffmpeg/filter.hpp"

struct FilterNode {
	const Filter& ref;
	std::map<absl::string_view, std::string> option;

	FilterNode(const Filter& f) : ref(f) {}
};
