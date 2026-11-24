#include "audio/AudioEngine.hpp"
#include "core/Logger.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace rps::audio {

namespace {

void write_le16(std::ofstream& out, std::uint16_t v) {
    out.put(static_cast<char>(v & 0xFF));
    out.put(static_cast<char>((v >> 8) & 0xFF));
}

void write_le32(std::ofstream& out, std::uint32_t v) {
    out.put(static_cast<char>(v & 0xFF));
    out.put(static_cast<char>((v >> 8) & 0xFF));
    out.put(static_cast<char>((v >> 16) & 0xFF));
    out.put(static_cast<char>((v >> 24) & 0xFF));
}

void ensure_tone_wav(const std::filesystem::path& path, float freq_start, float freq_end, float duration_sec) {
    if (std::filesystem::exists(path)) {
        return;
    }
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    constexpr std::uint32_t kSampleRate = 44100;
    const auto num_samples = static_cast<std::uint32_t>(kSampleRate * duration_sec);
    const std::uint32_t data_bytes = num_samples * sizeof(std::int16_t);

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return;
    }

    out.write("RIFF", 4);
    write_le32(out, 36 + data_bytes);
    out.write("WAVEfmt ", 8);
    write_le32(out, 16);
    write_le16(out, 1); // PCM
    write_le16(out, 1); // Mono
    write_le32(out, kSampleRate);
    write_le32(out, kSampleRate * sizeof(std::int16_t));
    write_le16(out, sizeof(std::int16_t));
    write_le16(out, 16);
    out.write("data", 4);
    write_le32(out, data_bytes);

    double phase = 0.0;
    constexpr double kTwoPi = 6.283185307179586;
    for (std::uint32_t i = 0; i < num_samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(num_samples);
        const float freq = freq_start + (freq_end - freq_start) * t;
        phase += kTwoPi * freq / static_cast<double>(kSampleRate);
        const float env = std::sin(3.14159265f * t);
        const float sample = static_cast<float>(std::sin(phase)) * env * 0.35f;
        const auto pcm = static_cast<std::int16_t>(std::clamp(sample, -1.0f, 1.0f) * 32767.0f);
        write_le16(out, static_cast<std::uint16_t>(pcm));
    }
}

std::filesystem::path resolve_sfx_path(SoundEffect sfx) {
    const char* filename = "tick.wav";
    switch (sfx) {
        case SoundEffect::Tick:    filename = "tick.wav"; break;
        case SoundEffect::Shoot:   filename = "go.wav"; break;
        case SoundEffect::Win:     filename = "win.wav"; break;
        case SoundEffect::Lose:    filename = "lose.wav"; break;
        case SoundEffect::Draw:    filename = "draw.wav"; break;
        case SoundEffect::Invalid: filename = "invalid.wav"; break;
    }
    const std::filesystem::path asset_path = std::filesystem::path("assets/sounds") / filename;
    if (std::filesystem::exists(asset_path)) {
        return asset_path;
    }
    return std::filesystem::path("data/sfx_cache") / filename;
}

} // namespace

struct AudioEngine::Impl {
    ma_engine engine{};
    bool engine_initialized = false;
};

AudioEngine::AudioEngine() : impl_(std::make_unique<Impl>()) {}

AudioEngine::~AudioEngine() {
    if (impl_->engine_initialized) {
        ma_engine_uninit(&impl_->engine);
        impl_->engine_initialized = false;
    }
}

bool AudioEngine::init() {
    const ma_result res = ma_engine_init(nullptr, &impl_->engine);
    if (res != MA_SUCCESS) {
        core::Logger::warn("miniaudio initialization failed (code {}); SFX disabled.", static_cast<int>(res));
        ready_ = false;
        return false;
    }

    impl_->engine_initialized = true;
    ready_ = true;
    ma_engine_set_volume(&impl_->engine, volume_);

    ensure_tone_wav(resolve_sfx_path(SoundEffect::Tick), 780.0f, 780.0f, 0.08f);
    ensure_tone_wav(resolve_sfx_path(SoundEffect::Shoot), 880.0f, 1320.0f, 0.16f);
    ensure_tone_wav(resolve_sfx_path(SoundEffect::Win), 523.25f, 1046.5f, 0.25f);
    ensure_tone_wav(resolve_sfx_path(SoundEffect::Lose), 392.0f, 220.0f, 0.25f);
    ensure_tone_wav(resolve_sfx_path(SoundEffect::Draw), 587.33f, 587.33f, 0.16f);
    ensure_tone_wav(resolve_sfx_path(SoundEffect::Invalid), 240.0f, 180.0f, 0.18f);

    core::Logger::info("AudioEngine initialized via miniaudio.");
    return true;
}

void AudioEngine::play(SoundEffect sfx) {
    if (!ready_ || muted_) {
        return;
    }
    const std::string path_str = resolve_sfx_path(sfx).string();
    ma_engine_play_sound(&impl_->engine, path_str.c_str(), nullptr);
}

void AudioEngine::set_volume(float volume) noexcept {
    volume_ = std::clamp(volume, 0.0f, 1.0f);
    if (ready_ && !muted_) {
        ma_engine_set_volume(&impl_->engine, volume_);
    }
}

void AudioEngine::set_muted(bool muted) noexcept {
    muted_ = muted;
    if (ready_) {
        ma_engine_set_volume(&impl_->engine, muted_ ? 0.0f : volume_);
    }
}

} // namespace rps::audio
