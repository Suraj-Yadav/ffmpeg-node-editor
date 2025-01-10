#pragma once

#include <utility>
#include <vector>

#include "ffmpeg/filter.hpp"
#include "ffmpeg/runner.hpp"

struct Profile {
	std::vector<Filter> filters;
	Runner runner;

	Profile(Runner r) : runner(std::move(r)) {}
};

Profile GetProfile();
