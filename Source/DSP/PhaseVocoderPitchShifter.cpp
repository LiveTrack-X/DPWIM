#include "PhaseVocoderPitchShifter.h"

#include <algorithm>
#include <cmath>

namespace dpwim {
namespace {

constexpr double pi = 3.14159265358979323846;
constexpr double twoPi = pi * 2.0;

} // namespace

void PhaseVocoderPitchShifter::prepareChannel(Channel& channel)
{
    constexpr auto bins =
        static_cast<std::size_t>(
            PhaseVocoderPitchShifter::kFftSize / 2 + 1);
    channel.inputFifo.assign(
        PhaseVocoderPitchShifter::kFftSize, 0.0f);
    channel.outputFifo.assign(
        PhaseVocoderPitchShifter::kFftSize, 0.0f);
    channel.outputAccum.assign(
        PhaseVocoderPitchShifter::kFftSize * 2, 0.0f);
    channel.lastPhase.assign(bins, 0.0f);
    channel.sumPhase.assign(bins, 0.0f);
    channel.analysisMagnitude.assign(bins, 0.0f);
    channel.analysisBin.assign(bins, 0.0f);
    channel.synthesisMagnitude.assign(bins, 0.0f);
    channel.synthesisBin.assign(bins, 0.0f);
    channel.fftInput.assign(
        PhaseVocoderPitchShifter::kFftSize, {});
    channel.fftOutput.assign(
        PhaseVocoderPitchShifter::kFftSize, {});
    channel.fftInverse.assign(
        PhaseVocoderPitchShifter::kFftSize, {});
    channel.rover =
        PhaseVocoderPitchShifter::kLatencySamples;
}

PhaseVocoderPitchShifter::PhaseVocoderPitchShifter()
{
    window_.resize(kFftSize);
    for (int sample = 0; sample < kFftSize; ++sample) {
        window_[static_cast<std::size_t>(sample)] =
            static_cast<float>(
                0.5 - 0.5 * std::cos(
                                twoPi * static_cast<double>(sample)
                                / static_cast<double>(kFftSize)));
    }
    for (auto& channel : channels_)
        prepareChannel(channel);
}

void PhaseVocoderPitchShifter::prepare(double sampleRate)
{
    juce::ignoreUnused(sampleRate);
    reset();
}

void PhaseVocoderPitchShifter::reset() noexcept
{
    for (auto& channel : channels_) {
        std::fill(
            channel.inputFifo.begin(), channel.inputFifo.end(), 0.0f);
        std::fill(
            channel.outputFifo.begin(), channel.outputFifo.end(), 0.0f);
        std::fill(
            channel.outputAccum.begin(),
            channel.outputAccum.end(), 0.0f);
        std::fill(
            channel.lastPhase.begin(), channel.lastPhase.end(), 0.0f);
        std::fill(
            channel.sumPhase.begin(), channel.sumPhase.end(), 0.0f);
        channel.rover = kLatencySamples;
    }
}

void PhaseVocoderPitchShifter::process(
    float* const* channels, int channelCount,
    int frames, double pitchRatio) noexcept
{
    if (channels == nullptr || frames <= 0)
        return;
    pitchRatio = std::clamp(pitchRatio, 0.45, 2.25);
    for (int channelIndex = 0;
         channelIndex < channelCount
         && channelIndex < static_cast<int>(channels_.size());
         ++channelIndex) {
        auto* samples = channels[channelIndex];
        if (samples == nullptr)
            continue;
        auto& state =
            channels_[static_cast<std::size_t>(channelIndex)];
        for (int frame = 0; frame < frames; ++frame)
            samples[frame] =
                processSample(state, samples[frame], pitchRatio);
    }
}

float PhaseVocoderPitchShifter::processSample(
    Channel& channel, float input, double pitchRatio) noexcept
{
    const int outputIndex = channel.rover - kLatencySamples;
    const float output =
        channel.outputFifo[static_cast<std::size_t>(outputIndex)];
    channel.inputFifo[
        static_cast<std::size_t>(channel.rover)] = input;
    ++channel.rover;
    if (channel.rover >= kFftSize) {
        channel.rover = kLatencySamples;
        processFrame(channel, pitchRatio);
    }
    return output;
}

void PhaseVocoderPitchShifter::processFrame(
    Channel& channel, double pitchRatio) noexcept
{
    constexpr int bins = kFftSize / 2 + 1;
    constexpr double expectedPhase =
        twoPi * static_cast<double>(kHopSize)
        / static_cast<double>(kFftSize);

    for (int sample = 0; sample < kFftSize; ++sample) {
        channel.fftInput[static_cast<std::size_t>(sample)] = {
            channel.inputFifo[static_cast<std::size_t>(sample)]
                * window_[static_cast<std::size_t>(sample)],
            0.0f};
    }
    fft_.perform(
        channel.fftInput.data(), channel.fftOutput.data(), false);

    for (int bin = 0; bin < bins; ++bin) {
        const auto value =
            channel.fftOutput[static_cast<std::size_t>(bin)];
        const float magnitude = std::abs(value);
        const double phase = std::atan2(
            static_cast<double>(value.imag()),
            static_cast<double>(value.real()));
        double delta = phase
            - static_cast<double>(
                channel.lastPhase[static_cast<std::size_t>(bin)]);
        channel.lastPhase[static_cast<std::size_t>(bin)] =
            static_cast<float>(phase);
        delta -= static_cast<double>(bin) * expectedPhase;
        delta = std::remainder(delta, twoPi);
        const auto trueBin =
            static_cast<double>(bin)
            + delta * static_cast<double>(kFftSize)
                / (twoPi * static_cast<double>(kHopSize));
        channel.analysisMagnitude[static_cast<std::size_t>(bin)] =
            magnitude;
        channel.analysisBin[static_cast<std::size_t>(bin)] =
            static_cast<float>(trueBin);
    }

    std::fill(
        channel.synthesisMagnitude.begin(),
        channel.synthesisMagnitude.end(), 0.0f);
    std::fill(
        channel.synthesisBin.begin(),
        channel.synthesisBin.end(), 0.0f);
    for (int bin = 0; bin < bins; ++bin) {
        const int destination = static_cast<int>(
            std::floor(static_cast<double>(bin) * pitchRatio));
        if (destination < 0 || destination >= bins)
            continue;
        channel.synthesisMagnitude[
            static_cast<std::size_t>(destination)]
            += channel.analysisMagnitude[
                static_cast<std::size_t>(bin)];
        channel.synthesisBin[static_cast<std::size_t>(destination)] =
            static_cast<float>(
                channel.analysisBin[static_cast<std::size_t>(bin)]
                * pitchRatio);
    }

    std::fill(
        channel.fftInput.begin(), channel.fftInput.end(),
        juce::dsp::Complex<float>{});
    for (int bin = 0; bin < bins; ++bin) {
        const auto deviation =
            static_cast<double>(
                channel.synthesisBin[static_cast<std::size_t>(bin)])
            - static_cast<double>(bin);
        const auto phaseIncrement =
            static_cast<double>(bin) * expectedPhase
            + deviation * expectedPhase;
        channel.sumPhase[static_cast<std::size_t>(bin)]
            += static_cast<float>(phaseIncrement);
        const auto phase =
            channel.sumPhase[static_cast<std::size_t>(bin)];
        const auto magnitude =
            channel.synthesisMagnitude[static_cast<std::size_t>(bin)];
        channel.fftInput[static_cast<std::size_t>(bin)] = {
            magnitude * std::cos(phase),
            magnitude * std::sin(phase)};
        if (bin > 0 && bin < kFftSize / 2)
            channel.fftInput[
                static_cast<std::size_t>(kFftSize - bin)]
                = std::conj(
                    channel.fftInput[static_cast<std::size_t>(bin)]);
    }

    fft_.perform(
        channel.fftInput.data(), channel.fftInverse.data(), true);
    constexpr float overlapNormalisation = 1.5f;
    for (int sample = 0; sample < kFftSize; ++sample) {
        channel.outputAccum[static_cast<std::size_t>(sample)]
            += channel.fftInverse[static_cast<std::size_t>(sample)].real()
                * window_[static_cast<std::size_t>(sample)]
                / overlapNormalisation;
    }
    std::copy_n(
        channel.outputAccum.begin(), kHopSize,
        channel.outputFifo.begin());
    std::move(
        channel.outputAccum.begin() + kHopSize,
        channel.outputAccum.end(),
        channel.outputAccum.begin());
    std::fill(
        channel.outputAccum.end() - kHopSize,
        channel.outputAccum.end(), 0.0f);
    std::move(
        channel.inputFifo.begin() + kHopSize,
        channel.inputFifo.end(),
        channel.inputFifo.begin());
}

} // namespace dpwim
