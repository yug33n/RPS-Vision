#include "ui/ArenaUI.hpp"
#include "ui/Theme.hpp"

#include <algorithm>
#include <format>
#include <imgui.h>
#include <string>

namespace rps::ui {

namespace {

ImU32 to_u32(const ImVec4& c) noexcept {
    return ImGui::ColorConvertFloat4ToU32(c);
}

void draw_corner_reticle(ImDrawList* draw_list, ImVec2 min_pt, ImVec2 max_pt, ImU32 color) {
    constexpr float kLen = 22.0f;
    constexpr float kThick = 2.5f;

    // Top-left
    draw_list->AddLine(min_pt, ImVec2(min_pt.x + kLen, min_pt.y), color, kThick);
    draw_list->AddLine(min_pt, ImVec2(min_pt.x, min_pt.y + kLen), color, kThick);
    // Top-right
    draw_list->AddLine(ImVec2(max_pt.x - kLen, min_pt.y), max_pt, color, kThick);
    draw_list->AddLine(ImVec2(max_pt.x, min_pt.y), ImVec2(max_pt.x, min_pt.y + kLen), color, kThick);
    // Bottom-left
    draw_list->AddLine(ImVec2(min_pt.x, max_pt.y - kLen), ImVec2(min_pt.x, max_pt.y), color, kThick);
    draw_list->AddLine(ImVec2(min_pt.x, max_pt.y), ImVec2(min_pt.x + kLen, max_pt.y), color, kThick);
    // Bottom-right
    draw_list->AddLine(ImVec2(max_pt.x - kLen, max_pt.y), max_pt, color, kThick);
    draw_list->AddLine(ImVec2(max_pt.x, max_pt.y - kLen), max_pt, color, kThick);
}

void draw_status_pill(const char* label, const ImVec4& accent_color) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.18f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.18f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.18f));
    ImGui::PushStyleColor(ImGuiCol_Text, accent_color);
    ImGui::PushStyleColor(ImGuiCol_Border, accent_color);
    ImGui::SmallButton(label);
    ImGui::PopStyleColor(5);
}

} // namespace

ArenaUI::ArenaUI(core::AppConfig& config, vision::CameraCapture& camera)
    : config_(config)
    , camera_(camera) {
    refresh_devices();
}

void ArenaUI::refresh_devices() {
    devices_ = vision::CameraCapture::enumerate_devices(5);
    if (devices_.empty()) {
        devices_.push_back(vision::CameraDevice{
            .index = config_.camera_index,
            .label = std::format("Camera {} (Default)", config_.camera_index)
        });
    }
}

