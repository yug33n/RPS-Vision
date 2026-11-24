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
        ? p.accent_primary
        : p.status_amber;
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
        draw_list->AddLine(pa, pb, bone_col, 2.0f);
    }

    for (std::size_t i = 0; i < tracking.landmarks.size(); ++i) {
        const ImVec2 pt = to_screen(tracking.landmarks[i]);
        const bool is_tip = (i == 4 || i == 8 || i == 12 || i == 16 || i == 20);
        const float radius = is_tip ? 5.0f : 3.5f;
        draw_list->AddCircleFilled(pt, radius + 1.5f, joint_ring);
        draw_list->AddCircleFilled(pt, radius, is_tip ? bone_col : joint_col);
    }
}

void draw_floating_pill(
    ImDrawList* dl,
    const ImVec2& pos,
    const std::string& text,
    const ImVec4& accent,
    ImFont* font = nullptr
) {
    const float s = ui_scale();
    ImFont* use_font = font ? font : ImGui::GetFont();
    const float font_size = ImGui::GetFontSize();
    const ImVec2 txt_sz = use_font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text.c_str());
    const float pad_x = 12.0f * s;
    const float pad_y = 6.0f * s;
    const ImVec2 max_pt(pos.x + txt_sz.x + pad_x * 2.0f, pos.y + txt_sz.y + pad_y * 2.0f);

    dl->AddRectFilled(pos, max_pt, IM_COL32(17, 19, 24, 215), 6.0f * s);
    dl->AddRect(pos, max_pt, to_u32(ImVec4(accent.x, accent.y, accent.z, 0.60f)), 6.0f * s, 0, 1.0f);
    dl->AddText(use_font, font_size, ImVec2(pos.x + pad_x, pos.y + pad_y), to_u32(accent), text.c_str());
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

    const float s = ui_scale();
    const ImVec2 center(rect_min.x + rect_size.x * 0.5f, rect_min.y + rect_size.y * 0.5f);
    const float radius = 68.0f * s;

    dl->AddCircleFilled(center, radius + 6.0f * s, IM_COL32(17, 19, 24, 210), 64);
    dl->AddCircle(center, radius, to_u32(p.border_subtle), 64, 3.0f * s);

    const float progress = (st == game::GameState::Capture) ? 1.0f : engine.countdown_progress();
    constexpr float kPi = 3.14159265f;
    const float start_angle = -kPi * 0.5f;
    const float end_angle = start_angle + progress * 2.0f * kPi;
    const ImVec4 arc_vec = (st == game::GameState::Capture) ? p.status_amber : p.accent_primary;

    dl->PathClear();
    dl->PathArcTo(center, radius, start_angle, end_angle, 64);
    dl->PathStroke(to_u32(arc_vec), ImDrawFlags_None, 4.5f * s);

    const std::string label = engine.countdown_label();
    ImFont* font = fonts().display ? fonts().display : ImGui::GetFont();
    const float font_size = (label == "SHOOT!" ? 32.0f : 48.0f) * s;
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
    const float s = ui_scale();
    ImVec4 accent = p.status_amber;
    if (rec.result == game::RoundResult::PlayerWin) accent = p.status_green;
    else if (rec.result == game::RoundResult::AiWin) accent = p.status_red;

    const float card_w = std::min(400.0f * s, rect_size.x - 32.0f * s);
    const float card_h = 84.0f * s;
    const ImVec2 c_min(rect_min.x + (rect_size.x - card_w) * 0.5f, rect_min.y + 52.0f * s);
    const ImVec2 c_max(c_min.x + card_w, c_min.y + card_h);

    dl->AddRectFilled(c_min, c_max, IM_COL32(24, 27, 34, 235), 8.0f * s);
    dl->AddRect(c_min, c_max, to_u32(accent), 8.0f * s, 0, 1.5f);

    ImFont* bold = fonts().bold ? fonts().bold : ImGui::GetFont();
    const float bold_sz = bold->LegacySize > 0.0f ? bold->LegacySize : ImGui::GetFontSize();

    const std::string title = std::format("Round #{}  -  {}", rec.round_number, game::to_string(rec.result));
    const ImVec2 title_sz = bold->CalcTextSizeA(bold_sz, FLT_MAX, 0.0f, title.c_str());
    dl->AddText(bold, bold_sz, ImVec2(c_min.x + (card_w - title_sz.x) * 0.5f, c_min.y + 12.0f * s),
                to_u32(accent), title.c_str());

    const std::string p_str = std::format("You: {}", game::to_string(rec.player_move));
    const std::string a_str = std::format("AI: {}", game::to_string(rec.ai_move));
    const char* vs_str = "vs";

    const ImVec2 a_sz = bold->CalcTextSizeA(bold_sz, FLT_MAX, 0.0f, a_str.c_str());
    const ImVec2 v_sz = bold->CalcTextSizeA(bold_sz, FLT_MAX, 0.0f, vs_str);

    const float row_y = c_min.y + 44.0f * s;
    dl->AddText(bold, bold_sz, ImVec2(c_min.x + 28.0f * s, row_y), to_u32(p.text_primary), p_str.c_str());
    dl->AddText(bold, bold_sz, ImVec2(c_min.x + (card_w - v_sz.x) * 0.5f, row_y), to_u32(p.text_muted), vs_str);
    dl->AddText(bold, bold_sz, ImVec2(c_max.x - 28.0f * s - a_sz.x, row_y), to_u32(p.text_primary), a_str.c_str());
}

