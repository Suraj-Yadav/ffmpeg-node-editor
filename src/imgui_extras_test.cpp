#include "imgui_extras.hpp"

#include <gtest/gtest.h>
#include <imgui.h>

#include <ios>

using namespace ImGui;

struct T {
	ImU32 v;
};

bool operator==(const T& a, const T& b) { return a.v == b.v; }

std::ostream& operator<<(std::ostream& os, const T& v) {
	os << std::hex << v.v << std::dec;
	return os;
}

TEST(ColorConvertHexToU32, Simple) {
	EXPECT_EQ(T{ColorConvertHexToU32("#FF0000")}, T{0xfF0000FF});
	EXPECT_EQ(T{ColorConvertHexToU32("0xFF0000")}, T{0xFF0000FF});
	EXPECT_EQ(T{ColorConvertHexToU32("#FF000000")}, T{0xFF000000});
	EXPECT_EQ(T{ColorConvertHexToU32("#FF00FF")}, T{0xFF00FFFF});
}

TEST(ColorConvertHexToU32, Constants) {
	EXPECT_EQ(T{ColorConvertHexToU32("black")}, T{0x000000FF});
	EXPECT_EQ(T{ColorConvertHexToU32("red")}, T{0xFF0000FF});
	EXPECT_EQ(T{ColorConvertHexToU32("green")}, T{0x00FF00FF});
	EXPECT_EQ(T{ColorConvertHexToU32("blue")}, T{0x0000FFFF});
	EXPECT_EQ(T{ColorConvertHexToU32("pink")}, T{0xFFC0CBFF});
}

TEST(ColorConvertHexToU32, Alpha) {
	EXPECT_EQ(T{ColorConvertHexToU32("#FF0000@0x23")}, T{0xFF000023});
	EXPECT_EQ(T{ColorConvertHexToU32("#FF0000@0.5")}, T{0xFF00007F});
	EXPECT_EQ(T{ColorConvertHexToU32("red@0x23")}, T{0xFF000023});
	EXPECT_EQ(T{ColorConvertHexToU32("red@0.5")}, T{0xFF00007F});
}
