#pragma once

#include <absl/strings/string_view.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include <string>

#include "extras/IconsFontAwesome6.h"
#include "file_utils.hpp"

namespace ImGui {

	inline ImVec2 GetItemRectPoint(float wx = 0, float wy = 0) {
		auto a = GetItemRectMin();
		auto b = GetItemRectMax();
		return {a.x * (1 - wx) + b.x * wx, a.y * (1 - wy) + b.y * wy};
	}

	inline void Text(const std::string& str) { Text("%s", str.c_str()); }

	ImU32 ColorConvertHexToU32(absl::string_view hex);
	std::string ColorConvertU32ToHex(ImU32);

	inline auto ColorConvertFloat4ToHex(const ImVec4& col) {
		return ColorConvertU32ToHex(ColorConvertFloat4ToU32(col));
	}

	inline auto ColorConvertHexToFloat4(absl::string_view hex) {
		return ColorConvertU32ToFloat4(ColorConvertHexToU32(hex));
	}

	inline auto InputFont(const char* label, std::string& str) {
		if (InputText(label, &str)) { return true; }
		Spring(0, 0);
		if (Button(ICON_FA_FONT)) {
			auto path = selectFont();
			if (path.has_value()) {
				str = path.value().string();
				return true;
			}
		}
		return false;
	}

}  // namespace ImGui