void draw_corner_reticle(ImDrawList*, ImVec2, ImVec2, ImU32) {}

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
    game::GameEngine& engine,
    ai::TemplateCommentator& template_commentator,
    ai::LlamaCommentator& llama_commentator,
    audio::AudioEngine& audio_engine
)
    : config_(config)
    , camera_(camera)
    , tracker_(tracker)
    , engine_(engine)
    , template_commentator_(template_commentator)
    , llama_commentator_(llama_commentator)
    , audio_engine_(audio_engine)
    , preview_difficulty_idx_(static_cast<int>(engine.difficulty()))
    , preview_commentator_idx_(
          (config.commentator_mode == "LLM" && llama_commentator.is_available()) ? 1 : 0)
    , language_idx_(config.ui_language == "ru" ? 1 : 0) {
    chat_log_.push_back(ai::CommentatorMessage{
        .author = "SYSTEM",
        .text = llama_commentator_.is_available()
            ? "Arena ready. Local GGUF LLM and Template Engine are online."
            : "Arena ready. Template Engine active (place models/commentator.gguf for Local LLM).",
        .timestamp = "INIT",
        .is_streaming = false
    });
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

    const auto completed = engine_.update(current_gesture);

    if (engine_.state() == game::GameState::Countdown || engine_.state() == game::GameState::Capture) {
        const std::string lbl = engine_.countdown_label();
        if (lbl != last_countdown_label_) {
            last_countdown_label_ = lbl;
            audio_engine_.play(lbl == "SHOOT!" ? audio::SoundEffect::Shoot : audio::SoundEffect::Tick);
        }
    } else {
        last_countdown_label_.clear();
    }

    ai::ICommentator& active_commentator = (preview_commentator_idx_ == 1 && llama_commentator_.is_available())
        ? static_cast<ai::ICommentator&>(llama_commentator_)
        : static_cast<ai::ICommentator&>(template_commentator_);

    if (completed.has_value()) {
        const auto& rec = *completed;
        switch (rec.result) {
            case game::RoundResult::PlayerWin:   audio_engine_.play(audio::SoundEffect::Win); break;
            case game::RoundResult::AiWin:       audio_engine_.play(audio::SoundEffect::Lose); break;
            case game::RoundResult::Draw:        audio_engine_.play(audio::SoundEffect::Draw); break;
            case game::RoundResult::InvalidMove: audio_engine_.play(audio::SoundEffect::Invalid); break;
        }

        if (rec.player_move != game::Move::None && rec.player_move == last_player_move_) {
            ++consecutive_same_move_;
        } else {
            last_player_move_ = rec.player_move;
            consecutive_same_move_ = (rec.player_move != game::Move::None) ? 1 : 0;
        }
        consecutive_draws_ = (rec.result == game::RoundResult::Draw) ? (consecutive_draws_ + 1) : 0;

        const ai::CommentaryContext ctx{
            .round = rec,
            .stats = engine_.stats(),
            .consecutive_same_player_move = consecutive_same_move_,
            .consecutive_draws = consecutive_draws_,
            .language = (language_idx_ == 1) ? ai::Language::Ru : ai::Language::En
        };
        active_commentator.request_commentary(ctx);
    }

    if (auto msg = active_commentator.poll_update()) {
        if (!chat_log_.empty() && chat_log_.back().is_streaming && msg->author == chat_log_.back().author) {
            chat_log_.back() = std::move(*msg);
        } else {
            chat_log_.push_back(std::move(*msg));
            if (chat_log_.size() > 60) {
                chat_log_.erase(chat_log_.begin());
            }
        }
        chat_scroll_to_bottom_ = true;
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
    const float left_width = std::max(400.0f, (avail.x - spacing_x) * 0.64f);
    const float right_width = std::max(300.0f, avail.x - left_width - spacing_x);

    render_left_column(left_width, avail.y);
    ImGui::SameLine();
    render_sidebar(right_width, avail.y);

    ImGui::End();
}

void ArenaUI::render_left_column(float width, float height) {
    ImGui::BeginChild("LeftColumn", ImVec2(width, height), ImGuiChildFlags_None);
    render_opponent_card();
    ImGui::Spacing();
    const float remain_h = std::max(240.0f, ImGui::GetContentRegionAvail().y);
    render_webcam_arena(width, remain_h);
    ImGui::EndChild();
}

void ArenaUI::render_top_bar() {
    const ThemePalette& p = palette();
    const float s = ui_scale();

    ImGui::BeginChild("TopBar", ImVec2(0.0f, 54.0f * s), ImGuiChildFlags_Borders);
    ImGui::AlignTextToFramePadding();
    if (fonts().bold) ImGui::PushFont(fonts().bold);
    ImGui::TextColored(p.text_primary, "RPS Vision");
    if (fonts().bold) ImGui::PopFont();

    ImGui::SameLine(0.0f, 22.0f * s);

    std::string active_label = std::format("Camera {}", config_.camera_index);
    for (const auto& dev : devices_) {
        if (dev.index == config_.camera_index) {
            active_label = dev.label;
            break;
        }
    }

    ImGui::SetNextItemWidth(230.0f * s);
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
    if (ImGui::Button(cam_running ? "Stop Camera" : "Start Camera")) {
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
    const std::string stats = std::format("UI {:.0f} fps  |  Cam {:.1f} fps  |  F11 Fullscreen", ui_fps, cam_fps);
    const float exit_w = 74.0f * s;
    const float stats_w = ImGui::CalcTextSize(stats.c_str()).x + exit_w + 32.0f * s;
    const float cursor_target = ImGui::GetWindowWidth() - stats_w - ImGui::GetStyle().WindowPadding.x;
    if (cursor_target > ImGui::GetCursorPosX() + 12.0f) {
        ImGui::SameLine(cursor_target);
    } else {
        ImGui::SameLine();
    }
    ImGui::TextColored(p.text_muted, "%s", stats.c_str());

    ImGui::SameLine(0.0f, 14.0f * s);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.973f, 0.443f, 0.443f, 0.14f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, p.status_red);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, p.status_red);
    ImGui::PushStyleColor(ImGuiCol_Border, p.status_red);
    if (ImGui::Button("Exit", ImVec2(exit_w, 0.0f))) {
        exit_requested_ = true;
    }
    ImGui::PopStyleColor(4);

    ImGui::EndChild();
}

