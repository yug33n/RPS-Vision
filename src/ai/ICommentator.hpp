#pragma once

#include "game/GameTypes.hpp"

#include <optional>
#include <string>

namespace rps::ai {

enum class Language {
    En,
    Ru
};

struct CommentaryContext {
    game::RoundRecord round;
    game::MatchStats stats;
    int consecutive_same_player_move = 0;
    int consecutive_draws = 0;
    Language language = Language::En;
};

struct CommentatorMessage {
    std::string author;
    std::string text;
    std::string timestamp;
    bool is_streaming = false;
};

/**
 * @brief Strategy interface for generating round commentary (offline templates or local LLM).
 */
class ICommentator {
public:
    virtual ~ICommentator() = default;

    /**
     * @brief Triggers commentary generation for a completed round.
     */
    virtual void request_commentary(const CommentaryContext& ctx) = 0;

    /**
     * @brief Polls for a newly generated or streaming-updated message.
     */
    [[nodiscard]] virtual std::optional<CommentatorMessage> poll_update() = 0;

    [[nodiscard]] virtual bool is_busy() const noexcept = 0;
};

} // namespace rps::ai
