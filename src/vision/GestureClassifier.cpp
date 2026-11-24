#include "vision/GestureClassifier.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace rps::vision {

namespace {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Vec3 sub(const LandmarkPoint& a, const LandmarkPoint& b) noexcept {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

float length(const Vec3& v) noexcept {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float cosine_similarity(const Vec3& a, const Vec3& b) noexcept {
    const float la = length(a);
    const float lb = length(b);
    if (la < 1e-6f || lb < 1e-6f) {
        return 0.0f;
    }
    const float dot = a.x * b.x + a.y * b.y + a.z * b.z;
    return std::clamp(dot / (la * lb), -1.0f, 1.0f);
}

float distance_3d(const LandmarkPoint& a, const LandmarkPoint& b) noexcept {
    return length(sub(a, b));
}

struct FingerEval {
    bool extended = false;
    float score = 0.0f; // [0..1] extension clarity when extended, curl clarity when curled
};

FingerEval evaluate_finger(
    const std::array<LandmarkPoint, 21>& lm,
    std::size_t mcp_idx,
    std::size_t pip_idx,
    std::size_t tip_idx
) noexcept {
    const Vec3 v_base = sub(lm[pip_idx], lm[mcp_idx]);
    const Vec3 v_dist = sub(lm[tip_idx], lm[pip_idx]);
    const float straightness = cosine_similarity(v_base, v_dist);

    const float wrist_to_pip = distance_3d(lm[0], lm[pip_idx]);
    const float wrist_to_tip = distance_3d(lm[0], lm[tip_idx]);
    const float reach_ratio = (wrist_to_pip > 1e-5f) ? (wrist_to_tip / wrist_to_pip) : 0.0f;

    const bool is_extended = (straightness > 0.55f) && (reach_ratio > 1.08f);
    const float ext_conf = std::clamp((straightness - 0.35f) * 1.4f, 0.0f, 1.0f);
    const float curl_conf = std::clamp((0.65f - straightness) * 1.2f, 0.0f, 1.0f);

    return FingerEval{
        .extended = is_extended,
        .score = is_extended ? ext_conf : curl_conf
    };
}

bool is_thumb_extended(const std::array<LandmarkPoint, 21>& lm) noexcept {
    const Vec3 v1 = sub(lm[3], lm[2]);
    const Vec3 v2 = sub(lm[4], lm[3]);
    const float straightness = cosine_similarity(v1, v2);
    const float tip_to_pinky = distance_3d(lm[4], lm[17]);
    const float ip_to_pinky = distance_3d(lm[3], lm[17]);
    return (straightness > 0.55f) && (tip_to_pinky > ip_to_pinky * 1.06f);
}

std::pair<HandGesture, float> classify_raw(
    const HandTrackingResult& tracking,
    std::array<bool, 5>& out_fingers
) noexcept {
    out_fingers.fill(false);
    if (!tracking.hand_detected) {
        return {HandGesture::Unknown, 0.0f};
    }

    const auto& lm = tracking.landmarks;
    const bool thumb = is_thumb_extended(lm);
    const FingerEval index  = evaluate_finger(lm, 5, 6, 8);
    const FingerEval middle = evaluate_finger(lm, 9, 10, 12);
    const FingerEval ring   = evaluate_finger(lm, 13, 14, 16);
    const FingerEval pinky  = evaluate_finger(lm, 17, 18, 20);

    out_fingers = {thumb, index.extended, middle.extended, ring.extended, pinky.extended};

    const float avg_clarity = 0.25f * (index.score + middle.score + ring.score + pinky.score);
    const float base_conf = std::clamp(0.65f + 0.34f * avg_clarity * tracking.presence_score, 0.50f, 0.99f);

    // Rock: all four main fingers curled
    if (!index.extended && !middle.extended && !ring.extended && !pinky.extended) {
        return {HandGesture::Rock, base_conf};
    }

    // Paper: all four main fingers extended
    if (index.extended && middle.extended && ring.extended && pinky.extended) {
        return {HandGesture::Paper, base_conf};
    }

    // Scissors: index + middle extended, ring + pinky curled
    if (index.extended && middle.extended && !ring.extended && !pinky.extended) {
        return {HandGesture::Scissors, base_conf};
    }

    return {HandGesture::Unknown, std::clamp(tracking.presence_score * 0.45f, 0.0f, 0.49f)};
}

} // namespace

GestureResult GestureClassifier::classify(const HandTrackingResult& tracking) {
    std::array<bool, 5> fingers{};
    const auto [raw_gesture, raw_conf] = classify_raw(tracking, fingers);

    history_[history_head_] = FrameSample{raw_gesture, raw_conf};
    history_head_ = (history_head_ + 1) % kSmoothingWindow;
    history_count_ = std::min(history_count_ + 1, kSmoothingWindow);

    std::array<int, 4> counts{};
    std::array<float, 4> conf_sums{};
    for (std::size_t i = 0; i < history_count_; ++i) {
        const auto idx = static_cast<std::size_t>(history_[i].gesture);
        counts[idx] += 1;
        conf_sums[idx] += history_[i].confidence;
    }

    std::size_t best_idx = 0;
    for (std::size_t i = 1; i < counts.size(); ++i) {
        if (counts[i] > counts[best_idx] ||
            (counts[i] == counts[best_idx] && conf_sums[i] > conf_sums[best_idx])) {
            best_idx = i;
        }
    }

    const int best_count = counts[best_idx];
    const float avg_conf = (best_count > 0) ? (conf_sums[best_idx] / static_cast<float>(best_count)) : 0.0f;

    return GestureResult{
        .gesture = static_cast<HandGesture>(best_idx),
        .confidence = avg_conf,
        .is_stable = (history_count_ >= 3 && best_count >= 3 && best_idx != 0),
        .fingers_extended = fingers
    };
}

void GestureClassifier::reset() noexcept {
    history_.fill(FrameSample{});
    history_count_ = 0;
    history_head_ = 0;
}

} // namespace rps::vision
