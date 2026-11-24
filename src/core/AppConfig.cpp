#include "core/AppConfig.hpp"
#include "core/Logger.hpp"

#include <format>
#include <fstream>
#include <nlohmann/json.hpp>

namespace rps::core {

namespace {

template <typename T>
void read_field(const nlohmann::json& j, const char* key, T& target) {
    if (j.contains(key) && !j.at(key).is_null()) {
        try {
            target = j.at(key).get<T>();
        } catch (const nlohmann::json::exception&) {
            // Keep default value if type mismatches
        }
    }
}

nlohmann::json to_json(const AppConfig& cfg) {
    return nlohmann::json{
        {"camera_index", cfg.camera_index},
        {"camera_width", cfg.camera_width},
        {"camera_height", cfg.camera_height},
        {"camera_fps", cfg.camera_fps},
        {"mirror_camera", cfg.mirror_camera},
        {"window_width", cfg.window_width},
        {"window_height", cfg.window_height},
        {"sfx_volume", cfg.sfx_volume},
        {"tts_enabled", cfg.tts_enabled},
        {"ui_language", cfg.ui_language},
        {"ai_difficulty", cfg.ai_difficulty},
        {"commentator_mode", cfg.commentator_mode},
        {"palm_model_path", cfg.palm_model_path},
        {"landmark_model_path", cfg.landmark_model_path},
        {"llm_model_path", cfg.llm_model_path},
        {"font_path", cfg.font_path}
    };
}

} // namespace

std::expected<AppConfig, std::string> load_config(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::unexpected(std::format("Unable to open config file: {}", path.string()));
    }

    nlohmann::json j;
    try {
        file >> j;
    } catch (const nlohmann::json::parse_error& err) {
        return std::unexpected(std::format("JSON parse error in {}: {}", path.string(), err.what()));
    }

    if (!j.is_object()) {
        return std::unexpected(std::format("Config root must be a JSON object: {}", path.string()));
    }

    AppConfig cfg;
    read_field(j, "camera_index", cfg.camera_index);
    read_field(j, "camera_width", cfg.camera_width);
    read_field(j, "camera_height", cfg.camera_height);
    read_field(j, "camera_fps", cfg.camera_fps);
    read_field(j, "mirror_camera", cfg.mirror_camera);
    read_field(j, "window_width", cfg.window_width);
    read_field(j, "window_height", cfg.window_height);
    read_field(j, "sfx_volume", cfg.sfx_volume);
    read_field(j, "tts_enabled", cfg.tts_enabled);
    read_field(j, "ui_language", cfg.ui_language);
    read_field(j, "ai_difficulty", cfg.ai_difficulty);
    read_field(j, "commentator_mode", cfg.commentator_mode);
    read_field(j, "palm_model_path", cfg.palm_model_path);
    read_field(j, "landmark_model_path", cfg.landmark_model_path);
    read_field(j, "llm_model_path", cfg.llm_model_path);
    read_field(j, "font_path", cfg.font_path);
    return cfg;
}

std::expected<void, std::string> save_config(
    const AppConfig& config,
    const std::filesystem::path& path
) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return std::unexpected(
                std::format("Failed to create directory {}: {}", path.parent_path().string(), ec.message())
            );
        }
    }

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return std::unexpected(std::format("Unable to write config file: {}", path.string()));
    }

    file << to_json(config).dump(2) << '\n';
    if (!file.good()) {
        return std::unexpected(std::format("I/O failure writing config file: {}", path.string()));
    }
    return {};
}

AppConfig load_or_create_config(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        auto result = load_config(path);
        if (result.has_value()) {
            return *result;
        }
        Logger::warn("Config load failed ({}); falling back to defaults.", result.error());
        return AppConfig{};
    }

    AppConfig defaults;
    if (auto saved = save_config(defaults, path); !saved.has_value()) {
        Logger::warn("Could not persist default config: {}", saved.error());
    } else {
        Logger::info("Created default configuration at {}", path.string());
    }
    return defaults;
}

} // namespace rps::core
