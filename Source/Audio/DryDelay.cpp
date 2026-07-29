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
    currentDelaySamples_ = -1;
    previousDelaySamples_ = 0;
    targetDelaySamples_ = 0;
    transitionRemaining_ = 0;
}

void DryDelay::reset() noexcept
{
    for (auto& buffer : buffers_)
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    writePosition_ = 0;
    currentDelaySamples_ = -1;
    transitionRemaining_ = 0;
}

void DryDelay::process(float* const* channels, int channelCount, int frames,
                       int delaySamples) noexcept
{
    if (capacity_ == 0 || channels == nullptr || frames <= 0)
        return;

    delaySamples = std::clamp(
        delaySamples, 0, static_cast<int>(capacity_ - 1));
    if (currentDelaySamples_ < 0) {
        currentDelaySamples_ = delaySamples;
        previousDelaySamples_ = delaySamples;
        targetDelaySamples_ = delaySamples;
    } else if (delaySamples != targetDelaySamples_) {
        previousDelaySamples_ = currentDelaySamples_;
        targetDelaySamples_ = delaySamples;
        transitionRemaining_ = kTransitionSamples;
    }

    for (int frame = 0; frame < frames; ++frame) {
        const auto targetReadPosition =
            (writePosition_ + capacity_
             - static_cast<std::size_t>(targetDelaySamples_))
            % capacity_;
        const auto previousReadPosition =
            (writePosition_ + capacity_
             - static_cast<std::size_t>(previousDelaySamples_))
            % capacity_;
        const float transition = transitionRemaining_ > 0
            ? 1.0f
                - static_cast<float>(transitionRemaining_)
                    / static_cast<float>(kTransitionSamples)
            : 1.0f;
        for (int channel = 0;
             channel < channelCount
             && channel < static_cast<int>(buffers_.size()); ++channel) {
            auto& line = buffers_[static_cast<std::size_t>(channel)];
            const float input = channels[channel][frame];
            line[writePosition_] = input;
            const float previous = line[previousReadPosition];
            const float target = line[targetReadPosition];
            channels[channel][frame] =
                previous + (target - previous) * transition;
        }
        if (transitionRemaining_ > 0) {
            --transitionRemaining_;
            if (transitionRemaining_ == 0) {
                currentDelaySamples_ = targetDelaySamples_;
                previousDelaySamples_ = targetDelaySamples_;
            }
        }
        writePosition_ = (writePosition_ + 1) % capacity_;
    }
}

} // namespace dpwim
