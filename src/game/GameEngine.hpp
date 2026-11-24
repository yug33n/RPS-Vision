#pragma once

#include "game/GameTypes.hpp"
#include "game/MarkovPredictor.hpp"
#include "vision/GestureClassifier.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rps::game {

/**
 * @brief Finite state machine driving RPS rounds, countdowns, anti-cheat capture, and stats.
 */
class GameEngine {
public:
    static constexpr double kCountdownSeconds = 3.0;
    static constexpr double kCaptureWindowSeconds = 0.40;
    static constexpr double kAutoNextDelaySeconds = 2.2;

    explicit GameEngine(std::filesystem::path stats_path = "data/stats.json");

    /**
     * @brief Starts a new 3-2-1-SHOOT round countdown.
     */
    void start_round();

    /**
     * @brief Cancels an active round and returns to Idle.
     */
    void cancel_round();

    /**
     * @brief Advances timers and state transitions based on the latest CV gesture.
     * @return Optional completed RoundRecord on the exact frame a round finishes.
     */
    std::optional<RoundRecord> update(const vision::GestureResult& live_gesture);

    /**
     * @brief Clears all match history, statistics, and Markov transition counts.
     */
    void reset_stats();

    void set_difficulty(AiDifficulty difficulty) noexcept { difficulty_ = difficulty; }
    [[nodiscard]] AiDifficulty difficulty() const noexcept { return difficulty_; }

    void set_auto_next_round(bool enabled) noexcept { auto_next_round_ = enabled; }
    [[nodiscard]] bool auto_next_round() const noexcept { return auto_next_round_; }

    [[nodiscard]] GameState state() const noexcept { return state_; }
    [[nodiscard]] double countdown_remaining() const noexcept { return countdown_remaining_; }
    [[nodiscard]] float countdown_progress() const noexcept;
    [[nodiscard]] std::string countdown_label() const;

    [[nodiscard]] const MatchStats& stats() const noexcept { return stats_; }
    [[nodiscard]] const std::vector<RoundRecord>& history() const noexcept { return history_; }
    [[nodiscard]] const std::optional<RoundRecord>& last_round() const noexcept { return last_round_; }
    [[nodiscard]] const std::vector<float>& winrate_progression() const noexcept { return winrate_series_; }
    [[nodiscard]] Move locked_ai_move() const noexcept { return locked_ai_move_; }

private:
    void enter_capture(const vision::GestureResult& live_gesture);
    RoundRecord finalize_round(Move player_move);
    void rebuild_winrate_series();

    std::filesystem::path stats_path_;
    MarkovPredictor markov_;
    MatchStats stats_;
    std::vector<RoundRecord> history_;
    std::vector<float> winrate_series_;
    std::optional<RoundRecord> last_round_;

    GameState state_ = GameState::Idle;
    AiDifficulty difficulty_ = AiDifficulty::Markov;
    bool auto_next_round_ = false;

    std::chrono::steady_clock::time_point state_start_;
    double countdown_remaining_ = kCountdownSeconds;

    Move locked_ai_move_ = Move::None;
    Move best_capture_move_ = Move::None;
    float best_capture_conf_ = 0.0f;

    Move prev_valid_player_move_ = Move::None;
    Move prev_valid_ai_move_ = Move::None;
};

} // namespace rps::game
