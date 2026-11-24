#include "ui/Theme.hpp"
#include "core/Logger.hpp"

#include <array>
#include <string>

namespace rps::ui {

const ThemePalette& palette() noexcept {
    static const ThemePalette p;
    return p;
}

void apply_esports_theme() {
    const ThemePalette& p = palette();
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding = ImVec2(14.0f, 12.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.CellPadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    style.WindowRounding = 8.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                 = p.text_primary;
    colors[ImGuiCol_TextDisabled]         = p.text_muted;
    colors[ImGuiCol_WindowBg]             = p.bg_canvas;
    colors[ImGuiCol_ChildBg]              = p.bg_panel;
    colors[ImGuiCol_PopupBg]              = p.bg_elevated;
    colors[ImGuiCol_Border]               = p.border_subtle;
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    colors[ImGuiCol_FrameBg]              = p.bg_elevated;
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.145f, 0.192f, 0.278f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.180f, 0.239f, 0.345f, 1.00f);

    colors[ImGuiCol_TitleBg]              = p.bg_panel;
    colors[ImGuiCol_TitleBgActive]        = p.bg_elevated;
    colors[ImGuiCol_TitleBgCollapsed]     = p.bg_canvas;
    colors[ImGuiCol_MenuBarBg]            = p.bg_panel;

    colors[ImGuiCol_ScrollbarBg]          = p.bg_canvas;
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.220f, 0.282f, 0.392f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = p.accent_cyan;
    colors[ImGuiCol_ScrollbarGrabActive]  = p.accent_purple;

    colors[ImGuiCol_CheckMark]            = p.accent_cyan;
    colors[ImGuiCol_SliderGrab]           = p.accent_cyan;
    colors[ImGuiCol_SliderGrabActive]     = p.accent_purple;

    colors[ImGuiCol_Button]               = ImVec4(0.118f, 0.173f, 0.255f, 1.00f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.000f, 0.620f, 0.720f, 0.85f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.000f, 0.780f, 0.900f, 1.00f);

    colors[ImGuiCol_Header]               = ImVec4(0.137f, 0.192f, 0.282f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.000f, 0.560f, 0.660f, 0.65f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.000f, 0.700f, 0.820f, 0.85f);

    colors[ImGuiCol_Separator]            = p.border_subtle;
    colors[ImGuiCol_SeparatorHovered]     = p.accent_cyan;
    colors[ImGuiCol_SeparatorActive]      = p.accent_purple;

    colors[ImGuiCol_Tab]                  = p.bg_elevated;
    colors[ImGuiCol_TabHovered]           = ImVec4(0.000f, 0.560f, 0.660f, 0.75f);
    colors[ImGuiCol_TabSelected]          = ImVec4(0.000f, 0.480f, 0.580f, 0.90f);
    colors[ImGuiCol_PlotLines]            = p.accent_cyan;
    colors[ImGuiCol_PlotHistogram]        = p.accent_purple;
}

void load_fonts(const std::filesystem::path& preferred_font_path, float base_size_px) {
    ImGuiIO& io = ImGui::GetIO();

    const std::array<std::filesystem::path, 4> candidates = {
        preferred_font_path,
        std::filesystem::path("C:/Windows/Fonts/segoeui.ttf"),
        std::filesystem::path("C:/Windows/Fonts/consola.ttf"),
        std::filesystem::path("C:/Windows/Fonts/arial.ttf")
    };

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (!candidate.empty() && std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec)) {
            const std::string path_str = candidate.string();
            ImFont* loaded = io.Fonts->AddFontFromFileTTF(path_str.c_str(), base_size_px);
            if (loaded != nullptr) {
                core::Logger::info("Loaded UI font: {}", path_str);
                return;
            }
        }
    }

    core::Logger::warn("No TTF font found; falling back to default ImGui font.");
    io.Fonts->AddFontDefault();
}

} // namespace rps::ui
