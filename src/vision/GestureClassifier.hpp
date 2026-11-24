#pragma once

#include "vision/HandTracker.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace rps::vision {

enum class HandGesture {
    Unknown,
    Rock,
    Paper,
    Scissors
};

struct GestureResult {
    HandGesture gesture = HandGesture::Unknown;
    float confidence = 0.0f;
    bool is_stable = false;
    std::array<bool, 5> fingers_extended{}; // Thumb, Index, Middle, Ring, Pinky
};

/**
 * @brief Classifies 21 3D hand landmarks into RPS gestures with 5-frame temporal smoothing.
 */
class GestureClassifier {
public:
    static constexpr std::size_t kSmoothingWindow = 5;

    /**
     * @brief Evaluates 3D finger extension geometry and updates the 5-frame smoothing buffer.
     */
    [[nodiscard]] GestureResult classify(const HandTrackingResult& tracking);

    /**
     * @brief Clears the temporal smoothing history.
     */
    void reset() noexcept;

    [[nodiscard]] static constexpr std::string_view to_string(HandGesture gesture) noexcept {
        switch (gesture) {
            case HandGesture::Rock:     return "ROCK";
            case HandGesture::Paper:    return "PAPER";
            case HandGesture::Scissors: return "SCISSORS";
            case HandGesture::Unknown:  return "UNKNOWN";
        }
        return "UNKNOWN";
    }

private:
    struct FrameSample {
        HandGesture gesture = HandGesture::Unknown;
        float confidence = 0.0f;
    };

    std::array<FrameSample, kSmoothingWindow> history_{};
    std::size_t history_count_ = 0;
    std::size_t history_head_ = 0;
};

} // namespace rps::vision
