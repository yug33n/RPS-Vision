#include "ui/TextureManager.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <GL/gl.h>

#include <opencv2/imgproc.hpp>
#include <utility>

namespace rps::ui {

namespace {
constexpr GLint kGlClampToEdge = 0x812F;
} // namespace

TextureManager::~TextureManager() {
    release();
}

TextureManager::TextureManager(TextureManager&& other) noexcept
    : texture_id_(std::exchange(other.texture_id_, 0))
    , width_(std::exchange(other.width_, 0))
    , height_(std::exchange(other.height_, 0))
    , rgba_buffer_(std::move(other.rgba_buffer_)) {}

TextureManager& TextureManager::operator=(TextureManager&& other) noexcept {
    if (this != &other) {
        release();
        texture_id_ = std::exchange(other.texture_id_, 0);
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
        rgba_buffer_ = std::move(other.rgba_buffer_);
    }
    return *this;
}

bool TextureManager::update(const cv::Mat& frame) {
    if (frame.empty()) {
        return false;
    }

    switch (frame.channels()) {
        case 3:
            cv::cvtColor(frame, rgba_buffer_, cv::COLOR_BGR2RGBA);
            break;
        case 4:
            cv::cvtColor(frame, rgba_buffer_, cv::COLOR_BGRA2RGBA);
            break;
        case 1:
            cv::cvtColor(frame, rgba_buffer_, cv::COLOR_GRAY2RGBA);
            break;
        default:
            return false;
    }

    if (!rgba_buffer_.isContinuous()) {
        rgba_buffer_ = rgba_buffer_.clone();
    }

    if (texture_id_ == 0) {
        GLuint id = 0;
        glGenTextures(1, &id);
        texture_id_ = static_cast<std::uint32_t>(id);
        if (texture_id_ == 0) {
            return false;
        }

        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kGlClampToEdge);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kGlClampToEdge);
    } else {
        glBindTexture(GL_TEXTURE_2D, texture_id_);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    const int new_w = rgba_buffer_.cols;
    const int new_h = rgba_buffer_.rows;
    if (new_w != width_ || new_h != height_) {
        width_ = new_w;
        height_ = new_h;
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            width_,
            height_,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            rgba_buffer_.data
        );
    } else {
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            width_,
            height_,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            rgba_buffer_.data
        );
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void TextureManager::release() noexcept {
    if (texture_id_ != 0) {
        const GLuint id = static_cast<GLuint>(texture_id_);
        glDeleteTextures(1, &id);
        texture_id_ = 0;
    }
    width_ = 0;
    height_ = 0;
}

} // namespace rps::ui