void ArenaUI::render() {
    if (camera_.try_get_latest_frame(latest_frame_, last_sequence_)) {
        if (camera_texture_.update(latest_frame_.bgr)) {
            last_sequence_ = latest_frame_.sequence;
        }
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    constexpr ImGuiWindowFlags kRootFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("RPSVisionRoot", nullptr, kRootFlags);

    render_top_bar();
    ImGui::Spacing();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float spacing_x = ImGui::GetStyle().ItemSpacing.x;
    const float left_width = std::max(320.0f, (avail.x - spacing_x) * 0.65f);
    const float right_width = std::max(260.0f, avail.x - left_width - spacing_x);

    render_webcam_arena(left_width, avail.y);
    ImGui::SameLine();
    render_sidebar(right_width, avail.y);

    ImGui::End();
}

void ArenaUI::render_top_bar() {
    const ThemePalette& p = palette();

    ImGui::BeginChild("TopBar", ImVec2(0.0f, 52.0f), ImGuiChildFlags_Borders);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(p.accent_cyan, "RPS VISION");
    ImGui::SameLine();
    ImGui::TextColored(p.text_muted, "// CYBER ARENA");

    ImGui::SameLine(0.0f, 24.0f);

    std::string active_label = std::format("Camera {}", config_.camera_index);
    for (const auto& dev : devices_) {
        if (dev.index == config_.camera_index) {
            active_label = dev.label;
            break;
        }
    }

    ImGui::SetNextItemWidth(210.0f);
    if (ImGui::BeginCombo("##CameraSelect", active_label.c_str())) {
        for (const auto& dev : devices_) {
            const bool selected = (dev.index == config_.camera_index);
            if (ImGui::Selectable(dev.label.c_str(), selected)) {
                config_.camera_index = dev.index;
                camera_.start(
                    config_.camera_index,
                    config_.camera_width,
                    config_.camera_height,
                    config_.camera_fps
                );
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        refresh_devices();
    }

    ImGui::SameLine();
    if (ImGui::Checkbox("Mirror", &config_.mirror_camera)) {
        camera_.set_mirror(config_.mirror_camera);
    }

    ImGui::SameLine();
    const bool cam_running = (camera_.status() == vision::CameraStatus::Running);
    if (ImGui::Button(cam_running ? "Stop Cam" : "Start Cam")) {
        if (cam_running) {
            camera_.stop();
        } else {
            camera_.start(
                config_.camera_index,
                config_.camera_width,
                config_.camera_height,
                config_.camera_fps
            );
        }
    }

    const float ui_fps = ImGui::GetIO().Framerate;
    const double cam_fps = camera_.current_fps();
    const std::string stats = std::format("UI {:.0f} FPS  |  CAM {:.1f} FPS", ui_fps, cam_fps);
    const float stats_w = ImGui::CalcTextSize(stats.c_str()).x + 12.0f;
    const float cursor_target = ImGui::GetWindowWidth() - stats_w - ImGui::GetStyle().WindowPadding.x;
    if (cursor_target > ImGui::GetCursorPosX() + 12.0f) {
        ImGui::SameLine(cursor_target);
    } else {
        ImGui::SameLine();
    }
    ImGui::TextColored(p.accent_cyan, "%s", stats.c_str());

    ImGui::EndChild();
}

void ArenaUI::render_webcam_arena(float width, float height) {
    const ThemePalette& p = palette();

    ImGui::BeginChild("WebcamArena", ImVec2(width, height), ImGuiChildFlags_Borders);

    const vision::CameraStatus status = camera_.status();
    if (status == vision::CameraStatus::Running) {
        const std::string live_label = camera_texture_.valid()
            ? std::format("LIVE  {}x{}", camera_texture_.width(), camera_texture_.height())
            : "LIVE";
        draw_status_pill(live_label.c_str(), p.status_green);
    } else if (status == vision::CameraStatus::Starting) {
        draw_status_pill("STARTING CAMERA...", p.status_amber);
    } else if (status == vision::CameraStatus::Error) {
        draw_status_pill("CAMERA ERROR", p.status_red);
    } else {
        draw_status_pill("CAMERA STOPPED", p.text_muted);
    }

    const char* cv_badge = "GESTURE: CV TRACKER PENDING";
    const float badge_w = ImGui::CalcTextSize(cv_badge).x + 24.0f;
    ImGui::SameLine(std::max(0.0f, ImGui::GetWindowContentRegionMax().x - badge_w));
    draw_status_pill(cv_badge, p.accent_purple);

    ImGui::Separator();

    const ImVec2 region = ImGui::GetContentRegionAvail();
    if (status == vision::CameraStatus::Running && camera_texture_.valid()) {
        const float tex_w = static_cast<float>(camera_texture_.width());
        const float tex_h = static_cast<float>(camera_texture_.height());
        const float scale = std::min(region.x / tex_w, region.y / tex_h);
        const ImVec2 draw_size(tex_w * scale, tex_h * scale);

        const ImVec2 cursor = ImGui::GetCursorPos();
        const ImVec2 offset(
            cursor.x + std::max(0.0f, (region.x - draw_size.x) * 0.5f),
            cursor.y + std::max(0.0f, (region.y - draw_size.y) * 0.5f)
        );
        ImGui::SetCursorPos(offset);

        const ImVec2 screen_min = ImGui::GetCursorScreenPos();
        ImGui::Image(static_cast<ImTextureID>(camera_texture_.imgui_texture_id()), draw_size);
        const ImVec2 screen_max(screen_min.x + draw_size.x, screen_min.y + draw_size.y);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRect(screen_min, screen_max, to_u32(p.border_subtle), 4.0f, 0, 1.0f);
        draw_corner_reticle(dl, screen_min, screen_max, to_u32(p.accent_cyan));
    } else {
        const std::string err = camera_.last_error();
        const char* heading = (status == vision::CameraStatus::Starting)
            ? "Initializing webcam stream..."
            : "Webcam feed unavailable";

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(20.0f, region.y * 0.35f));
        const float text_w = ImGui::CalcTextSize(heading).x;
        ImGui::SetCursorPosX(std::max(12.0f, (ImGui::GetWindowWidth() - text_w) * 0.5f));
        ImGui::TextColored(p.text_primary, "%s", heading);

        if (!err.empty()) {
            const float err_w = ImGui::CalcTextSize(err.c_str()).x;
            ImGui::SetCursorPosX(std::max(12.0f, (ImGui::GetWindowWidth() - err_w) * 0.5f));
            ImGui::TextColored(p.status_red, "%s", err.c_str());
        }

        ImGui::Spacing();
        constexpr float kBtnW = 150.0f;
        ImGui::SetCursorPosX(std::max(12.0f, (ImGui::GetWindowWidth() - kBtnW) * 0.5f));
        if (ImGui::Button("Retry Camera", ImVec2(kBtnW, 34.0f))) {
            camera_.start(
                config_.camera_index,
                config_.camera_width,
                config_.camera_height,
                config_.camera_fps
            );
        }
    }

    ImGui::EndChild();
}

void ArenaUI::render_sidebar(float width, float height) {
    ImGui::BeginChild("Sidebar", ImVec2(width, height), ImGuiChildFlags_None);

    const float spacing_y = ImGui::GetStyle().ItemSpacing.y;
    const float chat_h = std::max(160.0f, height * 0.32f);

    render_opponent_card();
    ImGui::Spacing();
    render_chat_card(chat_h - spacing_y);
    ImGui::Spacing();
    render_analytics_card();

    ImGui::EndChild();
}

void ArenaUI::render_opponent_card() {
    const ThemePalette& p = palette();
    ImGui::BeginChild("OpponentCard", ImVec2(0.0f, 175.0f), ImGuiChildFlags_Borders);
    ImGui::TextColored(p.accent_cyan, "AI OPPONENT & CONTROLS");
    ImGui::Separator();

    const char* difficulties[] = {"Casual (RNG)", "Tactician (Markov-2)", "Unbeatable (Cheater)"};
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##Difficulty", &preview_difficulty_idx_, difficulties, IM_ARRAYSIZE(difficulties));

    const char* commentators[] = {"Template Engine (Offline)", "Local LLM (GGUF)"};
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##Commentator", &preview_commentator_idx_, commentators, IM_ARRAYSIZE(commentators));

    ImGui::Checkbox("Voice TTS", &config_.tts_enabled);
    ImGui::SameLine();
    ImGui::TextColored(p.text_muted, "(Game loop in Milestone 4)");

    ImGui::BeginDisabled();
    ImGui::Button("PLAY ROUND (3-2-1-SHOOT)", ImVec2(-1.0f, 32.0f));
    ImGui::EndDisabled();

    ImGui::EndChild();
}

void ArenaUI::render_chat_card(float height) {
    const ThemePalette& p = palette();
    ImGui::BeginChild("CommentatorChatCard", ImVec2(0.0f, height), ImGuiChildFlags_Borders);
    ImGui::TextColored(p.accent_purple, "AI COMMENTATOR CHAT");
    ImGui::Separator();

    ImGui::BeginChild("ChatScroll", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    ImGui::TextColored(p.accent_cyan, "[SYSTEM]");
    ImGui::SameLine();
    ImGui::TextWrapped("Camera Arena online. Local AI commentary engine will activate in Milestone 5.");
    ImGui::Spacing();
    ImGui::TextColored(p.text_muted, "[STATUS]");
    ImGui::SameLine();
    ImGui::TextWrapped("Select your webcam above and verify smooth mirrored video capture.");
    ImGui::EndChild();

    ImGui::EndChild();
}

void ArenaUI::render_analytics_card() {
    const ThemePalette& p = palette();
    ImGui::BeginChild("AnalyticsCard", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    ImGui::TextColored(p.accent_cyan, "MATCH ANALYTICS & LOG");
    ImGui::Separator();

    ImGui::TextColored(p.status_green, "W: 0");
    ImGui::SameLine(0.0f, 18.0f);
    ImGui::TextColored(p.status_red, "L: 0");
    ImGui::SameLine(0.0f, 18.0f);
    ImGui::TextColored(p.status_amber, "D: 0");
    ImGui::SameLine(0.0f, 18.0f);
    ImGui::TextColored(p.text_muted, "Winrate: --%%");

    ImGui::Spacing();
    ImGui::TextColored(p.text_muted, "ImPlot progression chart and round history will populate once matches begin.");
    ImGui::EndChild();
}

} // namespace rps::ui
