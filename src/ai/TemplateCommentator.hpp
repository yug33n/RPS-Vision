#pragma once

#include "ai/ICommentator.hpp"

#include <array>
#include <optional>
#include <random>
#include <string>

namespace rps::ai {

/**
 * @brief Deterministic context-aware offline commentator with EN/RU reaction banks.
 */
class TemplateCommentator final : public ICommentator {
public:
    TemplateCommentator();

    void request_commentary(const CommentaryContext& ctx) override;
    [[nodiscard]] std::optional<CommentatorMessage> poll_update() override;
    [[nodiscard]] bool is_busy() const noexcept override { return false; }

    /**
     * @brief Generates an immediate commentary string for the given context.
     */
    [[nodiscard]] std::string generate_sync(const CommentaryContext& ctx);

private:
    enum class Category : std::size_t {
        PlayerWin = 0,
        PlayerStreak,
        AiWin,
        AiStreak,
        Draw,
        DrawChain,
        RepeatMove,
        InvalidMove,
        CheaterTaunt,
        Count
    };

    [[nodiscard]] Category classify_context(const CommentaryContext& ctx) const noexcept;

    std::mt19937 rng_;
    std::array<int, static_cast<std::size_t>(Category::Count)> last_index_{};
    std::optional<CommentatorMessage> pending_;
};

} // namespace rps::ai
