#include "vision/CameraCapture.hpp"
#include "core/Logger.hpp"

#include <chrono>
#include <format>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

namespace rps::vision {

namespace {

bool open_capture_device(cv::VideoCapture& cap, int index) {
#ifdef _WIN32
    if (cap.open(index, cv::CAP_DSHOW) && cap.isOpened()) {
        return true;
    }
    cap.release();
#endif
    return cap.open(index, cv::CAP_ANY) && cap.isOpened();
}

} // namespace

CameraCapture::~CameraCapture() {
    stop();
}

void CameraCapture::start(int device_index, int target_width, int target_height, int target_fps) {
    stop();

    device_index_.store(device_index, std::memory_order_relaxed);
    fps_.store(0.0, std::memory_order_relaxed);
    set_error({});
    status_.store(CameraStatus::Starting, std::memory_order_release);

    worker_ = std::jthread([this, device_index, target_width, target_height, target_fps](std::stop_token st) {
        capture_loop(std::move(st), device_index, target_width, target_height, target_fps);
    });
}

void CameraCapture::stop() {
    if (worker_.joinable()) {
        worker_.request_stop();
        worker_.join();
    }
    status_.store(CameraStatus::Stopped, std::memory_order_release);
}

bool CameraCapture::try_get_latest_frame(CapturedFrame& out_frame, std::uint64_t last_sequence) const {
    std::scoped_lock lock(frame_mutex_);
    if (latest_bgr_.empty() || frame_sequence_ == 0 || frame_sequence_ == last_sequence) {
        return false;
    }
    latest_bgr_.copyTo(out_frame.bgr);
    out_frame.sequence = frame_sequence_;
    out_frame.fps = fps_.load(std::memory_order_relaxed);
    return true;
}

std::string CameraCapture::last_error() const {
    std::scoped_lock lock(error_mutex_);
    return last_error_;
}

std::vector<CameraDevice> CameraCapture::enumerate_devices(int max_probe) {
    std::vector<CameraDevice> devices;
    for (int i = 0; i < max_probe; ++i) {
        cv::VideoCapture probe;
        if (open_capture_device(probe, i)) {
            const int w = static_cast<int>(probe.get(cv::CAP_PROP_FRAME_WIDTH));
            const int h = static_cast<int>(probe.get(cv::CAP_PROP_FRAME_HEIGHT));
            devices.push_back(CameraDevice{
                .index = i,
                .label = (w > 0 && h > 0)
                    ? std::format("Camera {} ({}x{})", i, w, h)
                    : std::format("Camera {}", i)
            });
            probe.release();
        }
    }
    return devices;
}

void CameraCapture::capture_loop(
    std::stop_token stop_token,
    int device_index,
    int width,
    int height,
    int fps
) {
    cv::VideoCapture cap;
    if (!open_capture_device(cap, device_index)) {
        const std::string msg = std::format("Failed to open camera device {}", device_index);
        core::Logger::warn("{}", msg);
        set_error(msg);
        status_.store(CameraStatus::Error, std::memory_order_release);
        return;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, static_cast<double>(width));
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, static_cast<double>(height));
    cap.set(cv::CAP_PROP_FPS, static_cast<double>(fps));
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1.0);

    core::Logger::info(
        "Camera {} opened ({}x{} @ {:.0f} FPS)",
        device_index,
        static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH)),
        static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT)),
        cap.get(cv::CAP_PROP_FPS)
    );

    status_.store(CameraStatus::Running, std::memory_order_release);

    cv::Mat raw_frame;
    cv::Mat processed_frame;
    auto prev_tick = std::chrono::steady_clock::now();
    double smoothed_fps = 0.0;

    while (!stop_token.stop_requested()) {
        if (!cap.read(raw_frame) || raw_frame.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (mirror_.load(std::memory_order_relaxed)) {
            cv::flip(raw_frame, processed_frame, 1);
        } else {
            processed_frame = raw_frame;
        }

        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - prev_tick).count();
        prev_tick = now;
        if (dt > 1e-4) {
            const double inst_fps = 1.0 / dt;
            smoothed_fps = (smoothed_fps <= 0.0) ? inst_fps : (0.9 * smoothed_fps + 0.1 * inst_fps);
            fps_.store(smoothed_fps, std::memory_order_relaxed);
        }

        {
            std::scoped_lock lock(frame_mutex_);
            processed_frame.copyTo(latest_bgr_);
            ++frame_sequence_;
        }
    }

    cap.release();
    core::Logger::info("Camera {} stopped", device_index);
}

void CameraCapture::set_error(std::string message) {
    std::scoped_lock lock(error_mutex_);
    last_error_ = std::move(message);
}

} // namespace rps::vision
