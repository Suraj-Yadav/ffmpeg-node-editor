#include "imgui_extras.hpp"

#include <imgui.h>

namespace ImGui {
	void AddRoundedFilledRect(
		ImDrawList* list, const ImVec2& a, const ImVec2& b, const float r,
		const ImColor& col, int corners) {
		list->PathClear();
		if (corners & Corner_TopLeft) {
			list->PathArcToFast({a.x + r, a.y + r}, r, 6, 9);
		} else {
			list->PathLineTo({a.x, a.y});
		}
		if (corners & Corner_TopRight) {
			list->PathArcToFast({b.x - r, a.y + r}, r, 9, 12);
		} else {
			list->PathLineTo({b.x, a.y});
		}
		if (corners & Corner_BottomRight) {
			list->PathArcToFast({b.x - r, b.y - r}, r, 0, 3);
		} else {
			list->PathLineTo({b.x, b.y});
		}
		if (corners & Corner_BottomLeft) {
			list->PathArcToFast({a.x + r, b.y - r}, r, 3, 6);
		} else {
			list->PathLineTo({a.x, b.y});
		}
		list->PathFillConvex(col);
	}
};	// namespace ImGui
