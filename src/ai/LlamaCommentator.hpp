#pragma once

#include "ai/ICommentator.hpp"
#include "ai/TemplateCommentator.hpp"

#include <atomic>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace rps::ai {

/**
 * @brief Embedded llama.cpp v1.0.0 local GGUF commentator running asynchronously on std::jthread.
 */
class LlamaCommentator final : public ICommentator {
public:
    LlamaCommentator();
    ~LlamaCommentator() override;

    LlamaCommentator(const LlamaCommentator&) = delete;
    LlamaCommentator& operator=(const LlamaCommentator&) = delete;

    /**
     * @brief Loads a local .gguf model file without blocking if missing.
     */
    [[nodiscard]] std::expected<void, std::string> init(const std::filesystem::path& model_path);

    void request_commentary(const CommentaryContext& ctx) override;
    [[nodiscard]] std::optional<CommentatorMessage> poll_update() override;
    [[nodiscard]] bool is_busy() const noexcept override {
        return busy_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool is_available() const noexcept {
        return available_.load(std::memory_order_acquire);
    }
    [[nodiscard]] std::string status_message() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    TemplateCommentator fallback_;
    std::atomic<bool> available_{false};
    std::atomic<bool> busy_{false};
};

} // namespace rps::ai
