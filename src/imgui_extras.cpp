#include "imgui_extras.hpp"

namespace ImGui {
	std::vector<float> rightEdge;

	void PushRightEdge(float width) {
		rightEdge.push_back(GetCursorPosX() + width);
	}
	void PopRightEdge() {
		if (!rightEdge.empty()) { rightEdge.pop_back(); }
	}

	void AlignedText(const std::string& text, TextAlign align) {
		if (align != TextAlign::AlignLeft && rightEdge.empty()) {
			align = TextAlign::AlignLeft;
		}

		auto posX = GetCursorPosX();
		auto width = rightEdge.back() - posX;

		if (align == TextAlign::AlignCenter) {
			posX += (width - CalcTextSize(text.c_str()).x) / 2;
		}
		if (align == TextAlign::AlignRight) {
			posX += width - CalcTextSize(text.c_str()).x;
		}

		if (posX > ImGui::GetCursorPosX()) { ImGui::SetCursorPosX(posX); }
		Text("%s", text.c_str());
	}

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
