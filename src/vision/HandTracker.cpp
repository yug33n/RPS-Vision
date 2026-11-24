#include "vision/HandTracker.hpp"
#include "core/Logger.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <format>
#include <mutex>
#include <opencv2/imgproc.hpp>
#include <vector>

#if RPS_HAS_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

namespace rps::vision {

namespace {

constexpr int kPalmInputSize = 192;
constexpr int kLandmarkInputSize = 224;
constexpr float kPalmScoreThreshold = 0.50f;
constexpr float kLandmarkPresenceThreshold = 0.50f;
constexpr float kPi = 3.14159265358979323846f;

struct Anchor {
    float cx = 0.0f;
    float cy = 0.0f;
};

struct PalmDetection {
    float score = 0.0f;
    float cx = 0.0f;
    float cy = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float kp0_x = 0.0f;
    float kp0_y = 0.0f;
    float kp2_x = 0.0f;
    float kp2_y = 0.0f;
};

float sigmoid(float x) noexcept {
    const float clamped = std::clamp(x, -40.0f, 40.0f);
    return 1.0f / (1.0f + std::exp(-clamped));
}

std::vector<Anchor> generate_palm_anchors() {
    std::vector<Anchor> anchors;
    anchors.reserve(2016);

    auto add_layer = [&](int grid, int num_anchors) {
        for (int y = 0; y < grid; ++y) {
            for (int x = 0; x < grid; ++x) {
                const float cx = (static_cast<float>(x) + 0.5f) / static_cast<float>(grid);
                const float cy = (static_cast<float>(y) + 0.5f) / static_cast<float>(grid);
                for (int a = 0; a < num_anchors; ++a) {
                    anchors.push_back(Anchor{cx, cy});
                }
            }
        }
    };

    add_layer(24, 2); // 1152 anchors (stride 8)
    add_layer(12, 6); // 864 anchors (stride 16)
    return anchors;
}

void fill_rgb_tensor(const cv::Mat& rgb_float, bool is_nhwc, std::vector<float>& out) {
    const int h = rgb_float.rows;
    const int w = rgb_float.cols;
    out.resize(static_cast<std::size_t>(h * w * 3));

    if (is_nhwc) {
        std::memcpy(out.data(), rgb_float.ptr<float>(), out.size() * sizeof(float));
        return;
    }

    const int plane = h * w;
    float* dst_r = out.data();
    float* dst_g = out.data() + plane;
    float* dst_b = out.data() + 2 * plane;
    for (int y = 0; y < h; ++y) {
        const auto* row = rgb_float.ptr<cv::Vec3f>(y);
        for (int x = 0; x < w; ++x) {
            const int idx = y * w + x;
            dst_r[idx] = row[x][0];
            dst_g[idx] = row[x][1];
            dst_b[idx] = row[x][2];
        }
    }
}

} // namespace

struct HandTracker::Impl {
    TrackerStatus status = TrackerStatus::Uninitialized;
    std::string status_message = "Hand tracker not initialized.";
    mutable std::mutex state_mutex;

#if RPS_HAS_ONNXRUNTIME
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "RPSVision"};
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    std::unique_ptr<Ort::Session> palm_session;
    std::unique_ptr<Ort::Session> landmark_session;

    std::string palm_in_name;
    std::vector<std::string> palm_out_names;
    bool palm_nhwc = true;

    std::string landmark_in_name;
    std::vector<std::string> landmark_out_names;
    bool landmark_nhwc = true;

    std::vector<Anchor> anchors = generate_palm_anchors();
    std::vector<float> palm_tensor_buf;
    std::vector<float> landmark_tensor_buf;

    bool detect_palm(const cv::Mat& bgr, PalmDetection& best) {
        cv::Mat resized;
        cv::Mat rgb;
        cv::Mat rgb_f32;
        cv::resize(bgr, resized, cv::Size(kPalmInputSize, kPalmInputSize), 0.0, 0.0, cv::INTER_LINEAR);
        cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
        rgb.convertTo(rgb_f32, CV_32FC3, 1.0 / 255.0);
        fill_rgb_tensor(rgb_f32, palm_nhwc, palm_tensor_buf);

        const std::array<int64_t, 4> shape = palm_nhwc
            ? std::array<int64_t, 4>{1, kPalmInputSize, kPalmInputSize, 3}
            : std::array<int64_t, 4>{1, 3, kPalmInputSize, kPalmInputSize};

        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, palm_tensor_buf.data(), palm_tensor_buf.size(), shape.data(), shape.size()
        );

        const char* in_names[] = {palm_in_name.c_str()};
        std::vector<const char*> out_names;
        for (const auto& n : palm_out_names) {
            out_names.push_back(n.c_str());
        }

        auto outputs = palm_session->Run(
            Ort::RunOptions{nullptr}, in_names, &input_tensor, 1, out_names.data(), out_names.size()
        );

        const float* reg_ptr = nullptr;
        const float* cls_ptr = nullptr;
        std::size_t num_boxes = 0;

