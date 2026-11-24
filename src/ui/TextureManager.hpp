#pragma once

#include <cstdint>
#include <opencv2/core/mat.hpp>

namespace rps::ui {

/**
 * @brief RAII manager for uploading OpenCV cv::Mat frames to an OpenGL 2D RGBA texture.
 */
class TextureManager {
public:
    TextureManager() = default;
    ~TextureManager();

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    TextureManager(TextureManager&& other) noexcept;
    TextureManager& operator=(TextureManager&& other) noexcept;

    /**
     * @brief Uploads a BGR/BGRA/RGB/Grayscale cv::Mat frame to the managed GL_RGBA texture.
     * @return true if the texture was updated successfully.
     */
    bool update(const cv::Mat& frame);

    /**
     * @brief Deletes the OpenGL texture resource if allocated.
     */
    void release() noexcept;

    [[nodiscard]] std::uint32_t texture_id() const noexcept { return texture_id_; }
    [[nodiscard]] std::uintptr_t imgui_texture_id() const noexcept {
        return static_cast<std::uintptr_t>(texture_id_);
    }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] bool valid() const noexcept { return texture_id_ != 0 && width_ > 0 && height_ > 0; }

private:
    std::uint32_t texture_id_ = 0;
    int width_ = 0;
    int height_ = 0;
    cv::Mat rgba_buffer_;
};

} // namespace rps::ui
