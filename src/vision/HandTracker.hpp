#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <opencv2/core/mat.hpp>
#include <string>

namespace rps::vision {

enum class TrackerStatus {
    Uninitialized,
    Ready,
    ModelsMissing,
    Error
};

struct LandmarkPoint {
    float x = 0.0f; // Normalized [0, 1] in full-frame image coordinates
    float y = 0.0f; // Normalized [0, 1] in full-frame image coordinates
    float z = 0.0f; // Relative depth from wrist
};

struct HandTrackingResult {
    bool hand_detected = false;
    float presence_score = 0.0f;
    float handedness = 0.5f;
    std::array<LandmarkPoint, 21> landmarks{};
    double inference_ms = 0.0;
    std::uint64_t frame_sequence = 0;
};

/**
 * @brief Two-stage MediaPipe ONNX hand tracking pipeline (palm detection + 21-keypoint landmarks).
 */
class HandTracker {
public:
    HandTracker();
    ~HandTracker();

    HandTracker(const HandTracker&) = delete;
    HandTracker& operator=(const HandTracker&) = delete;

    /**
     * @brief Loads palm detection and hand landmark ONNX models.
     */
    [[nodiscard]] std::expected<void, std::string> init(
        const std::filesystem::path& palm_model_path,
        const std::filesystem::path& landmark_model_path
    );

    /**
     * @brief Runs palm detection + 21-point landmark extraction on a BGR frame.
     */
    [[nodiscard]] HandTrackingResult process_frame(const cv::Mat& bgr_frame, std::uint64_t sequence = 0);

    [[nodiscard]] TrackerStatus status() const noexcept;
    [[nodiscard]] std::string status_message() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rps::vision
