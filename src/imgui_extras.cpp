#include "imgui_extras.hpp"

#include <absl/strings/string_view.h>
#include <imgui.h>

#include <algorithm>

namespace ImGui {
	std::vector<float> rightEdge;

	void PushRightEdge(float width) {
		rightEdge.push_back(GetCursorPosX() + width);
	}
	void PopRightEdge() {
		if (!rightEdge.empty()) { rightEdge.pop_back(); }
	}

	void AlignedText(
		absl::string_view text, TextAlign align, float* maxTextWidth) {
		auto posX = GetCursorPosX();
		auto width = rightEdge.back() - posX;
		auto textWidth = CalcTextSize(text.data()).x;

		if (align == TextAlign::AlignCenter) {
			posX += (width - textWidth) / 2;
		}
		if (align == TextAlign::AlignRight) { posX += width - textWidth; }

		if (posX > ImGui::GetCursorPosX()) { ImGui::SetCursorPosX(posX); }
		Text("%s", text.data());

		if (maxTextWidth != nullptr) {
			*maxTextWidth = std::max(*maxTextWidth, textWidth);
		}
	}

	float GetRemainingWidth() { return rightEdge.back() - GetCursorPosX(); }

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
