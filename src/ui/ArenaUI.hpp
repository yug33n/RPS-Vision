#pragma once

#include "ai/ICommentator.hpp"
#include "ai/LlamaCommentator.hpp"
#include "ai/TemplateCommentator.hpp"
#include "audio/AudioEngine.hpp"
#include "core/AppConfig.hpp"
#include "game/GameEngine.hpp"
#include "ui/TextureManager.hpp"
#include "vision/CameraCapture.hpp"
#include "vision/GestureClassifier.hpp"
#include "vision/HandTracker.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace rps::ui {

/**
 * @brief Main three-pane Dear ImGui esports interface hosting the webcam arena and control panels.
 */
class ArenaUI {
public:
    ArenaUI(
        core::AppConfig& config,
        vision::CameraCapture& camera,
        vision::HandTracker& tracker,
        game::GameEngine& engine,
        ai::TemplateCommentator& template_commentator,
        ai::LlamaCommentator& llama_commentator,
        audio::AudioEngine& audio_engine
    );
    ~ArenaUI();

    ArenaUI(const ArenaUI&) = delete;
    ArenaUI& operator=(const ArenaUI&) = delete;

    /**
     * @brief Refreshes the list of available webcam devices.
     */
    void refresh_devices();

    /**
     * @brief Pulls latest camera frame if available and renders the full viewport UI.
     */
    void render();

    [[nodiscard]] bool exit_requested() const noexcept { return exit_requested_; }

private:
    void cv_worker_loop(std::stop_token stop_token);
    void render_top_bar();
    void render_webcam_arena(float width, float height);
    void render_sidebar(float width, float height);
    void render_opponent_card();
    void render_chat_card(float height);
    void render_analytics_card();

    core::AppConfig& config_;
    vision::CameraCapture& camera_;
    vision::HandTracker& tracker_;
    game::GameEngine& engine_;
    ai::TemplateCommentator& template_commentator_;
    ai::LlamaCommentator& llama_commentator_;
    audio::AudioEngine& audio_engine_;
    vision::GestureClassifier classifier_;
    TextureManager camera_texture_;

    vision::CapturedFrame latest_frame_;
    std::uint64_t last_sequence_ = 0;

    std::jthread cv_worker_;
    mutable std::mutex cv_mutex_;
    vision::HandTrackingResult latest_tracking_;
    vision::GestureResult latest_gesture_;

    std::vector<ai::CommentatorMessage> chat_log_;
    bool chat_scroll_to_bottom_ = false;
    std::string last_countdown_label_;
    game::Move last_player_move_ = game::Move::None;
    int consecutive_same_move_ = 0;
    int consecutive_draws_ = 0;

    std::vector<vision::CameraDevice> devices_;
    int preview_difficulty_idx_ = 1;
    int preview_commentator_idx_ = 0;
    int language_idx_ = 0;
    bool exit_requested_ = false;
};

} // namespace rps::ui
