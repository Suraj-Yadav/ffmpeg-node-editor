#include "ffmpeg/runner.hpp"

#include <gtest/gtest.h>

#include "string_utils.hpp"

TEST(Runner, Simple) {
	Runner runner;
	EXPECT_EQ(runner.lineScanner({"-version"}, nullptr), 0);
}

TEST(Runner, play_success) {
	Runner runner;
	auto val = runner.play({}, "testsrc", {}, "file\n%f");
	EXPECT_EQ(val.first, 0);
}

TEST(Runner, play_fail) {
	Runner runner;
	auto val = runner.play({}, "tessrc", {}, "file\n%f");
	EXPECT_NE(val.first, 0);
}
