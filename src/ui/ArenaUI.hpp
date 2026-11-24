#pragma once

#include "core/AppConfig.hpp"
#include "ui/TextureManager.hpp"
#include "vision/CameraCapture.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace rps::ui {

/**
 * @brief Main three-pane Dear ImGui esports interface hosting the webcam arena and control panels.
 */
class ArenaUI {
public:
    ArenaUI(core::AppConfig& config, vision::CameraCapture& camera);

    /**
     * @brief Refreshes the list of available webcam devices.
     */
    void refresh_devices();

    /**
     * @brief Pulls latest camera frame if available and renders the full viewport UI.
     */
    void render();

private:
    void render_top_bar();
    void render_webcam_arena(float width, float height);
    void render_sidebar(float width, float height);
    void render_opponent_card();
    void render_chat_card(float height);
    void render_analytics_card();

    core::AppConfig& config_;
    vision::CameraCapture& camera_;
    TextureManager camera_texture_;

    vision::CapturedFrame latest_frame_;
    std::uint64_t last_sequence_ = 0;

    std::vector<vision::CameraDevice> devices_;
    int preview_difficulty_idx_ = 1;
    int preview_commentator_idx_ = 0;
};

} // namespace rps::ui
