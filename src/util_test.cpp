#include <gtest/gtest.h>

#include "string_utils.hpp"

TEST(str, starts_with) {
	EXPECT_EQ(str::starts_with("abcdef", ""), true);
	EXPECT_EQ(str::starts_with("abcdef", "abcd"), true);
	EXPECT_EQ(str::starts_with("abcef", "abcd"), false);
	EXPECT_EQ(str::starts_with("ABCDEF", "abcd"), false);
	EXPECT_EQ(str::starts_with("ABCEF", "abcd"), false);
	EXPECT_EQ(str::starts_with("ABCDEF", "abcd", true), true);
	EXPECT_EQ(str::starts_with("ABCEF", "abcd", true), false);
}

TEST(str, ends_with) {
	EXPECT_EQ(str::ends_with("fedcba", ""), true);
	EXPECT_EQ(str::ends_with("fedcba", "dcba"), true);
	EXPECT_EQ(str::ends_with("fecba", "dcba"), false);
	EXPECT_EQ(str::ends_with("FEDCBA", "dcba"), false);
	EXPECT_EQ(str::ends_with("FECBA", "dcba"), false);
	EXPECT_EQ(str::ends_with("FEDCBA", "dcba", true), true);
	EXPECT_EQ(str::ends_with("FECBA", "dcba", true), false);
}

TEST(str, contains) {
	EXPECT_EQ(str::contains("abcdef", ""), true);
	EXPECT_EQ(str::contains("abcdef", "bcd"), true);
	EXPECT_EQ(str::contains("abcef", "bcd"), false);
	EXPECT_EQ(str::contains("ABCDEF", "bcd"), false);
	EXPECT_EQ(str::contains("ABCEF", "bcd"), false);
	EXPECT_EQ(str::contains("ABCDEF", "bcd", true), true);
	EXPECT_EQ(str::contains("ABCEF", "bcd", true), false);
}

TEST(str, strip) {
	EXPECT_EQ(str::strip("abcd"), "abcd");
	EXPECT_EQ(str::strip("abcd   "), "abcd");
	EXPECT_EQ(str::strip("    abcd"), "abcd");
	EXPECT_EQ(str::strip("    abcd    "), "abcd");
}

TEST(str, match) {
	std::string_view str("how are you?"), how, are, you;
	std::regex re1(R"((\w+) (\w+) (\w+)\?)");
	EXPECT_TRUE(str::match(str, re1, {how, are, you}));
	EXPECT_EQ(how, "how");
	EXPECT_EQ(are, "are");
	EXPECT_EQ(you, "you");

	EXPECT_FALSE(str::match(str, re1, {how, are}));
	EXPECT_EQ(how, "how");
	EXPECT_EQ(are, "are");
}

TEST(str, split) {
	EXPECT_EQ(
		str::split("how are you", ' '),
		(std::vector<std::string_view>{"how", "are", "you"}));
	EXPECT_EQ(
		str::split("how     are     you", ' '),
		(std::vector<std::string_view>{"how", "are", "you"}));
}
