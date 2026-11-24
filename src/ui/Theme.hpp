#pragma once

#include <filesystem>
#include <imgui.h>

namespace rps::ui {

struct ThemePalette {
    ImVec4 bg_canvas      {0.043f, 0.059f, 0.090f, 1.00f}; // #0B0F17
    ImVec4 bg_panel       {0.075f, 0.102f, 0.149f, 1.00f}; // #131A26
    ImVec4 bg_elevated    {0.106f, 0.141f, 0.204f, 1.00f}; // #1B2434
    ImVec4 border_subtle  {0.165f, 0.216f, 0.306f, 0.85f}; // #2A374E
    ImVec4 accent_cyan    {0.000f, 0.898f, 1.000f, 1.00f}; // #00E5FF
    ImVec4 accent_purple  {0.659f, 0.333f, 0.969f, 1.00f}; // #A855F7
    ImVec4 status_green   {0.063f, 0.725f, 0.506f, 1.00f}; // #10B981
    ImVec4 status_red     {0.937f, 0.267f, 0.267f, 1.00f}; // #EF4444
    ImVec4 status_amber   {0.961f, 0.620f, 0.043f, 1.00f}; // #F59E0B
    ImVec4 text_primary   {0.929f, 0.949f, 0.980f, 1.00f}; // #EDF2FA
    ImVec4 text_muted     {0.580f, 0.639f, 0.722f, 1.00f}; // #94A3B8
};

/**
 * @brief Returns the active esports dark color palette.
 */
[[nodiscard]] const ThemePalette& palette() noexcept;

/**
 * @brief Applies the custom dark esports Dear ImGui styling and colors.
 */
void apply_esports_theme();

/**
 * @brief Loads Latin + Cyrillic fonts with graceful fallback to system fonts or default atlas.
 */
void load_fonts(const std::filesystem::path& preferred_font_path, float base_size_px = 17.0f);

} // namespace rps::ui
