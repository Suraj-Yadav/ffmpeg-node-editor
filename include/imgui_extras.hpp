#include <imgui.h>

#include <string>

namespace ImGui {

	inline ImVec2 GetItemRectPoint(float wx = 0, float wy = 0) {
		auto a = GetItemRectMin();
		auto b = GetItemRectMax();
		return {a.x * (1 - wx) + b.x * wx, a.y * (1 - wy) + b.y * wy};
	}

	inline void Text(const std::string& str) { Text("%s", str.c_str()); }

}  // namespace ImGui