void ArenaUI::render_webcam_arena(float width, float height) {
    const ThemePalette& p = palette();

    ImGui::BeginChild("WebcamArena", ImVec2(width, height), ImGuiChildFlags_Borders);

    const vision::CameraStatus status = camera_.status();
    std::string live_label = "CAMERA STOPPED";
    ImVec4 live_col = p.text_muted;
    if (status == vision::CameraStatus::Running) {
        live_label = camera_texture_.valid()
            ? std::format("LIVE  {}x{}", camera_texture_.width(), camera_texture_.height())
            : "LIVE";
        live_col = p.status_green;
    } else if (status == vision::CameraStatus::Starting) {
        live_label = "STARTING CAMERA...";
        live_col = p.status_amber;
    } else if (status == vision::CameraStatus::Error) {
        live_label = "CAMERA ERROR";
        live_col = p.status_red;
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
    ImVec4 cv_badge_color = p.text_muted;
    if (tracker_status == vision::TrackerStatus::ModelsMissing) {
        cv_badge = "CV: Models missing";
        cv_badge_color = p.status_amber;
    } else if (tracker_status == vision::TrackerStatus::Error) {
        cv_badge = "CV: Offline";
        cv_badge_color = p.status_red;
    } else if (tracking.hand_detected) {
        const int pct = static_cast<int>(gesture.confidence * 100.0f);
        cv_badge = std::format("{}  {}%  ({:.1f} ms)",
                               vision::GestureClassifier::to_string(gesture.gesture),
                               pct,
                               tracking.inference_ms);
        cv_badge_color = (gesture.gesture != vision::HandGesture::Unknown)
            ? p.accent_hover
            : p.status_amber;
    } else {
        cv_badge = "No hand detected";
        cv_badge_color = p.text_muted;
    }

    const float s = ui_scale();
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
        draw_hand_skeleton(dl, screen_min, draw_size, tracking, gesture, p);
        draw_countdown_overlay(dl, screen_min, draw_size, engine_, p);
        if (engine_.state() == game::GameState::RoundEnd && engine_.last_round().has_value()) {
            draw_round_result_card(dl, screen_min, draw_size, *engine_.last_round(), p);
        }

        draw_floating_pill(dl, ImVec2(screen_min.x + 12.0f * s, screen_min.y + 12.0f * s), live_label, live_col, fonts().bold);
        const float cv_w = ImGui::CalcTextSize(cv_badge.c_str()).x + 24.0f * s;
        draw_floating_pill(dl, ImVec2(screen_max.x - cv_w - 12.0f * s, screen_min.y + 12.0f * s), cv_badge, cv_badge_color, fonts().bold);

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
    const float chat_h = std::max(180.0f * ui_scale(), height * 0.36f);

    render_chat_card(chat_h - spacing_y);
    ImGui::Spacing();
    render_analytics_card();

    ImGui::EndChild();
}

void ArenaUI::render_opponent_card() {
    const ThemePalette& p = palette();
    const float s = ui_scale();
    ImGui::BeginChild("OpponentCard", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

    const bool active = (engine_.state() == game::GameState::Countdown ||
                         engine_.state() == game::GameState::Capture);
    const float btn_w = 220.0f * s;
    const float btn_h = 38.0f * s;

    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, p.status_red);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.98f, 0.50f, 0.50f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, p.status_red);
        ImGui::PushStyleColor(ImGuiCol_Text, p.text_primary);
        if (fonts().bold) ImGui::PushFont(fonts().bold);
        if (ImGui::Button("Cancel Round", ImVec2(btn_w, btn_h))) {
            engine_.cancel_round();
        }
        if (fonts().bold) ImGui::PopFont();
        ImGui::PopStyleColor(4);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, p.accent_primary);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, p.accent_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, p.accent_primary);
        ImGui::PushStyleColor(ImGuiCol_Text, p.text_primary);
        if (fonts().bold) ImGui::PushFont(fonts().bold);
        if (ImGui::Button("Play Round (3-2-1)", ImVec2(btn_w, btn_h))) {
            engine_.start_round();
        }
        if (fonts().bold) ImGui::PopFont();
        ImGui::PopStyleColor(4);
    }

    ImGui::SameLine();
    const char* difficulties[] = {"Casual (RNG)", "Tactician (Markov-2)", "Unbeatable (Cheater)"};
    ImGui::SetNextItemWidth(205.0f * s);
    if (ImGui::Combo("##Difficulty", &preview_difficulty_idx_, difficulties, IM_ARRAYSIZE(difficulties))) {
        const auto diff = static_cast<game::AiDifficulty>(preview_difficulty_idx_);
        engine_.set_difficulty(diff);
        if (diff == game::AiDifficulty::Casual) config_.ai_difficulty = "Casual";
        else if (diff == game::AiDifficulty::Cheater) config_.ai_difficulty = "Cheater";
        else config_.ai_difficulty = "Markov";
    }

    ImGui::SameLine();
    const char* commentators[] = {
        "Template Commentator",
        llama_commentator_.is_available() ? "Local LLM (GGUF)" : "Local LLM (Fallback)"
    };
    ImGui::SetNextItemWidth(220.0f * s);
    if (ImGui::Combo("##Commentator", &preview_commentator_idx_, commentators, IM_ARRAYSIZE(commentators))) {
        config_.commentator_mode = (preview_commentator_idx_ == 1) ? "LLM" : "Template";
    }

    ImGui::SameLine();
    const char* langs[] = {"EN", "RU"};
    ImGui::SetNextItemWidth(68.0f * s);
    if (ImGui::Combo("##Lang", &language_idx_, langs, IM_ARRAYSIZE(langs))) {
        config_.ui_language = (language_idx_ == 1) ? "ru" : "en";
    }

    bool auto_next = engine_.auto_next_round();
    if (ImGui::Checkbox("Auto-Next", &auto_next)) {
        engine_.set_auto_next_round(auto_next);
    }
    ImGui::SameLine();
    bool muted = audio_engine_.is_muted();
    if (ImGui::Checkbox("Mute SFX", &muted)) {
        audio_engine_.set_muted(muted);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f * s);
    if (ImGui::SliderFloat("##SfxVol", &config_.sfx_volume, 0.0f, 1.0f, "Volume %.0f%%")) {
        audio_engine_.set_volume(config_.sfx_volume);
    }

    ImGui::EndChild();
}

