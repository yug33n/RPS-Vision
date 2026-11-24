#pragma once

#include <memory>

namespace rps::audio {

enum class SoundEffect {
    Tick,
    Shoot,
    Win,
    Lose,
    Draw,
    Invalid
};

/**
 * @brief Low-latency miniaudio sound effects engine with procedural WAV fallback.
 */
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    /**
     * @brief Initializes miniaudio playback engine and prepares SFX assets.
     */
    bool init();

    /**
     * @brief Plays the specified sound effect asynchronously.
     */
    void play(SoundEffect sfx);

    void set_volume(float volume) noexcept;
    [[nodiscard]] float volume() const noexcept { return volume_; }

    void set_muted(bool muted) noexcept;
    [[nodiscard]] bool is_muted() const noexcept { return muted_; }
    [[nodiscard]] bool is_ready() const noexcept { return ready_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    float volume_ = 0.75f;
    bool muted_ = false;
    bool ready_ = false;
};

} // namespace rps::audio
