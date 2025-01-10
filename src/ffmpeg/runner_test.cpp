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
	EXPECT_EQ(val.second, "");
}

TEST(Runner, play_fail) {
	Runner runner;
	auto val = runner.play({}, "tessrc", {}, "file\n%f");
	EXPECT_NE(val.first, 0);
}

TEST(Runner, getInfo) {
	Runner runner;
	auto val = runner.getInfo("./test/temp427506003.mkv");
	EXPECT_EQ(val.streams.size(), 3);
	EXPECT_EQ(val.streams[0].type, "video");
	EXPECT_EQ(val.streams[1].type, "audio");
	EXPECT_EQ(val.streams[2].type, "audio");
}
