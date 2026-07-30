#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dpwim {

class AudioRingBuffer {
public:
    static constexpr double kMinimumRenderRatio = 0.998;
    static constexpr double kMaximumRenderRatio = 1.002;

    static std::uint32_t safeTargetFillFrames(
        std::uint32_t blockFrames) noexcept;

    void configure(std::size_t capacityFrames, std::uint32_t channels);
    void reset() noexcept;
    void discard(std::uint32_t fadeFrames = 128) noexcept;
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

#if defined(DPWIM_AUDIO_RING_BUFFER_TEST_HOOKS)
    using AfterFirstRenderedFrameHook =
        void (*)(AudioRingBuffer&) noexcept;
    void setAfterFirstRenderedFrameHookForTesting(
        AfterFirstRenderedFrameHook hook) noexcept
    {
        afterFirstRenderedFrameHook_ = hook;
    }
#endif

    std::size_t capacityFrames() const noexcept { return capacityFrames_; }
    std::uint32_t channels() const noexcept { return channels_; }

private:
    void resetConsumerState() noexcept;
    void adoptConsumerGeneration(std::uint64_t generation) noexcept;

    std::vector<float> data_;
    std::size_t capacityFrames_ = 0;
    std::uint32_t channels_ = 0;
    std::atomic<std::uint64_t> writeFrame_{0};
    std::atomic<std::uint64_t> generation_{0};
    std::atomic<std::uint64_t> discardFrame_{0};
    std::atomic<std::uint32_t> discardFadeFrames_{0};

    double readFrame_ = 0.0; // audio-consumer thread only
    bool primed_ = false;    // audio-consumer thread only
    double ratio_ = 1.0;     // audio-consumer thread only
    std::uint32_t fadeTotal_ = 0;
    std::uint32_t fadeRemaining_ = 0;
    std::uint64_t consumerGeneration_ = 0;
    std::uint64_t consumerStartFrame_ = 0;
#if defined(DPWIM_AUDIO_RING_BUFFER_TEST_HOOKS)
    AfterFirstRenderedFrameHook afterFirstRenderedFrameHook_ = nullptr;
#endif
};

} // namespace dpwim
