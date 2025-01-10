#pragma once

#include <imgui.h>
#include <imgui_stdlib.h>

#include <string>

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

	bool InputFont(const char* label, std::string& str, float width = -1);

	bool InputFile(const char* label, std::string& str, float width = -1);

	bool InputColor(const char* label, std::string& str, float width = -1);

	bool InputCheckbox(const char* label, std::string& str, float width = -1);

	inline int UnsavedDocumentFlag(
		bool unsaved, int flag = ImGuiWindowFlags_None) {
		if (unsaved) { return flag | ImGuiWindowFlags_UnsavedDocument; }
		return flag;
	}

	enum class UnsavedDocumentAction {
		NO_OP,
		SAVE_CHANGES,
		CANCEL_CLOSE,
		DISCARD_CHANGES
	};

	UnsavedDocumentAction UnsavedDocumentClose(
		bool unsaved, bool open, std::string const& title,
		std::string const& text);

}  // namespace ImGui
