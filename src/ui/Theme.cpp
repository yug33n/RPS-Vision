#include "ui/Theme.hpp"
#include "core/Logger.hpp"

#include <array>
#include <implot.h>
#include <string>

namespace rps::ui {

namespace {
ThemeFonts g_fonts;
} // namespace

const ThemePalette& palette() noexcept {
    static const ThemePalette p;
    return p;
}

const ThemeFonts& fonts() noexcept {
    return g_fonts;
}

void apply_esports_theme() {
    const ThemePalette& p = palette();
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding = ImVec2(16.0f, 14.0f);
    style.FramePadding = ImVec2(10.0f, 7.0f);
    style.CellPadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 10.0f;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    style.WindowRounding = 0.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
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

    colors[ImGuiCol_ScrollbarBg]          = p.bg_panel;
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.220f, 0.282f, 0.392f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = p.accent_cyan;
    colors[ImGuiCol_ScrollbarGrabActive]  = p.accent_purple;

    colors[ImGuiCol_CheckMark]            = p.accent_cyan;
    colors[ImGuiCol_SliderGrab]           = p.accent_cyan;
    colors[ImGuiCol_SliderGrabActive]     = p.accent_purple;

    colors[ImGuiCol_Button]               = p.bg_elevated;
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.000f, 0.620f, 0.720f, 0.85f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.000f, 0.780f, 0.900f, 1.00f);

    colors[ImGuiCol_Header]               = ImVec4(0.137f, 0.192f, 0.282f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.000f, 0.560f, 0.660f, 0.65f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.000f, 0.700f, 0.820f, 0.85f);

    colors[ImGuiCol_Separator]            = p.border_subtle;
    colors[ImGuiCol_SeparatorHovered]     = p.accent_cyan;
    colors[ImGuiCol_SeparatorActive]      = p.accent_purple;

    colors[ImGuiCol_TableHeaderBg]        = p.bg_elevated;
    colors[ImGuiCol_TableRowBg]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt]        = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);

    ImPlotStyle& plot_style = ImPlot::GetStyle();
    plot_style.Colors[ImPlotCol_PlotBg]    = ImVec4(0.043f, 0.059f, 0.090f, 0.65f);
    plot_style.Colors[ImPlotCol_FrameBg]   = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    plot_style.Colors[ImPlotCol_PlotBorder]= p.border_subtle;
    plot_style.PlotPadding = ImVec2(6.0f, 6.0f);
}

void load_fonts(const std::filesystem::path& preferred_font_path, float base_size_px) {
    ImGuiIO& io = ImGui::GetIO();

    auto try_load = [&](const std::filesystem::path& path, float size) -> ImFont* {
        std::error_code ec;
        if (!path.empty() && std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec)) {
            const std::string path_str = path.string();
            if (ImFont* f = io.Fonts->AddFontFromFileTTF(path_str.c_str(), size)) {
                core::Logger::info("Loaded UI font ({:.0f}px): {}", size, path_str);
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
        if (ImFont* f = try_load(candidate, base_size_px)) {
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
        if (ImFont* f = try_load(candidate, base_size_px + 1.0f)) {
            g_fonts.bold = f;
            break;
        }
    }
    if (g_fonts.bold == nullptr) {
        g_fonts.bold = g_fonts.regular;
    }

    for (const auto& candidate : bold_candidates) {
        if (ImFont* f = try_load(candidate, 46.0f)) {
            g_fonts.display = f;
            break;
        }
    }
    if (g_fonts.display == nullptr) {
        g_fonts.display = g_fonts.bold;
    }
}

} // namespace rps::ui
