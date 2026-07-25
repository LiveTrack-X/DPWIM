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
};

} // namespace dpwim
