#include <vector>

#include "ffmpeg/filter.hpp"
#include "ffmpeg/runner.hpp"

class Profile {
   public:
	std::vector<Filter> filters;
	Runner runner;

	Profile(Runner r) : runner(r) {}
};

Profile GetProfile();
