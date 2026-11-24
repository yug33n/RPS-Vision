#include "game/MarkovPredictor.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <nlohmann/json.hpp>

namespace rps::game {

MarkovPredictor::MarkovPredictor()
    : rng_(std::random_device{}()) {}

void MarkovPredictor::record_transition(Move prev_player, Move prev_ai, Move next_player) noexcept {
    if (!is_valid_move(prev_player) || !is_valid_move(prev_ai) || !is_valid_move(next_player)) {
        return;
    }
    const auto p = static_cast<std::size_t>(prev_player);
    const auto a = static_cast<std::size_t>(prev_ai);
    const auto n = static_cast<std::size_t>(next_player);
    transitions_[p][a][n] += 1;
}

std::optional<Move> MarkovPredictor::predict_player_move(Move prev_player, Move prev_ai) const noexcept {
    if (!is_valid_move(prev_player) || !is_valid_move(prev_ai)) {
        return std::nullopt;
    }

    const auto& counts = transitions_[static_cast<std::size_t>(prev_player)]
                                     [static_cast<std::size_t>(prev_ai)];
    const int total = counts[0] + counts[1] + counts[2];
    if (total < 2) {
        return std::nullopt;
    }

    std::size_t best_idx = 0;
    bool tie = false;
    for (std::size_t i = 1; i < 3; ++i) {
        if (counts[i] > counts[best_idx]) {
            best_idx = i;
            tie = false;
        } else if (counts[i] == counts[best_idx]) {
            tie = true;
        }
    }

    if (tie) {
        return std::nullopt;
    }
    return static_cast<Move>(best_idx);
}

Move MarkovPredictor::choose_ai_move(Move prev_player, Move prev_ai) {
    if (const auto predicted = predict_player_move(prev_player, prev_ai)) {
        return counter_move(*predicted);
    }
    return random_move();
}

Move MarkovPredictor::random_move() {
    std::uniform_int_distribution<int> dist(0, 2);
    return static_cast<Move>(dist(rng_));
}

void MarkovPredictor::reset() noexcept {
    for (auto& row : transitions_) {
        for (auto& cell : row) {
            cell.fill(0);
        }
    }
}

std::expected<void, std::string> MarkovPredictor::load(
    const std::filesystem::path& path,
    MatchStats& out_stats,
    std::vector<RoundRecord>& out_history
) {
    if (!std::filesystem::exists(path)) {
        return {};
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return std::unexpected(std::format("Failed to open stats file: {}", path.string()));
    }

    nlohmann::json j;
    try {
        file >> j;
    } catch (const nlohmann::json::parse_error& err) {
        return std::unexpected(std::format("Stats JSON parse error: {}", err.what()));
    }

    if (j.contains("markov_matrix") && j["markov_matrix"].is_array() && j["markov_matrix"].size() == 3) {
        for (std::size_t p = 0; p < 3; ++p) {
            if (!j["markov_matrix"][p].is_array() || j["markov_matrix"][p].size() != 3) {
                continue;
            }
            for (std::size_t a = 0; a < 3; ++a) {
                if (!j["markov_matrix"][p][a].is_array() || j["markov_matrix"][p][a].size() != 3) {
                    continue;
                }
                for (std::size_t n = 0; n < 3; ++n) {
                    transitions_[p][a][n] = j["markov_matrix"][p][a][n].get<int>();
                }
            }
        }
    }

    if (j.contains("stats") && j["stats"].is_object()) {
        const auto& s = j["stats"];
        out_stats.total_rounds = s.value("total_rounds", 0);
        out_stats.player_wins = s.value("player_wins", 0);
        out_stats.ai_wins = s.value("ai_wins", 0);
        out_stats.draws = s.value("draws", 0);
        out_stats.invalid_moves = s.value("invalid_moves", 0);
        out_stats.current_streak = s.value("current_streak", 0);
        out_stats.best_player_streak = s.value("best_player_streak", 0);
    }

    out_history.clear();
    if (j.contains("history") && j["history"].is_array()) {
        for (const auto& item : j["history"]) {
            if (!item.is_object()) {
                continue;
            }
            RoundRecord rec;
            rec.round_number = item.value("round", 0);
            rec.player_move = static_cast<Move>(std::clamp(item.value("player_move", 3), 0, 3));
            rec.ai_move = static_cast<Move>(std::clamp(item.value("ai_move", 3), 0, 3));
            rec.result = static_cast<RoundResult>(std::clamp(item.value("result", 3), 0, 3));
            rec.difficulty = static_cast<AiDifficulty>(std::clamp(item.value("difficulty", 1), 0, 2));
            rec.timestamp = item.value("timestamp", "");
            out_history.push_back(std::move(rec));
        }
    }

    return {};
}

std::expected<void, std::string> MarkovPredictor::save(
    const std::filesystem::path& path,
    const MatchStats& stats,
    const std::vector<RoundRecord>& history
) const {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    nlohmann::json matrix = nlohmann::json::array();
    for (std::size_t p = 0; p < 3; ++p) {
        nlohmann::json row = nlohmann::json::array();
        for (std::size_t a = 0; a < 3; ++a) {
            row.push_back({transitions_[p][a][0], transitions_[p][a][1], transitions_[p][a][2]});
        }
        matrix.push_back(std::move(row));
    }

    nlohmann::json hist_json = nlohmann::json::array();
    constexpr std::size_t kMaxSavedHistory = 200;
    const std::size_t start_idx = history.size() > kMaxSavedHistory
        ? (history.size() - kMaxSavedHistory)
        : 0;
    for (std::size_t i = start_idx; i < history.size(); ++i) {
        const auto& rec = history[i];
        hist_json.push_back({
            {"round", rec.round_number},
            {"player_move", static_cast<int>(rec.player_move)},
            {"ai_move", static_cast<int>(rec.ai_move)},
            {"result", static_cast<int>(rec.result)},
            {"difficulty", static_cast<int>(rec.difficulty)},
            {"timestamp", rec.timestamp}
        });
    }

    const nlohmann::json root = {
        {"stats", {
            {"total_rounds", stats.total_rounds},
            {"player_wins", stats.player_wins},
            {"ai_wins", stats.ai_wins},
            {"draws", stats.draws},
            {"invalid_moves", stats.invalid_moves},
            {"current_streak", stats.current_streak},
            {"best_player_streak", stats.best_player_streak}
        }},
        {"markov_matrix", std::move(matrix)},
        {"history", std::move(hist_json)}
    };

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return std::unexpected(std::format("Failed to write stats file: {}", path.string()));
    }
    file << root.dump(2) << '\n';
    return {};
}

} // namespace rps::game