        for (auto& out : outputs) {
            const auto info = out.GetTensorTypeAndShapeInfo();
            const auto dims = info.GetShape();
            const std::size_t elems = info.GetElementCount();
            if (dims.size() >= 2 && dims.back() == 18) {
                reg_ptr = out.GetTensorData<float>();
                num_boxes = elems / 18;
            } else if (dims.size() >= 2 && dims.back() == 1) {
                cls_ptr = out.GetTensorData<float>();
            }
        }

        if (!reg_ptr || !cls_ptr || num_boxes != anchors.size()) {
            return false;
        }

        int best_idx = -1;
        float best_score = kPalmScoreThreshold;
        for (std::size_t i = 0; i < num_boxes; ++i) {
            const float score = sigmoid(cls_ptr[i]);
            if (score > best_score) {
                best_score = score;
                best_idx = static_cast<int>(i);
            }
        }
        if (best_idx < 0) {
            return false;
        }

        const Anchor& a = anchors[static_cast<std::size_t>(best_idx)];
        const float* r = reg_ptr + static_cast<std::size_t>(best_idx) * 18;
        const float inv = 1.0f / static_cast<float>(kPalmInputSize);

        best.score = best_score;
        best.cx = a.cx + r[0] * inv;
        best.cy = a.cy + r[1] * inv;
        best.w  = r[2] * inv;
        best.h  = r[3] * inv;
        best.kp0_x = a.cx + r[4] * inv;
        best.kp0_y = a.cy + r[5] * inv;
        best.kp2_x = a.cx + r[8] * inv;
        best.kp2_y = a.cy + r[9] * inv;
        return true;
    }

    bool extract_landmarks(const cv::Mat& bgr, const PalmDetection& palm, HandTrackingResult& res) {
        const float img_w = static_cast<float>(bgr.cols);
        const float img_h = static_cast<float>(bgr.rows);

        const float dx = (palm.kp2_x - palm.kp0_x) * img_w;
        const float dy = (palm.kp2_y - palm.kp0_y) * img_h;
        const float angle_rad = std::atan2(dy, dx) + kPi * 0.5f;
        const float angle_deg = angle_rad * (180.0f / kPi);

        const float box_size = std::max(palm.w * img_w, palm.h * img_h) * 2.6f;
        if (box_size < 16.0f) {
            return false;
        }

        const float shift_y = -0.5f * std::max(palm.w * img_w, palm.h * img_h);
        const float center_x = palm.cx * img_w - shift_y * std::sin(angle_rad);
        const float center_y = palm.cy * img_h + shift_y * std::cos(angle_rad);

        cv::Mat rot = cv::getRotationMatrix2D(cv::Point2f(center_x, center_y), angle_deg,
                                              static_cast<double>(kLandmarkInputSize) / box_size);
        rot.at<double>(0, 2) += (kLandmarkInputSize * 0.5) - center_x;
        rot.at<double>(1, 2) += (kLandmarkInputSize * 0.5) - center_y;

        cv::Mat crop_bgr;
        cv::warpAffine(bgr, crop_bgr, rot, cv::Size(kLandmarkInputSize, kLandmarkInputSize),
                       cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

        cv::Mat crop_rgb;
        cv::Mat crop_f32;
        cv::cvtColor(crop_bgr, crop_rgb, cv::COLOR_BGR2RGB);
        crop_rgb.convertTo(crop_f32, CV_32FC3, 1.0 / 255.0);
        fill_rgb_tensor(crop_f32, landmark_nhwc, landmark_tensor_buf);

        const std::array<int64_t, 4> shape = landmark_nhwc
            ? std::array<int64_t, 4>{1, kLandmarkInputSize, kLandmarkInputSize, 3}
            : std::array<int64_t, 4>{1, 3, kLandmarkInputSize, kLandmarkInputSize};

        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, landmark_tensor_buf.data(), landmark_tensor_buf.size(), shape.data(), shape.size()
        );

        const char* in_names[] = {landmark_in_name.c_str()};
        std::vector<const char*> out_names;
        for (const auto& n : landmark_out_names) {
            out_names.push_back(n.c_str());
        }

        auto outputs = landmark_session->Run(
            Ort::RunOptions{nullptr}, in_names, &input_tensor, 1, out_names.data(), out_names.size()
        );

        const float* lm_ptr = nullptr;
        float presence = palm.score;
        float handedness = 0.5f;

        for (std::size_t i = 0; i < outputs.size(); ++i) {
            const auto info = outputs[i].GetTensorTypeAndShapeInfo();
            const std::size_t count = info.GetElementCount();
            const float* ptr = outputs[i].GetTensorData<float>();
            if (count == 63) {
                lm_ptr = ptr;
            } else if (count == 1) {
                const float val = (ptr[0] < 0.0f || ptr[0] > 1.0f) ? sigmoid(ptr[0]) : ptr[0];
                if (i == 1) {
                    presence = val;
                } else {
                    handedness = val;
                }
            }
        }

        if (!lm_ptr || presence < kLandmarkPresenceThreshold) {
            return false;
        }

        cv::Mat inv_rot;
        cv::invertAffineTransform(rot, inv_rot);
        const double* m = inv_rot.ptr<double>();

        // Detect whether model outputs pixel space [0, 224] or normalized [0, 1]
        const float max_coord = std::max(std::abs(lm_ptr[0]), std::abs(lm_ptr[1]));
        const float coord_scale = (max_coord > 2.0f) ? 1.0f : static_cast<float>(kLandmarkInputSize);

        for (std::size_t i = 0; i < 21; ++i) {
            const double lx = static_cast<double>(lm_ptr[i * 3 + 0] * coord_scale);
            const double ly = static_cast<double>(lm_ptr[i * 3 + 1] * coord_scale);
            const float lz = lm_ptr[i * 3 + 2] / static_cast<float>(kLandmarkInputSize);

            const float orig_x = static_cast<float>(m[0] * lx + m[1] * ly + m[2]);
            const float orig_y = static_cast<float>(m[3] * lx + m[4] * ly + m[5]);

            res.landmarks[i] = LandmarkPoint{
                .x = std::clamp(orig_x / img_w, 0.0f, 1.0f),
                .y = std::clamp(orig_y / img_h, 0.0f, 1.0f),
                .z = lz
            };
        }

        res.hand_detected = true;
        res.presence_score = presence;
        res.handedness = handedness;
        return true;
    }
