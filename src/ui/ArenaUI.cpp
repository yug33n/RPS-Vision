#include "ui/ArenaUI.hpp"
#include "ui/Theme.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <format>
#include <imgui.h>
#include <implot.h>
#include <string>
#include <utility>

namespace rps::ui {

namespace {

constexpr std::array<std::pair<int, int>, 21> kHandBoneConnections = {{
    {0, 1}, {1, 2}, {2, 3}, {3, 4},       // Thumb
    {0, 5}, {5, 6}, {6, 7}, {7, 8},       // Index
    {0, 9}, {9, 10}, {10, 11}, {11, 12},  // Middle
    {0, 13}, {13, 14}, {14, 15}, {15, 16},// Ring
    {0, 17}, {17, 18}, {18, 19}, {19, 20},// Pinky
    {5, 9}                                // Palm bridge (index-middle)
}};

ImU32 to_u32(const ImVec4& c) noexcept {
    return ImGui::ColorConvertFloat4ToU32(c);
}

void draw_hand_skeleton(
    ImDrawList* draw_list,
    const ImVec2& rect_min,
    const ImVec2& rect_size,
    const vision::HandTrackingResult& tracking,
    const vision::GestureResult& gesture,
    const ThemePalette& p
) {
    if (!tracking.hand_detected) {
        return;
    }

    const ImVec4 bone_vec = (gesture.gesture != vision::HandGesture::Unknown)
        ? p.accent_cyan
        : p.status_amber;
    const ImU32 glow_col = to_u32(ImVec4(bone_vec.x, bone_vec.y, bone_vec.z, 0.30f));
    const ImU32 bone_col = to_u32(bone_vec);
    const ImU32 joint_col = to_u32(p.text_primary);
    const ImU32 joint_ring = to_u32(p.bg_canvas);

    auto to_screen = [&](const vision::LandmarkPoint& pt) -> ImVec2 {
        return ImVec2(
            rect_min.x + pt.x * rect_size.x,
            rect_min.y + pt.y * rect_size.y
        );
    };

    for (const auto& [a_idx, b_idx] : kHandBoneConnections) {
        const ImVec2 pa = to_screen(tracking.landmarks[static_cast<std::size_t>(a_idx)]);
        const ImVec2 pb = to_screen(tracking.landmarks[static_cast<std::size_t>(b_idx)]);
        draw_list->AddLine(pa, pb, glow_col, 6.0f);
        draw_list->AddLine(pa, pb, bone_col, 2.2f);
    }

    for (std::size_t i = 0; i < tracking.landmarks.size(); ++i) {
        const ImVec2 pt = to_screen(tracking.landmarks[i]);
        const bool is_tip = (i == 4 || i == 8 || i == 12 || i == 16 || i == 20);
        const float radius = is_tip ? 5.5f : 3.8f;
        draw_list->AddCircleFilled(pt, radius + 1.5f, joint_ring);
        draw_list->AddCircleFilled(pt, radius, is_tip ? bone_col : joint_col);
    }
}

void draw_countdown_overlay(
    ImDrawList* dl,
    const ImVec2& rect_min,
    const ImVec2& rect_size,
    const game::GameEngine& engine,
    const ThemePalette& p
) {
    const game::GameState st = engine.state();
    if (st != game::GameState::Countdown && st != game::GameState::Capture) {
        return;
    }

    const ImVec2 center(rect_min.x + rect_size.x * 0.5f, rect_min.y + rect_size.y * 0.5f);
    constexpr float kRadius = 64.0f;

    dl->AddCircleFilled(center, kRadius, IM_COL32(11, 15, 23, 195), 48);
    dl->AddCircle(center, kRadius, to_u32(p.border_subtle), 48, 3.0f);

    const float progress = (st == game::GameState::Capture) ? 1.0f : engine.countdown_progress();
    constexpr float kPi = 3.14159265f;
    const float start_angle = -kPi * 0.5f;
    const float end_angle = start_angle + progress * 2.0f * kPi;
    const ImU32 arc_col = (st == game::GameState::Capture) ? to_u32(p.accent_purple) : to_u32(p.accent_cyan);

    dl->PathClear();
    dl->PathArcTo(center, kRadius, start_angle, end_angle, 48);
    dl->PathStroke(arc_col, ImDrawFlags_None, 5.0f);

    const std::string label = engine.countdown_label();
    const float font_scale = (label == "SHOOT!") ? 1.8f : 2.8f;
    ImFont* font = ImGui::GetFont();
    const float font_size = ImGui::GetFontSize() * font_scale;
    const ImVec2 text_sz = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, label.c_str());
    dl->AddText(
        font,
        font_size,
        ImVec2(center.x - text_sz.x * 0.5f, center.y - text_sz.y * 0.5f),
        to_u32(p.text_primary),
        label.c_str()
    );
}

void draw_round_result_card(
    ImDrawList* dl,
    const ImVec2& rect_min,
    const ImVec2& rect_size,
    const game::RoundRecord& rec,
    const ThemePalette& p
) {
    ImVec4 accent = p.status_amber;
    if (rec.result == game::RoundResult::PlayerWin) accent = p.status_green;
    else if (rec.result == game::RoundResult::AiWin) accent = p.status_red;

    const float card_w = std::min(360.0f, rect_size.x - 32.0f);
    constexpr float card_h = 92.0f;
    const ImVec2 c_min(rect_min.x + (rect_size.x - card_w) * 0.5f, rect_min.y + 18.0f);
    const ImVec2 c_max(c_min.x + card_w, c_min.y + card_h);

    dl->AddRectFilled(c_min, c_max, IM_COL32(14, 19, 30, 225), 10.0f);
    dl->AddRect(c_min, c_max, to_u32(accent), 10.0f, 0, 2.0f);

    const std::string title = std::format("ROUND #{}  -  {}", rec.round_number, game::to_string(rec.result));
    const ImVec2 title_sz = ImGui::CalcTextSize(title.c_str());
    dl->AddText(ImVec2(c_min.x + (card_w - title_sz.x) * 0.5f, c_min.y + 14.0f), to_u32(accent), title.c_str());

    const std::string vs_line = std::format("YOU: {}   vs   AI: {}",
                                            game::to_string(rec.player_move),
                                            game::to_string(rec.ai_move));
    const ImVec2 vs_sz = ImGui::CalcTextSize(vs_line.c_str());
    dl->AddText(ImVec2(c_min.x + (card_w - vs_sz.x) * 0.5f, c_min.y + 48.0f), to_u32(p.text_primary), vs_line.c_str());
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

ArenaUI::ArenaUI(
    core::AppConfig& config,
    vision::CameraCapture& camera,
    vision::HandTracker& tracker,
    game::GameEngine& engine
)
    : config_(config)
    , camera_(camera)
    , tracker_(tracker)
    , engine_(engine)
    , preview_difficulty_idx_(static_cast<int>(engine.difficulty())) {
    refresh_devices();
    cv_worker_ = std::jthread([this](std::stop_token st) {
        cv_worker_loop(std::move(st));
    });
}

ArenaUI::~ArenaUI() {
    if (cv_worker_.joinable()) {
        cv_worker_.request_stop();
        cv_worker_.join();
    }
}

void ArenaUI::cv_worker_loop(std::stop_token stop_token) {
    vision::CapturedFrame frame;
    std::uint64_t processed_seq = 0;

    while (!stop_token.stop_requested()) {
        if (tracker_.status() != vision::TrackerStatus::Ready ||
            !camera_.try_get_latest_frame(frame, processed_seq)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
            continue;
        }

        processed_seq = frame.sequence;
        const vision::HandTrackingResult tracking = tracker_.process_frame(frame.bgr, frame.sequence);
        const vision::GestureResult gesture = classifier_.classify(tracking);

        std::scoped_lock lock(cv_mutex_);
        latest_tracking_ = tracking;
        latest_gesture_ = gesture;
    }
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

    vision::GestureResult current_gesture;
    {
        std::scoped_lock lock(cv_mutex_);
        current_gesture = latest_gesture_;
    }
    (void)engine_.update(current_gesture);

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

    vision::HandTrackingResult tracking;
    vision::GestureResult gesture;
    {
        std::scoped_lock lock(cv_mutex_);
        tracking = latest_tracking_;
        gesture = latest_gesture_;
    }

    const vision::TrackerStatus tracker_status = tracker_.status();
    std::string cv_badge;
    ImVec4 cv_badge_color = p.accent_purple;
    if (tracker_status == vision::TrackerStatus::ModelsMissing) {
        cv_badge = "CV: MODELS MISSING";
        cv_badge_color = p.status_amber;
    } else if (tracker_status == vision::TrackerStatus::Error) {
        cv_badge = "CV: OFFLINE";
        cv_badge_color = p.status_red;
    } else if (tracking.hand_detected) {
        const int pct = static_cast<int>(gesture.confidence * 100.0f);
        cv_badge = std::format("{} [{}%] ({:.1f}ms)",
                               vision::GestureClassifier::to_string(gesture.gesture),
                               pct,
                               tracking.inference_ms);
        cv_badge_color = (gesture.gesture != vision::HandGesture::Unknown)
            ? p.accent_cyan
            : p.status_amber;
    } else {
        cv_badge = "NO HAND DETECTED";
        cv_badge_color = p.text_muted;
    }

    const float badge_w = ImGui::CalcTextSize(cv_badge.c_str()).x + 24.0f;
    ImGui::SameLine(std::max(0.0f, ImGui::GetWindowContentRegionMax().x - badge_w));
    draw_status_pill(cv_badge.c_str(), cv_badge_color);

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
        draw_hand_skeleton(dl, screen_min, draw_size, tracking, gesture, p);
        draw_countdown_overlay(dl, screen_min, draw_size, engine_, p);
        if (engine_.state() == game::GameState::RoundEnd && engine_.last_round().has_value()) {
            draw_round_result_card(dl, screen_min, draw_size, *engine_.last_round(), p);
        }

        if (tracker_status == vision::TrackerStatus::ModelsMissing ||
            tracker_status == vision::TrackerStatus::Error) {
            const std::string banner = tracker_.status_message();
            const ImVec2 b_min(screen_min.x + 12.0f, screen_max.y - 44.0f);
            const ImVec2 b_max(screen_max.x - 12.0f, screen_max.y - 12.0f);
            dl->AddRectFilled(b_min, b_max, IM_COL32(25, 18, 12, 215), 6.0f);
            dl->AddRect(b_min, b_max, to_u32(p.status_amber), 6.0f, 0, 1.5f);
            dl->AddText(ImVec2(b_min.x + 10.0f, b_min.y + 8.0f), to_u32(p.status_amber), banner.c_str());
        }
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
    ImGui::BeginChild("OpponentCard", ImVec2(0.0f, 185.0f), ImGuiChildFlags_Borders);
    ImGui::TextColored(p.accent_cyan, "AI OPPONENT & CONTROLS");
    ImGui::Separator();

    const char* difficulties[] = {"Casual (RNG)", "Tactician (Markov-2)", "Unbeatable (Cheater)"};
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##Difficulty", &preview_difficulty_idx_, difficulties, IM_ARRAYSIZE(difficulties))) {
        const auto diff = static_cast<game::AiDifficulty>(preview_difficulty_idx_);
        engine_.set_difficulty(diff);
        if (diff == game::AiDifficulty::Casual) config_.ai_difficulty = "Casual";
        else if (diff == game::AiDifficulty::Cheater) config_.ai_difficulty = "Cheater";
        else config_.ai_difficulty = "Markov";
    }

    const char* commentators[] = {"Template Engine (Offline)", "Local LLM (GGUF)"};
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##Commentator", &preview_commentator_idx_, commentators, IM_ARRAYSIZE(commentators));

    bool auto_next = engine_.auto_next_round();
    if (ImGui::Checkbox("Auto-Next Round", &auto_next)) {
        engine_.set_auto_next_round(auto_next);
    }

    const bool active = (engine_.state() == game::GameState::Countdown ||
                         engine_.state() == game::GameState::Capture);
    if (active) {
        if (ImGui::Button("CANCEL ROUND", ImVec2(-1.0f, 34.0f))) {
            engine_.cancel_round();
        }
    } else {
        if (ImGui::Button("PLAY ROUND (3-2-1-SHOOT)", ImVec2(-1.0f, 34.0f))) {
            engine_.start_round();
        }
    }

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
    const auto& stats = engine_.stats();
    const auto& history = engine_.history();
    const auto& winrate_series = engine_.winrate_progression();

    ImGui::BeginChild("AnalyticsCard", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    ImGui::TextColored(p.accent_cyan, "MATCH ANALYTICS & LOG");
    ImGui::SameLine(std::max(0.0f, ImGui::GetWindowWidth() - 78.0f));
    if (ImGui::SmallButton("Reset")) {
        engine_.reset_stats();
    }
    ImGui::Separator();

    ImGui::TextColored(p.status_green, "W:%d", stats.player_wins);
    ImGui::SameLine(0.0f, 12.0f);
    ImGui::TextColored(p.status_red, "L:%d", stats.ai_wins);
    ImGui::SameLine(0.0f, 12.0f);
    ImGui::TextColored(p.status_amber, "D:%d", stats.draws);
    ImGui::SameLine(0.0f, 12.0f);
    ImGui::TextColored(p.accent_cyan, "WR: %.1f%%", stats.winrate_percent());

    if (!winrate_series.empty()) {
        if (ImPlot::BeginPlot("##WinratePlot", ImVec2(-1.0f, 125.0f),
                              ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoTitle)) {
            ImPlot::SetupAxes(nullptr, "Win %", ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoTickLabels,
                              ImPlotAxisFlags_None);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 100.0, ImGuiCond_Always);
            ImPlotSpec spec;
            spec.LineColor = p.accent_cyan;
            spec.LineWeight = 2.0f;
            ImPlot::PlotLine("Winrate", winrate_series.data(), static_cast<int>(winrate_series.size()), 1.0, 1.0, spec);
            ImPlot::EndPlot();
        }
    } else {
        ImGui::TextColored(p.text_muted, "Play rounds to populate win-rate chart.");
    }

    constexpr ImGuiTableFlags kTableFlags =
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH;
    if (ImGui::BeginTable("RoundHistoryTable", 4, kTableFlags, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 34.0f);
        ImGui::TableSetupColumn("You", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("AI", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (auto it = history.rbegin(); it != history.rend(); ++it) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", it->round_number);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", std::string(game::to_string(it->player_move)).c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", std::string(game::to_string(it->ai_move)).c_str());
            ImGui::TableSetColumnIndex(3);
            ImVec4 col = p.status_amber;
            if (it->result == game::RoundResult::PlayerWin) col = p.status_green;
            else if (it->result == game::RoundResult::AiWin) col = p.status_red;
            ImGui::TextColored(col, "%s", std::string(game::to_string(it->result)).c_str());
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

} // namespace rps::ui
