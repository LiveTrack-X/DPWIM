#pragma once

#include <cstddef>
#include <vector>

namespace dpwim {

class DryDelay {
public:
    void prepare(int channels, int maximumDelaySamples);
    void reset() noexcept;
    void process(float* const* channels, int channelCount, int frames,
                 int delaySamples) noexcept;

private:
    std::vector<std::vector<float>> buffers_;
    std::size_t writePosition_ = 0;
    std::size_t capacity_ = 0;
    std::size_t validFrames_ = 0;
    int currentDelaySamples_ = -1;
    int previousDelaySamples_ = 0;
    int targetDelaySamples_ = 0;
    int transitionRemaining_ = 0;
    static constexpr int kTransitionSamples = 128;
};

} // namespace dpwim
