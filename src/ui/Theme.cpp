#include "ui/Theme.hpp"
#include "core/Logger.hpp"

#include <algorithm>
#include <array>
#include <implot.h>
#include <string>

namespace rps::ui {

namespace {
ThemeFonts g_fonts;
float g_ui_scale = 1.0f;
} // namespace

const ThemePalette& palette() noexcept {
    static const ThemePalette p;
    return p;
}

const ThemeFonts& fonts() noexcept {
    return g_fonts;
}

float ui_scale() noexcept {
    return g_ui_scale;
}

void apply_esports_theme(float scale) {
    g_ui_scale = std::clamp(scale, 1.0f, 2.0f);
    const ThemePalette& p = palette();
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding = ImVec2(14.0f * g_ui_scale, 12.0f * g_ui_scale);
    style.FramePadding = ImVec2(10.0f * g_ui_scale, 6.0f * g_ui_scale);
    style.CellPadding = ImVec2(8.0f * g_ui_scale, 6.0f * g_ui_scale);
    style.ItemSpacing = ImVec2(10.0f * g_ui_scale, 8.0f * g_ui_scale);
    style.ItemInnerSpacing = ImVec2(6.0f * g_ui_scale, 6.0f * g_ui_scale);
    style.ScrollbarSize = 10.0f * g_ui_scale;
    style.GrabMinSize = 10.0f * g_ui_scale;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    style.WindowRounding = 0.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 5.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 5.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                 = p.text_primary;
    colors[ImGuiCol_TextDisabled]         = p.text_muted;
    colors[ImGuiCol_WindowBg]             = p.bg_canvas;
    colors[ImGuiCol_ChildBg]              = p.bg_panel;
    colors[ImGuiCol_PopupBg]              = p.bg_elevated;
    colors[ImGuiCol_Border]               = p.border_subtle;
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    colors[ImGuiCol_FrameBg]              = p.bg_elevated;
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.173f, 0.192f, 0.239f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.208f, 0.231f, 0.286f, 1.00f);

    colors[ImGuiCol_TitleBg]              = p.bg_panel;
    colors[ImGuiCol_TitleBgActive]        = p.bg_elevated;
    colors[ImGuiCol_TitleBgCollapsed]     = p.bg_canvas;
    colors[ImGuiCol_MenuBarBg]            = p.bg_panel;

    colors[ImGuiCol_ScrollbarBg]          = p.bg_panel;
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.240f, 0.267f, 0.325f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.320f, 0.355f, 0.430f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = p.accent_primary;

    colors[ImGuiCol_CheckMark]            = p.accent_primary;
    colors[ImGuiCol_SliderGrab]           = p.accent_primary;
    colors[ImGuiCol_SliderGrabActive]     = p.accent_hover;

    colors[ImGuiCol_Button]               = p.bg_elevated;
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.190f, 0.215f, 0.270f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = p.accent_primary;

    colors[ImGuiCol_Header]               = p.bg_elevated;
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.200f, 0.230f, 0.290f, 1.00f);
    colors[ImGuiCol_HeaderActive]         = p.accent_primary;

    colors[ImGuiCol_Separator]            = p.border_subtle;
    colors[ImGuiCol_SeparatorHovered]     = p.accent_primary;
    colors[ImGuiCol_SeparatorActive]      = p.accent_hover;

    colors[ImGuiCol_TableHeaderBg]        = p.bg_elevated;
    colors[ImGuiCol_TableRowBg]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt]        = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);

    ImPlotStyle& plot_style = ImPlot::GetStyle();
    plot_style.Colors[ImPlotCol_PlotBg]    = p.bg_canvas;
    plot_style.Colors[ImPlotCol_FrameBg]   = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    plot_style.Colors[ImPlotCol_PlotBorder]= p.border_subtle;
    plot_style.PlotPadding = ImVec2(6.0f, 6.0f);
}

void load_fonts(const std::filesystem::path& preferred_font_path, float base_size_px) {
    ImGuiIO& io = ImGui::GetIO();
    const float sz = base_size_px * g_ui_scale;

    auto try_load = [&](const std::filesystem::path& path, float px) -> ImFont* {
        std::error_code ec;
        if (!path.empty() && std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec)) {
            const std::string path_str = path.string();
            if (ImFont* f = io.Fonts->AddFontFromFileTTF(path_str.c_str(), px)) {
                core::Logger::info("Loaded UI font ({:.0f}px): {}", px, path_str);
                return f;
            }
        }
        return nullptr;
    };

    const std::array<std::filesystem::path, 4> regular_candidates = {
        preferred_font_path,
        std::filesystem::path("C:/Windows/Fonts/segoeui.ttf"),
        std::filesystem::path("C:/Windows/Fonts/consola.ttf"),
        std::filesystem::path("C:/Windows/Fonts/arial.ttf")
    };
    for (const auto& candidate : regular_candidates) {
        if (ImFont* f = try_load(candidate, sz)) {
            g_fonts.regular = f;
            break;
        }
    }

    if (g_fonts.regular == nullptr) {
        core::Logger::warn("No TTF font found; falling back to default ImGui font.");
        g_fonts.regular = io.Fonts->AddFontDefault();
    }

    const std::array<std::filesystem::path, 3> bold_candidates = {
        std::filesystem::path("C:/Windows/Fonts/segoeuib.ttf"),
        std::filesystem::path("C:/Windows/Fonts/consolab.ttf"),
        preferred_font_path
    };
    for (const auto& candidate : bold_candidates) {
        if (ImFont* f = try_load(candidate, sz + 1.0f)) {
            g_fonts.bold = f;
            break;
        }
    }
    if (g_fonts.bold == nullptr) {
        g_fonts.bold = g_fonts.regular;
    }

    for (const auto& candidate : bold_candidates) {
        if (ImFont* f = try_load(candidate, 44.0f * g_ui_scale)) {
            g_fonts.display = f;
            break;
        }
    }
    if (g_fonts.display == nullptr) {
        g_fonts.display = g_fonts.bold;
    }
}

} // namespace rps::ui
