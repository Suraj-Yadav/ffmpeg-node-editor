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
	EXPECT_EQ(T{ColorConvertHexToU32("#FF0000")}, T{IM_COL32(255, 0, 0, 255)});
	EXPECT_EQ(T{ColorConvertHexToU32("0xFF0000")}, T{IM_COL32(255, 0, 0, 255)});
	EXPECT_EQ(T{ColorConvertHexToU32("#FF000000")}, T{IM_COL32(255, 0, 0, 0)});
	EXPECT_EQ(
		T{ColorConvertHexToU32("#FF00FF")}, T{IM_COL32(255, 0, 255, 255)});
}

TEST(ColorConvertHexToU32, Constants) {
	EXPECT_EQ(T{ColorConvertHexToU32("black")}, T{IM_COL32(0, 0, 0, 255)});
	EXPECT_EQ(T{ColorConvertHexToU32("red")}, T{IM_COL32(255, 0, 0, 255)});
	EXPECT_EQ(T{ColorConvertHexToU32("green")}, T{IM_COL32(0, 255, 0, 255)});
	EXPECT_EQ(T{ColorConvertHexToU32("blue")}, T{IM_COL32(0, 0, 255, 255)});
	EXPECT_EQ(T{ColorConvertHexToU32("pink")}, T{IM_COL32(255, 192, 203, 255)});
}

TEST(ColorConvertHexToU32, Alpha) {
	EXPECT_EQ(
		T{ColorConvertHexToU32("#FF0000@0x23")}, T{IM_COL32(255, 0, 0, 35)});
	EXPECT_EQ(
		T{ColorConvertHexToU32("#FF0000@0.5")}, T{IM_COL32(255, 0, 0, 127)});
	EXPECT_EQ(T{ColorConvertHexToU32("red@0x23")}, T{IM_COL32(255, 0, 0, 35)});
	EXPECT_EQ(T{ColorConvertHexToU32("red@0.5")}, T{IM_COL32(255, 0, 0, 127)});
}

TEST(ColorConvertU32ToHex, Simple) {
	EXPECT_EQ(ColorConvertU32ToHex(IM_COL32(255, 0, 0, 255)), "#FF0000FF");
}
