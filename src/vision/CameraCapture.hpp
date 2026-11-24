#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <opencv2/core/mat.hpp>
#include <string>
#include <thread>
#include <vector>

namespace rps::vision {

enum class CameraStatus {
    Stopped,
    Starting,
    Running,
    Error
};

struct CameraDevice {
    int index = 0;
    std::string label;
};

struct CapturedFrame {
    cv::Mat bgr;
    std::uint64_t sequence = 0;
    double fps = 0.0;
};

/**
 * @brief Background-threaded OpenCV webcam reader with horizontal mirroring and frame sequence tracking.
 */
class CameraCapture {
public:
    CameraCapture() = default;
    ~CameraCapture();

    CameraCapture(const CameraCapture&) = delete;
    CameraCapture& operator=(const CameraCapture&) = delete;

    /**
     * @brief Starts capturing from the given camera device index on a background std::jthread.
     */
    void start(int device_index, int target_width, int target_height, int target_fps);

    /**
     * @brief Signals the capture thread to stop and joins it cleanly.
     */
    void stop();

    /**
     * @brief Copies the latest captured BGR frame only if its sequence differs from last_sequence.
     * @return true if a newer frame was copied into out_frame.
     */
    [[nodiscard]] bool try_get_latest_frame(CapturedFrame& out_frame, std::uint64_t last_sequence) const;

    void set_mirror(bool enabled) noexcept { mirror_.store(enabled, std::memory_order_relaxed); }
    [[nodiscard]] bool is_mirrored() const noexcept { return mirror_.load(std::memory_order_relaxed); }

    [[nodiscard]] CameraStatus status() const noexcept { return status_.load(std::memory_order_acquire); }
    [[nodiscard]] int device_index() const noexcept { return device_index_.load(std::memory_order_relaxed); }
    [[nodiscard]] double current_fps() const noexcept { return fps_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::string last_error() const;

    /**
     * @brief Probes camera indices [0, max_probe) and returns available devices.
     */
    [[nodiscard]] static std::vector<CameraDevice> enumerate_devices(int max_probe = 4);

private:
    void capture_loop(std::stop_token stop_token, int device_index, int width, int height, int fps);
    void set_error(std::string message);

    std::jthread worker_;
    mutable std::mutex frame_mutex_;
    cv::Mat latest_bgr_;
    std::uint64_t frame_sequence_ = 0;

    mutable std::mutex error_mutex_;
    std::string last_error_;

    std::atomic<CameraStatus> status_{CameraStatus::Stopped};
    std::atomic<int> device_index_{0};
    std::atomic<bool> mirror_{true};
    std::atomic<double> fps_{0.0};
};

} // namespace rps::vision
