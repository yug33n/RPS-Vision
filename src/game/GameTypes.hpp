#pragma once

#include "vision/GestureClassifier.hpp"

#include <string>
#include <string_view>

namespace rps::game {

enum class Move {
    Rock = 0,
    Paper = 1,
    Scissors = 2,
    None = 3
};

enum class RoundResult {
    PlayerWin,
    AiWin,
    Draw,
    InvalidMove
};

enum class GameState {
    Idle,
    Countdown,
    Capture,
    RoundEnd
};

enum class AiDifficulty {
    Casual = 0,
    Markov = 1,
    Cheater = 2
};

struct RoundRecord {
    int round_number = 0;
    Move player_move = Move::None;
    Move ai_move = Move::None;
    RoundResult result = RoundResult::InvalidMove;
    AiDifficulty difficulty = AiDifficulty::Markov;
    std::string timestamp;
};

struct MatchStats {
    int total_rounds = 0;
    int player_wins = 0;
    int ai_wins = 0;
    int draws = 0;
    int invalid_moves = 0;
    int current_streak = 0; // Positive: player streak, Negative: AI streak
    int best_player_streak = 0;

    [[nodiscard]] double winrate_percent() const noexcept {
        const int decided = player_wins + ai_wins;
        if (decided <= 0) {
            return 0.0;
        }
        return (static_cast<double>(player_wins) * 100.0) / static_cast<double>(decided);
    }
};

[[nodiscard]] constexpr Move move_from_gesture(vision::HandGesture gesture) noexcept {
    switch (gesture) {
        case vision::HandGesture::Rock:     return Move::Rock;
        case vision::HandGesture::Paper:    return Move::Paper;
        case vision::HandGesture::Scissors: return Move::Scissors;
        case vision::HandGesture::Unknown:  return Move::None;
    }
    return Move::None;
}

[[nodiscard]] constexpr Move counter_move(Move m) noexcept {
    switch (m) {
        case Move::Rock:     return Move::Paper;
        case Move::Paper:    return Move::Scissors;
        case Move::Scissors: return Move::Rock;
        case Move::None:     return Move::Rock;
    }
    return Move::Rock;
}

[[nodiscard]] constexpr RoundResult evaluate_round(Move player, Move ai) noexcept {
    if (player == Move::None) {
        return RoundResult::InvalidMove;
    }
    if (player == ai) {
        return RoundResult::Draw;
    }
    if ((player == Move::Rock && ai == Move::Scissors) ||
        (player == Move::Paper && ai == Move::Rock) ||
        (player == Move::Scissors && ai == Move::Paper)) {
        return RoundResult::PlayerWin;
    }
    return RoundResult::AiWin;
}

[[nodiscard]] constexpr std::string_view to_string(Move m) noexcept {
    switch (m) {
        case Move::Rock:     return "ROCK";
        case Move::Paper:    return "PAPER";
        case Move::Scissors: return "SCISSORS";
        case Move::None:     return "NONE";
    }
    return "NONE";
}

[[nodiscard]] constexpr std::string_view to_string(RoundResult r) noexcept {
    switch (r) {
        case RoundResult::PlayerWin:   return "YOU WIN";
        case RoundResult::AiWin:       return "AI WINS";
        case RoundResult::Draw:        return "DRAW";
        case RoundResult::InvalidMove: return "INVALID";
    }
    return "INVALID";
}

[[nodiscard]] constexpr std::string_view to_string(AiDifficulty d) noexcept {
    switch (d) {
        case AiDifficulty::Casual:  return "Casual (RNG)";
        case AiDifficulty::Markov:  return "Tactician (Markov-2)";
        case AiDifficulty::Cheater: return "Unbeatable (Cheater)";
    }
    return "Tactician (Markov-2)";
}

} // namespace rps::game
