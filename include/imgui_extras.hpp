#pragma once

#include <imgui.h>
#include <imgui_stdlib.h>

#include <string>

#include "extras/IconsFontAwesome6.h"
#include "file_utils.hpp"
#include "util.hpp"

namespace ImGui {

	inline ImVec2 GetItemRectPoint(float wx = 0, float wy = 0) {
		auto a = GetItemRectMin();
		auto b = GetItemRectMax();
		return {a.x * (1 - wx) + b.x * wx, a.y * (1 - wy) + b.y * wy};
	}

	inline bool FontButton(const char* label) {
		return Button(label, {GetFrameHeight(), GetFrameHeight()});
	}

	inline ImVec2 CalcTextSize(const std::string& str) {
		return CalcTextSize(str.c_str());
	}
	inline void Text(const std::string& str) { TextUnformatted(str.c_str()); }
	inline void Text(const std::string_view& str) {
		TextUnformatted(str.data(), str.data() + str.size());
	}

	// Below functions this application specific
	ImU32 ColorConvertHexToU32(std::string_view hex);
	std::string ColorConvertU32ToHex(ImU32);

	inline auto ColorConvertFloat4ToHex(const ImVec4& col) {
		return ColorConvertU32ToHex(ColorConvertFloat4ToU32(col));
	}

	inline auto ColorConvertHexToFloat4(std::string_view hex) {
		return ColorConvertU32ToFloat4(ColorConvertHexToU32(hex));
	}

	inline bool InputFont(
		const char* label, std::string& str, float width = -1) {
#ifdef _WIN32
		PushItemWidth(std::max(width - GetFrameHeight(), 0.0f));
		defer w([&]() { PopItemWidth(); });
		if (InputText(label, &str)) { return true; }
		Spring(0, 0);
		if (FontButton(ICON_FA_FONT)) {
			auto path = selectFont();
			if (path.has_value()) {
				str = path.value().string();
				return true;
			}
		}
		return false;
#else
		return InputText(label, &str);
#endif
	}

	inline bool InputFile(
		const char* label, std::string& str, float width = -1) {
		PushItemWidth(std::max(width - GetFrameHeight(), 0.0f));
		defer w([&]() { PopItemWidth(); });
		if (InputText(label, &str)) { return true; }
		Spring(0, 0);
		if (FontButton(ICON_FA_FOLDER_OPEN)) {
			auto path = openFile();
			if (path.has_value()) {
				str = path->string();
				return true;
			}
		}
		return false;
	}

	inline bool InputColor(
		const char* label, std::string& str, float width = -1) {
		PushItemWidth(std::max(width - GetFrameHeight(), 0.0f));
		defer w([&]() { PopItemWidth(); });
		if (InputText(label, &str)) { return true; }
		Spring(0, 0);
		auto color = ColorConvertHexToFloat4(str);
		if (ColorEdit4(
				"##col", &color.x,
				ImGuiColorEditFlags_AlphaPreviewHalf |
					ImGuiColorEditFlags_AlphaBar |
					ImGuiColorEditFlags_NoOptions)) {
			str = ColorConvertFloat4ToHex(color);
			return true;
		}
		return false;
	}

	inline bool InputCheckbox(
		const char* label, std::string& str, float width = -1) {
		PushItemWidth(std::max(width - GetFrameHeight(), 0.0f));
		defer w([&]() { PopItemWidth(); });
		const int TRUE = 1, FALSE = 0, MAYBE = 2;
		int v = FALSE;
		if (str == "1") {
			v = TRUE | MAYBE;
		} else if (str != "0") {
			v = MAYBE;
		}
		if (InputText(label, &str)) { return true; }
		Spring(0, 0);
		if (CheckboxFlags("##b", &v, TRUE | MAYBE)) {
			if (v == FALSE) {
				str = "0";
			} else {
				str = "1";
			}
			return true;
		}
		return false;
	}

}  // namespace ImGui
