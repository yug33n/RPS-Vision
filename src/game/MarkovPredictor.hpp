#pragma once

#include "game/GameTypes.hpp"

#include <array>
#include <expected>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace rps::game {

/**
 * @brief 2nd-order Markov Chain predictor tracking (prev_player, prev_ai) -> next_player transitions.
 */
class MarkovPredictor {
public:
    MarkovPredictor();

    /**
     * @brief Records a valid player transition for (prev_player, prev_ai) -> next_player.
     */
    void record_transition(Move prev_player, Move prev_ai, Move next_player) noexcept;

    /**
     * @brief Predicts the player's most likely next move given the previous round pair.
     */
    [[nodiscard]] std::optional<Move> predict_player_move(Move prev_player, Move prev_ai) const noexcept;

    /**
     * @brief Selects an AI counter-move using 2nd-order Markov statistics or uniform RNG fallback.
     */
    [[nodiscard]] Move choose_ai_move(Move prev_player, Move prev_ai);

    /**
     * @brief Picks a uniform random valid move (Rock, Paper, or Scissors).
     */
    [[nodiscard]] Move random_move();

    /**
     * @brief Resets all transition counts.
     */
    void reset() noexcept;

    /**
     * @brief Loads Markov matrix, stats, and round history from JSON.
     */
    [[nodiscard]] std::expected<void, std::string> load(
        const std::filesystem::path& path,
        MatchStats& out_stats,
        std::vector<RoundRecord>& out_history
    );

    /**
     * @brief Saves Markov matrix, stats, and round history to JSON.
     */
    [[nodiscard]] std::expected<void, std::string> save(
        const std::filesystem::path& path,
        const MatchStats& stats,
        const std::vector<RoundRecord>& history
    ) const;

private:
    static constexpr bool is_valid_move(Move m) noexcept {
        return m == Move::Rock || m == Move::Paper || m == Move::Scissors;
    }

    // [prev_player][prev_ai][next_player]
    std::array<std::array<std::array<int, 3>, 3>, 3> transitions_{};
    std::mt19937 rng_;
};

} // namespace rps::game
