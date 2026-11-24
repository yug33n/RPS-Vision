#include "game/GameEngine.hpp"
#include "core/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <format>

namespace rps::game {

namespace {

std::string current_timestamp_hhmmss() {
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    return std::format("{:%H:%M:%S}", now);
}

} // namespace

GameEngine::GameEngine(std::filesystem::path stats_path)
    : stats_path_(std::move(stats_path)) {
    if (auto res = markov_.load(stats_path_, stats_, history_); !res.has_value()) {
        core::Logger::warn("Could not load match stats: {}", res.error());
    }
    for (auto it = history_.rbegin(); it != history_.rend(); ++it) {
        if (it->player_move != Move::None && it->ai_move != Move::None) {
            prev_valid_player_move_ = it->player_move;
            prev_valid_ai_move_ = it->ai_move;
            break;
        }
    }
    rebuild_winrate_series();
}

void GameEngine::start_round() {
    state_ = GameState::Countdown;
    state_start_ = std::chrono::steady_clock::now();
    countdown_remaining_ = kCountdownSeconds;
    locked_ai_move_ = Move::None;
    best_capture_move_ = Move::None;
    best_capture_conf_ = 0.0f;
}

void GameEngine::cancel_round() {
    state_ = GameState::Idle;
    countdown_remaining_ = kCountdownSeconds;
    locked_ai_move_ = Move::None;
}

std::optional<RoundRecord> GameEngine::update(const vision::GestureResult& live_gesture) {
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(now - state_start_).count();

    if (state_ == GameState::Countdown) {
        countdown_remaining_ = std::max(0.0, kCountdownSeconds - elapsed);
        if (elapsed >= kCountdownSeconds) {
            enter_capture(live_gesture);
        }
        return std::nullopt;
    }

    if (state_ == GameState::Capture) {
        const Move live_move = move_from_gesture(live_gesture.gesture);
        if (live_move != Move::None && live_gesture.confidence >= best_capture_conf_) {
            best_capture_move_ = live_move;
            best_capture_conf_ = live_gesture.confidence;
        }

        const bool stable_lock = (best_capture_move_ != Move::None) && live_gesture.is_stable;
        if (stable_lock || elapsed >= kCaptureWindowSeconds) {
            if (difficulty_ == AiDifficulty::Cheater && best_capture_move_ != Move::None) {
                locked_ai_move_ = counter_move(best_capture_move_);
            }
            return finalize_round(best_capture_move_);
        }
        return std::nullopt;
    }

    if (state_ == GameState::RoundEnd && auto_next_round_ && elapsed >= kAutoNextDelaySeconds) {
        start_round();
    }

    return std::nullopt;
}

void GameEngine::reset_stats() {
    markov_.reset();
    stats_ = MatchStats{};
    history_.clear();
    winrate_series_.clear();
    last_round_.reset();
    prev_valid_player_move_ = Move::None;
    prev_valid_ai_move_ = Move::None;

    if (auto res = markov_.save(stats_path_, stats_, history_); !res.has_value()) {
        core::Logger::warn("Failed to reset stats file: {}", res.error());
    }
}

float GameEngine::countdown_progress() const noexcept {
    if (state_ != GameState::Countdown) {
        return 0.0f;
    }
    return std::clamp(
        static_cast<float>((kCountdownSeconds - countdown_remaining_) / kCountdownSeconds),
        0.0f,
        1.0f
    );
}

std::string GameEngine::countdown_label() const {
    if (state_ == GameState::Capture) {
        return "SHOOT!";
    }
    if (state_ != GameState::Countdown) {
        return "";
    }
    if (countdown_remaining_ > 2.0) return "3";
    if (countdown_remaining_ > 1.0) return "2";
    if (countdown_remaining_ > 0.15) return "1";
    return "SHOOT!";
}

void GameEngine::enter_capture(const vision::GestureResult& live_gesture) {
    state_ = GameState::Capture;
    state_start_ = std::chrono::steady_clock::now();

    const Move live_move = move_from_gesture(live_gesture.gesture);
    best_capture_move_ = live_move;
    best_capture_conf_ = (live_move != Move::None) ? live_gesture.confidence : 0.0f;

    switch (difficulty_) {
        case AiDifficulty::Casual:
            locked_ai_move_ = markov_.random_move();
            break;
        case AiDifficulty::Markov:
            locked_ai_move_ = markov_.choose_ai_move(prev_valid_player_move_, prev_valid_ai_move_);
            break;
        case AiDifficulty::Cheater:
            locked_ai_move_ = (live_move != Move::None)
                ? counter_move(live_move)
                : markov_.random_move();
            break;
    }
}

RoundRecord GameEngine::finalize_round(Move player_move) {
    const RoundResult result = evaluate_round(player_move, locked_ai_move_);

    stats_.total_rounds += 1;
    switch (result) {
        case RoundResult::PlayerWin:
            stats_.player_wins += 1;
            stats_.current_streak = (stats_.current_streak > 0) ? (stats_.current_streak + 1) : 1;
            stats_.best_player_streak = std::max(stats_.best_player_streak, stats_.current_streak);
            break;
        case RoundResult::AiWin:
            stats_.ai_wins += 1;
            stats_.current_streak = (stats_.current_streak < 0) ? (stats_.current_streak - 1) : -1;
            break;
        case RoundResult::Draw:
            stats_.draws += 1;
            break;
        case RoundResult::InvalidMove:
            stats_.invalid_moves += 1;
            break;
    }

    if (player_move != Move::None) {
        markov_.record_transition(prev_valid_player_move_, prev_valid_ai_move_, player_move);
        prev_valid_player_move_ = player_move;
        prev_valid_ai_move_ = locked_ai_move_;
    }

    RoundRecord rec{
        .round_number = stats_.total_rounds,
        .player_move = player_move,
        .ai_move = locked_ai_move_,
        .result = result,
        .difficulty = difficulty_,
        .timestamp = current_timestamp_hhmmss()
    };

    history_.push_back(rec);
    last_round_ = rec;
    rebuild_winrate_series();

    if (auto res = markov_.save(stats_path_, stats_, history_); !res.has_value()) {
        core::Logger::warn("Failed to persist stats: {}", res.error());
    }

    state_ = GameState::RoundEnd;
    state_start_ = std::chrono::steady_clock::now();
    return rec;
}

void GameEngine::rebuild_winrate_series() {
    winrate_series_.clear();
    int wins = 0;
    int decided = 0;
    for (const auto& rec : history_) {
        if (rec.result == RoundResult::PlayerWin) {
            ++wins;
            ++decided;
        } else if (rec.result == RoundResult::AiWin) {
            ++decided;
        }
        if (decided > 0) {
            winrate_series_.push_back(
                static_cast<float>((static_cast<double>(wins) * 100.0) / static_cast<double>(decided))
            );
        }
    }
}

} // namespace rps::game