#endif
};

HandTracker::HandTracker() : impl_(std::make_unique<Impl>()) {}
HandTracker::~HandTracker() = default;

std::expected<void, std::string> HandTracker::init(
    const std::filesystem::path& palm_model_path,
    const std::filesystem::path& landmark_model_path
) {
    std::scoped_lock lock(impl_->state_mutex);

    for (const auto& model_path : {palm_model_path, landmark_model_path}) {
        if (!std::filesystem::exists(model_path)) {
            impl_->status = TrackerStatus::ModelsMissing;
            impl_->status_message = std::format(
                "Missing ONNX model: {}. See models/README_MODELS.md", model_path.string()
            );
            core::Logger::warn("{}", impl_->status_message);
            return std::unexpected(impl_->status_message);
        }
    }

#if !RPS_HAS_ONNXRUNTIME
    impl_->status = TrackerStatus::Error;
    impl_->status_message =
        "ONNX Runtime SDK not linked. Extract Microsoft.ML.OnnxRuntime into third_party/onnxruntime and rebuild.";
    core::Logger::warn("{}", impl_->status_message);
    return std::unexpected(impl_->status_message);
#else
    try {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(2);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        impl_->palm_session = std::make_unique<Ort::Session>(
            impl_->env, palm_model_path.wstring().c_str(), opts
        );
        impl_->landmark_session = std::make_unique<Ort::Session>(
            impl_->env, landmark_model_path.wstring().c_str(), opts
        );

        Ort::AllocatorWithDefaultOptions alloc;
        auto load_io_meta = [&](Ort::Session& sess, std::string& in_name,
                                std::vector<std::string>& out_names, bool& is_nhwc) {
            in_name = sess.GetInputNameAllocated(0, alloc).get();
            const auto shape = sess.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
            is_nhwc = !(shape.size() == 4 && shape[1] == 3);
            out_names.clear();
            for (std::size_t i = 0; i < sess.GetOutputCount(); ++i) {
                out_names.emplace_back(sess.GetOutputNameAllocated(i, alloc).get());
            }
        };

        load_io_meta(*impl_->palm_session, impl_->palm_in_name, impl_->palm_out_names, impl_->palm_nhwc);
        load_io_meta(*impl_->landmark_session, impl_->landmark_in_name, impl_->landmark_out_names, impl_->landmark_nhwc);

        impl_->status = TrackerStatus::Ready;
        impl_->status_message = "ONNX Runtime hand tracker ready.";
        core::Logger::info("Loaded ONNX hand tracking models successfully.");
        return {};
    } catch (const Ort::Exception& ex) {
        impl_->status = TrackerStatus::Error;
        impl_->status_message = std::format("ONNX Runtime error: {}", ex.what());
        core::Logger::error("{}", impl_->status_message);
        return std::unexpected(impl_->status_message);
    }
#endif
}

HandTrackingResult HandTracker::process_frame(const cv::Mat& bgr_frame, std::uint64_t sequence) {
    HandTrackingResult result;
    result.frame_sequence = sequence;

#if RPS_HAS_ONNXRUNTIME
    if (bgr_frame.empty() || impl_->status != TrackerStatus::Ready) {
        return result;
    }

    const auto t0 = std::chrono::steady_clock::now();
    try {
        PalmDetection palm;
        if (impl_->detect_palm(bgr_frame, palm)) {
            impl_->extract_landmarks(bgr_frame, palm, result);
        }
    } catch (const Ort::Exception& ex) {
        core::Logger::warn("ONNX inference error: {}", ex.what());
    }
    const auto t1 = std::chrono::steady_clock::now();
    result.inference_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
#else
    (void)bgr_frame;
#endif
    return result;
}

TrackerStatus HandTracker::status() const noexcept {
    std::scoped_lock lock(impl_->state_mutex);
    return impl_->status;
}

std::string HandTracker::status_message() const {
    std::scoped_lock lock(impl_->state_mutex);
    return impl_->status_message;
}

} // namespace rps::vision
