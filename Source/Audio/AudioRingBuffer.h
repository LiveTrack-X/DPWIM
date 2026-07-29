#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dpwim {

class AudioRingBuffer {
public:
    void configure(std::size_t capacityFrames, std::uint32_t channels);
    void reset() noexcept;
    void reprime(std::uint32_t fadeFrames = 128) noexcept;

    void write(const float* interleaved, std::uint32_t frames,
               std::uint32_t channels, bool silent = false) noexcept;

    struct RenderResult {
        std::uint32_t renderedFrames = 0;
        double ratio = 1.0;
        double fillFrames = 0.0;
        bool primed = false;
        bool discontinuity = false;
    };

    RenderResult renderAdd(float* const* outputs, int outputChannels,
                           std::uint32_t frames, float gain,
                           double targetFillFrames) noexcept;

    std::size_t capacityFrames() const noexcept { return capacityFrames_; }
    std::uint32_t channels() const noexcept { return channels_; }

private:
    std::vector<float> data_;
    std::size_t capacityFrames_ = 0;
    std::uint32_t channels_ = 0;
    std::atomic<std::uint64_t> writeFrame_{0};

    double readFrame_ = 0.0; // audio-consumer thread only
    bool primed_ = false;    // audio-consumer thread only
    double ratio_ = 1.0;     // audio-consumer thread only
    std::uint32_t fadeTotal_ = 0;
    std::uint32_t fadeRemaining_ = 0;
};

} // namespace dpwim
