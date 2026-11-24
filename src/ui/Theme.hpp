#pragma once

#include <filesystem>
#include <imgui.h>

namespace rps::ui {

struct ThemePalette {
    ImVec4 bg_canvas      {0.039f, 0.055f, 0.086f, 1.00f}; // #0A0E16
    ImVec4 bg_panel       {0.067f, 0.094f, 0.141f, 1.00f}; // #111824
    ImVec4 bg_elevated    {0.098f, 0.133f, 0.196f, 1.00f}; // #192232
    ImVec4 border_subtle  {0.165f, 0.220f, 0.314f, 0.85f}; // #2A3850
    ImVec4 accent_cyan    {0.000f, 0.898f, 1.000f, 1.00f}; // #00E5FF
    ImVec4 accent_purple  {0.659f, 0.333f, 0.969f, 1.00f}; // #A855F7
    ImVec4 status_green   {0.063f, 0.725f, 0.506f, 1.00f}; // #10B981
    ImVec4 status_red     {0.937f, 0.267f, 0.267f, 1.00f}; // #EF4444
    ImVec4 status_amber   {0.961f, 0.620f, 0.043f, 1.00f}; // #F59E0B
    ImVec4 text_primary   {0.937f, 0.957f, 0.984f, 1.00f}; // #EFF4FB
    ImVec4 text_muted     {0.580f, 0.639f, 0.722f, 1.00f}; // #94A3B8
};

struct ThemeFonts {
    ImFont* regular = nullptr;
    ImFont* bold = nullptr;
    ImFont* display = nullptr;
};

[[nodiscard]] const ThemePalette& palette() noexcept;
[[nodiscard]] const ThemeFonts& fonts() noexcept;

void apply_esports_theme();
void load_fonts(const std::filesystem::path& preferred_font_path, float base_size_px = 18.0f);

} // namespace rps::ui
