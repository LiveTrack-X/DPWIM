#include "DryDelay.h"

#include <algorithm>

namespace dpwim {

void DryDelay::prepare(int channels, int maximumDelaySamples)
{
    capacity_ = static_cast<std::size_t>(
        std::max(maximumDelaySamples + 2, 2));
    buffers_.assign(static_cast<std::size_t>(std::max(channels, 1)),
                    std::vector<float>(capacity_, 0.0f));
    writePosition_ = 0;
}

void DryDelay::reset() noexcept
{
    for (auto& buffer : buffers_)
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    writePosition_ = 0;
}

void DryDelay::process(float* const* channels, int channelCount, int frames,
                       int delaySamples) noexcept
{
    if (capacity_ == 0 || channels == nullptr || frames <= 0)
        return;

    delaySamples = std::clamp(
        delaySamples, 0, static_cast<int>(capacity_ - 1));

    for (int frame = 0; frame < frames; ++frame) {
        const auto readPosition =
            (writePosition_ + capacity_
             - static_cast<std::size_t>(delaySamples)) % capacity_;
        for (int channel = 0;
             channel < channelCount
             && channel < static_cast<int>(buffers_.size()); ++channel) {
            auto& line = buffers_[static_cast<std::size_t>(channel)];
            const float input = channels[channel][frame];
            line[writePosition_] = input;
            channels[channel][frame] = line[readPosition];
        }
        writePosition_ = (writePosition_ + 1) % capacity_;
    }
}

} // namespace dpwim