void ArenaUI::render_chat_card(float height) {
    const ThemePalette& p = palette();
    ImGui::BeginChild("CommentatorChatCard", ImVec2(0.0f, height), ImGuiChildFlags_Borders);
    if (fonts().bold) ImGui::PushFont(fonts().bold);
    ImGui::TextColored(p.text_primary, "Commentator");
    if (fonts().bold) ImGui::PopFont();
    if (llama_commentator_.is_busy()) {
        ImGui::SameLine();
        ImGui::TextColored(p.text_muted, "(typing...)");
    }
    ImGui::Separator();

    ImGui::BeginChild("ChatScroll", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (const auto& msg : chat_log_) {
        const ImVec4 tag_col = (msg.author == "LOCAL-LLM") ? p.accent_hover : p.accent_primary;
        const ImVec2 row_start = ImGui::GetCursorScreenPos();
        ImGui::Indent(10.0f * ui_scale());
        if (fonts().bold) ImGui::PushFont(fonts().bold);
        ImGui::TextColored(tag_col, "%s", msg.author.c_str());
        if (fonts().bold) ImGui::PopFont();
        ImGui::SameLine();
        ImGui::TextColored(p.text_muted, "%s", msg.timestamp.c_str());
        ImGui::TextWrapped("%s", msg.text.c_str());
        ImGui::Unindent(10.0f * ui_scale());
        const ImVec2 row_end = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(
            ImVec2(row_start.x, row_start.y + 2.0f),
            ImVec2(row_start.x + 2.5f * ui_scale(), std::max(row_start.y + 20.0f, row_end.y - 2.0f)),
            to_u32(tag_col),
            1.5f
        );
        ImGui::Spacing();
    }
    if (chat_scroll_to_bottom_) {
        ImGui::SetScrollHereY(1.0f);
        chat_scroll_to_bottom_ = false;
    }
    ImGui::EndChild();

    ImGui::EndChild();
}

void ArenaUI::render_analytics_card() {
    const ThemePalette& p = palette();
    const float s = ui_scale();
    const auto& stats = engine_.stats();
    const auto& history = engine_.history();
    const auto& winrate_series = engine_.winrate_progression();

    ImGui::BeginChild("AnalyticsCard", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    if (fonts().bold) ImGui::PushFont(fonts().bold);
    ImGui::TextColored(p.text_primary, "Match Analytics");
    if (fonts().bold) ImGui::PopFont();
    ImGui::SameLine(std::max(0.0f, ImGui::GetWindowWidth() - 80.0f * s));
    if (ImGui::SmallButton("Reset")) {
        engine_.reset_stats();
    }
    ImGui::Separator();

    auto draw_kpi_tile = [&](const char* id, const char* label, const std::string& val, const ImVec4& col) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, p.bg_elevated);
        ImGui::BeginChild(id, ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        ImGui::TextColored(p.text_muted, "%s", label);
        if (fonts().bold) ImGui::PushFont(fonts().bold);
        ImGui::TextColored(col, "%s", val.c_str());
        if (fonts().bold) ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    };

    if (ImGui::BeginTable("KpiTiles", 4, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        draw_kpi_tile("KpiW", "Wins", std::to_string(stats.player_wins), p.status_green);
        ImGui::TableSetColumnIndex(1);
        draw_kpi_tile("KpiL", "Losses", std::to_string(stats.ai_wins), p.status_red);
        ImGui::TableSetColumnIndex(2);
        draw_kpi_tile("KpiD", "Draws", std::to_string(stats.draws), p.status_amber);
        ImGui::TableSetColumnIndex(3);
        draw_kpi_tile("KpiWR", "Win Rate", std::format("{:.0f}%", stats.winrate_percent()), p.accent_hover);
        ImGui::EndTable();
    }

    ImGui::Spacing();

    if (!winrate_series.empty()) {
        if (ImPlot::BeginPlot("##WinratePlot", ImVec2(-1.0f, 130.0f * s),
                              ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoTitle)) {
            ImPlot::SetupAxes(nullptr, "Win %", ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoTickLabels,
                              ImPlotAxisFlags_None);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 100.0, ImGuiCond_Always);
            ImPlotSpec spec;
            spec.LineColor = p.accent_primary;
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
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 38.0f * s);
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
