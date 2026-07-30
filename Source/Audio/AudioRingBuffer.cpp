#include "AudioRingBuffer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace dpwim {

std::uint32_t AudioRingBuffer::safeTargetFillFrames(
    std::uint32_t blockFrames) noexcept
{
    if (blockFrames == 0)
        return 0;
    const auto safeFrames =
        std::ceil(
            static_cast<double>(blockFrames)
            * kMaximumRenderRatio);
    return static_cast<std::uint32_t>(
        std::min(
            safeFrames,
            static_cast<double>(
                std::numeric_limits<std::uint32_t>::max())));
}

void AudioRingBuffer::configure(std::size_t capacityFrames,
                                std::uint32_t channels)
{
    capacityFrames_ = std::max<std::size_t>(capacityFrames, 8);
    channels_ = std::max<std::uint32_t>(channels, 1);
    data_.assign(capacityFrames_ * channels_, 0.0f);
    generation_.store(0, std::memory_order_relaxed);
    discardFrame_.store(0, std::memory_order_relaxed);
    discardFadeFrames_.store(0, std::memory_order_relaxed);
    consumerGeneration_ = 0;
    reset();
}

void AudioRingBuffer::reset() noexcept
{
    writeFrame_.store(0, std::memory_order_release);
    discardFrame_.store(0, std::memory_order_release);
    discardFadeFrames_.store(0, std::memory_order_release);
    resetConsumerState();
    consumerGeneration_ =
        generation_.load(std::memory_order_acquire);
}

void AudioRingBuffer::discard(std::uint32_t fadeFrames) noexcept
{
    discardFadeFrames_.store(fadeFrames, std::memory_order_release);
    discardFrame_.store(
        writeFrame_.load(std::memory_order_acquire),
        std::memory_order_release);
    generation_.fetch_add(1, std::memory_order_acq_rel);
}

void AudioRingBuffer::resetConsumerState() noexcept
{
    readFrame_ = 0.0;
    primed_ = false;
    ratio_ = 1.0;
    fadeTotal_ = 0;
    fadeRemaining_ = 0;
    consumerStartFrame_ = 0;
}

void AudioRingBuffer::reprime(std::uint32_t fadeFrames) noexcept
{
    primed_ = false;
    ratio_ = 1.0;
    fadeTotal_ = fadeFrames;
    fadeRemaining_ = fadeFrames;
}

void AudioRingBuffer::adoptConsumerGeneration(
    std::uint64_t generation) noexcept
{
    resetConsumerState();
    fadeTotal_ =
        discardFadeFrames_.load(std::memory_order_acquire);
    fadeRemaining_ = fadeTotal_;
    consumerStartFrame_ =
        discardFrame_.load(std::memory_order_acquire);
    consumerGeneration_ = generation;
}

void AudioRingBuffer::write(const float* interleaved, std::uint32_t frames,
                            std::uint32_t sourceChannels, bool silent) noexcept
{
    if (capacityFrames_ == 0 || channels_ == 0 || frames == 0)
        return;

    const auto start = writeFrame_.load(std::memory_order_relaxed);
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const auto destinationFrame =
            static_cast<std::size_t>((start + frame) % capacityFrames_);
        for (std::uint32_t channel = 0; channel < channels_; ++channel) {
            float value = 0.0f;
            if (!silent && interleaved != nullptr && sourceChannels > 0) {
                const auto sourceChannel =
                    std::min<std::uint32_t>(channel, sourceChannels - 1);
                value = interleaved[
                    static_cast<std::size_t>(frame) * sourceChannels
                    + sourceChannel];
            }
            data_[destinationFrame * channels_ + channel] = value;
        }
    }
    writeFrame_.store(start + frames, std::memory_order_release);
}

