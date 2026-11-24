#pragma once

#include <filesystem>
#include <imgui.h>

namespace rps::ui {

struct ThemePalette {
    ImVec4 bg_canvas      {0.067f, 0.075f, 0.094f, 1.00f}; // #111318
    ImVec4 bg_panel       {0.094f, 0.106f, 0.133f, 1.00f}; // #181B22
    ImVec4 bg_elevated    {0.133f, 0.149f, 0.188f, 1.00f}; // #222630
    ImVec4 border_subtle  {0.176f, 0.196f, 0.243f, 1.00f}; // #2D323E
    ImVec4 accent_primary {0.231f, 0.510f, 0.965f, 1.00f}; // #3B82F6
    ImVec4 accent_hover   {0.310f, 0.576f, 0.980f, 1.00f}; // #4F93FA
    ImVec4 status_green   {0.204f, 0.827f, 0.600f, 1.00f}; // #34D399
    ImVec4 status_red     {0.973f, 0.443f, 0.443f, 1.00f}; // #F87171
    ImVec4 status_amber   {0.984f, 0.749f, 0.141f, 1.00f}; // #FBBF24
    ImVec4 text_primary   {0.953f, 0.957f, 0.965f, 1.00f}; // #F3F4F6
    ImVec4 text_muted     {0.612f, 0.639f, 0.686f, 1.00f}; // #9CA3AF
};

struct ThemeFonts {
    ImFont* regular = nullptr;
    ImFont* bold = nullptr;
    ImFont* display = nullptr;
};

[[nodiscard]] const ThemePalette& palette() noexcept;
[[nodiscard]] const ThemeFonts& fonts() noexcept;
[[nodiscard]] float ui_scale() noexcept;

void apply_esports_theme(float scale = 1.0f);
void load_fonts(const std::filesystem::path& preferred_font_path, float base_size_px = 19.0f);

} // namespace rps::ui
