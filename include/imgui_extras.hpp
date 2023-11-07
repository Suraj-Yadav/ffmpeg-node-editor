#include <absl/strings/string_view.h>
#include <imgui.h>

#include <string>
#include <vector>

namespace ImGui {

	enum TextAlign { AlignLeft, AlignCenter, AlignRight };

	inline ImVec2 GetItemRectPoint(float wx = 0, float wy = 0) {
		auto a = GetItemRectMin();
		auto b = GetItemRectMax();
		return {a.x * (1 - wx) + b.x * wx, a.y * (1 - wy) + b.y * wy};
	}

	void PushRightEdge(float width);
	void PopRightEdge();

	float GetRemainingWidth();
	void AlignedText(
		absl::string_view text, TextAlign align = TextAlign::AlignLeft,
		float* maxTextWidth = nullptr);

	inline void AlignedText(
		absl::string_view text, float* maxTextWidth = nullptr) {
		AlignedText(text, TextAlign::AlignLeft, maxTextWidth);
	}

	const int Corner_TopLeft = 1 << 0;
	const int Corner_TopRight = 1 << 1;
	const int Corner_BottomRight = 1 << 2;
	const int Corner_BottomLeft = 1 << 3;

	void AddRoundedFilledRect(
		ImDrawList* list, const ImVec2& a, const ImVec2& b, const float r,
		const ImColor& col,
		int corners = Corner_TopLeft | Corner_TopRight | Corner_BottomRight |
					  Corner_BottomLeft);

}  // namespace ImGui