AudioRingBuffer::RenderResult AudioRingBuffer::renderAdd(
    float* const* outputs, int outputChannels, std::uint32_t frames,
    float gain, double targetFillFrames) noexcept
{
    RenderResult result;
    if (capacityFrames_ == 0 || channels_ == 0 || outputs == nullptr
        || outputChannels <= 0 || frames == 0)
        return result;

    const auto generation =
        generation_.load(std::memory_order_acquire);
    if (generation != consumerGeneration_) {
        adoptConsumerGeneration(generation);
        result.discontinuity = true;
    }

    const auto write = writeFrame_.load(std::memory_order_acquire);
    targetFillFrames = std::clamp(targetFillFrames, 2.0,
                                  static_cast<double>(capacityFrames_ - 4));

    if (primed_) {
        const auto oldestSafe =
            write > capacityFrames_ ? write - capacityFrames_ : 0;
        if (readFrame_ < static_cast<double>(oldestSafe)) {
            reprime();
            result.discontinuity = true;
        }
    }

    if (!primed_) {
        const auto availableSinceDiscard =
            write >= consumerStartFrame_
            ? write - consumerStartFrame_
            : 0;
        if (static_cast<double>(availableSinceDiscard)
            < targetFillFrames + 2.0)
            return result;
        readFrame_ = static_cast<double>(write) - targetFillFrames;
        primed_ = true;
    }

    const double fill = static_cast<double>(write) - readFrame_;
    const double normalizedError =
        (fill - targetFillFrames) / std::max(targetFillFrames, 64.0);
    const double wantedRatio =
        std::clamp(
            1.0 + normalizedError * 0.002,
            kMinimumRenderRatio, kMaximumRenderRatio);
    ratio_ += (wantedRatio - ratio_) * 0.02;

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const auto currentGeneration =
            generation_.load(std::memory_order_acquire);
        if (currentGeneration != consumerGeneration_) {
            adoptConsumerGeneration(currentGeneration);
            result.renderedFrames = 0;
            result.discontinuity = true;
            break;
        }

        const auto availableWrite =
            writeFrame_.load(std::memory_order_acquire);
        const auto base = static_cast<std::uint64_t>(std::floor(readFrame_));
        if (base + 1 >= availableWrite) {
            reprime();
            consumerStartFrame_ = availableWrite;
            result.discontinuity = true;
            break;
        }

        const auto next = base + 1;
        const float fraction =
            static_cast<float>(readFrame_ - static_cast<double>(base));
        const auto baseIndex =
            static_cast<std::size_t>(base % capacityFrames_) * channels_;
        const auto nextIndex =
            static_cast<std::size_t>(next % capacityFrames_) * channels_;
        float transitionGain = 1.0f;
        if (fadeRemaining_ > 0 && fadeTotal_ > 0) {
            transitionGain =
                static_cast<float>(fadeTotal_ - fadeRemaining_ + 1)
                / static_cast<float>(fadeTotal_);
            --fadeRemaining_;
        }

        if (outputChannels == 1 && channels_ >= 2) {
            const float left =
                data_[baseIndex] + (data_[nextIndex] - data_[baseIndex]) * fraction;
            const float right =
                data_[baseIndex + 1]
                + (data_[nextIndex + 1] - data_[baseIndex + 1]) * fraction;
            outputs[0][frame] +=
                0.5f * (left + right) * gain * transitionGain;
        } else {
            for (int channel = 0; channel < outputChannels; ++channel) {
                const auto sourceChannel =
                    std::min<std::uint32_t>(
                        static_cast<std::uint32_t>(channel), channels_ - 1);
                const float a = data_[baseIndex + sourceChannel];
                const float b = data_[nextIndex + sourceChannel];
                outputs[channel][frame] +=
                    (a + (b - a) * fraction)
                    * gain * transitionGain;
            }
        }

        readFrame_ += ratio_;
        ++result.renderedFrames;
#if defined(DPWIM_AUDIO_RING_BUFFER_TEST_HOOKS)
        if (result.renderedFrames == 1
            && afterFirstRenderedFrameHook_ != nullptr) {
            const auto hook = afterFirstRenderedFrameHook_;
            afterFirstRenderedFrameHook_ = nullptr;
            hook(*this);
        }
#endif
    }

    const auto finalGeneration =
        generation_.load(std::memory_order_acquire);
    if (finalGeneration != consumerGeneration_) {
        adoptConsumerGeneration(finalGeneration);
        result.renderedFrames = 0;
        result.discontinuity = true;
    }

    result.ratio = ratio_;
    result.fillFrames = fill;
    result.primed = primed_;
    return result;
}

} // namespace dpwim
