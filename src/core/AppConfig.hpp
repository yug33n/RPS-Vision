#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace rps::core {

/**
 * @brief Persistent application configuration loaded from and saved to JSON.
 */
struct AppConfig {
    int camera_index = 0;
    int camera_width = 1280;
    int camera_height = 720;
    int camera_fps = 30;
    bool mirror_camera = true;

    int window_width = 1440;
    int window_height = 900;

    float sfx_volume = 0.75f;
    bool tts_enabled = false;
    std::string ui_language = "en";
    std::string ai_difficulty = "Markov";
    std::string commentator_mode = "Template";

    std::string palm_model_path = "models/palm_detection.onnx";
    std::string landmark_model_path = "models/hand_landmark.onnx";
    std::string llm_model_path = "models/commentator.gguf";
    std::string font_path = "assets/fonts/JetBrainsMono-Regular.ttf";
};

/**
 * @brief Loads configuration from JSON or returns a descriptive error string.
 */
[[nodiscard]] std::expected<AppConfig, std::string> load_config(const std::filesystem::path& path);

/**
 * @brief Writes configuration to disk as formatted JSON.
 */
[[nodiscard]] std::expected<void, std::string> save_config(
    const AppConfig& config,
    const std::filesystem::path& path
);

/**
 * @brief Loads config if present, otherwise creates and persists default settings.
 */
[[nodiscard]] AppConfig load_or_create_config(const std::filesystem::path& path);

} // namespace rps::core
