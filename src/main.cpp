#include "ai/LlamaCommentator.hpp"
#include "ai/TemplateCommentator.hpp"
#include "audio/AudioEngine.hpp"
#include "core/AppConfig.hpp"
#include "core/Logger.hpp"
#include "game/GameEngine.hpp"
#include "ui/ArenaUI.hpp"
#include "ui/Theme.hpp"
#include "vision/CameraCapture.hpp"
#include "vision/HandTracker.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

#include <filesystem>

namespace {

void glfw_error_callback(int error_code, const char* description) {
    rps::core::Logger::error("GLFW error {}: {}", error_code, description ? description : "unknown");
}

} // namespace

int main() {
    const std::filesystem::path log_path = "data/rps_vision.log";
    const std::filesystem::path config_path = "data/settings.json";

    rps::core::Logger::init(log_path);
    rps::core::Logger::info("Starting RPS Vision...");

    rps::core::AppConfig config = rps::core::load_or_create_config(config_path);

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        rps::core::Logger::error("Failed to initialize GLFW.");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(
        config.window_width,
        config.window_height,
        "RPS Vision // Cyber Arena",
        nullptr,
        nullptr
    );
    if (window == nullptr) {
        rps::core::Logger::error("Failed to create GLFW window.");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;

    rps::ui::apply_esports_theme();
    rps::ui::load_fonts(config.font_path, 17.0f);

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        rps::core::Logger::error("Failed to initialize ImGui GLFW backend.");
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        rps::core::Logger::error("Failed to initialize ImGui OpenGL3 backend.");
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    rps::vision::HandTracker hand_tracker;
    (void)hand_tracker.init(config.palm_model_path, config.landmark_model_path);

    rps::game::GameEngine game_engine("data/stats.json");
    if (config.ai_difficulty == "Casual") {
        game_engine.set_difficulty(rps::game::AiDifficulty::Casual);
    } else if (config.ai_difficulty == "Cheater") {
        game_engine.set_difficulty(rps::game::AiDifficulty::Cheater);
    } else {
        game_engine.set_difficulty(rps::game::AiDifficulty::Markov);
    }

    rps::ai::TemplateCommentator template_commentator;
    rps::ai::LlamaCommentator llama_commentator;
    (void)llama_commentator.init(config.llm_model_path);

    rps::audio::AudioEngine audio_engine;
    audio_engine.set_volume(config.sfx_volume);
    (void)audio_engine.init();

    rps::vision::CameraCapture camera;
    camera.set_mirror(config.mirror_camera);

    rps::ui::ArenaUI arena_ui(
        config, camera, hand_tracker, game_engine,
        template_commentator, llama_commentator, audio_engine
    );
    camera.start(config.camera_index, config.camera_width, config.camera_height, config.camera_fps);

    const rps::ui::ThemePalette& pal = rps::ui::palette();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        if (display_w == 0 || display_h == 0) {
            glfwWaitEventsTimeout(0.05);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        arena_ui.render();

        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(pal.bg_canvas.x, pal.bg_canvas.y, pal.bg_canvas.z, pal.bg_canvas.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    camera.stop();

    int win_w = 0;
    int win_h = 0;
    glfwGetWindowSize(window, &win_w, &win_h);
    if (win_w >= 640 && win_h >= 480) {
        config.window_width = win_w;
        config.window_height = win_h;
    }
    if (auto saved = rps::core::save_config(config, config_path); !saved.has_value()) {
        rps::core::Logger::warn("Failed to save settings: {}", saved.error());
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    rps::core::Logger::info("RPS Vision shut down cleanly.");
    return 0;
}
