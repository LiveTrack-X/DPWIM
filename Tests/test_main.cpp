#include "Audio/AudioRingBuffer.h"
#include "Audio/DryDelay.h"
#include "Audio/SyncTimeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void testDryDelay()
{
    dpwim::DryDelay delay;
    delay.prepare(1, 16);
    std::vector<float> data(8, 0.0f);
    data[0] = 1.0f;
    float* channels[] = {data.data()};
    delay.process(channels, 1, static_cast<int>(data.size()), 3);
    check(std::abs(data[3] - 1.0f) < 1.0e-6f,
          "dry delay places impulse at requested sample");
    check(std::abs(data[0]) < 1.0e-6f,
          "dry delay clears undelayed impulse");
}

void testDryDelayTransition()
{
    dpwim::DryDelay delay;
    delay.prepare(1, 32);
    std::vector<float> warmup(64);
    for (std::size_t index = 0; index < warmup.size(); ++index)
        warmup[index] = static_cast<float>(index);
    float* warmupChannels[] = {warmup.data()};
    delay.process(warmupChannels, 1,
                  static_cast<int>(warmup.size()), 2);

    std::vector<float> changed(128);
    for (std::size_t index = 0; index < changed.size(); ++index)
        changed[index] = 64.0f + static_cast<float>(index);
    float* changedChannels[] = {changed.data()};
    delay.process(changedChannels, 1,
                  static_cast<int>(changed.size()), 10);

    check(std::abs(changed.front() - 62.0f) < 1.0e-5f,
          "dry delay change begins from the previous delay tap");
    float largestStep = 0.0f;
    for (std::size_t index = 1; index < changed.size(); ++index)
        largestStep = std::max(
            largestStep,
            std::abs(changed[index] - changed[index - 1]));
    check(largestStep < 2.0f,
          "dry delay change crossfades without a hard timeline jump");
    check(std::abs(changed.back() - 181.0625f) < 0.1f,
          "dry delay transition reaches the new delay tap");
}

void testDryDelayResetInvalidatesHistory()
{
    dpwim::DryDelay delay;
    delay.prepare(1, 16);
    std::vector<float> priming(4, 0.0f);
    priming[2] = 1.0f;
    float* primingChannels[] = {priming.data()};
    delay.process(
        primingChannels, 1, static_cast<int>(priming.size()), 3);

    delay.reset();
    std::vector<float> afterReset(8, 0.0f);
    float* resetChannels[] = {afterReset.data()};
    delay.process(
        resetChannels, 1, static_cast<int>(afterReset.size()), 3);
    check(std::all_of(
              afterReset.begin(), afterReset.end(),
              [](float sample) { return std::abs(sample) < 1.0e-6f; }),
          "dry delay reset invalidates buffered history");
}

float renderFirstAtTarget(double target)
{
    dpwim::AudioRingBuffer ring;
    ring.configure(2048, 2);
    std::vector<float> input(2000);
    for (int frame = 0; frame < 1000; ++frame) {
        input[static_cast<std::size_t>(frame) * 2] =
            static_cast<float>(frame);
        input[static_cast<std::size_t>(frame) * 2 + 1] =
            static_cast<float>(frame);
    }
    ring.write(input.data(), 1000, 2, false);
    float left[2]{};
    float right[2]{};
    float* outputs[] = {left, right};
    ring.renderAdd(outputs, 2, 1, 1.0f, target);
    return left[0];
}

void testOffsetDirection()
{
    const float base = renderFirstAtTarget(100.0);
    const float delayed = renderFirstAtTarget(120.0);
    const float advanced = renderFirstAtTarget(80.0);
    check(delayed < base, "positive source offset reads older audio");
    check(advanced > base, "negative source offset reads newer audio");
}

void testSyncTimelineRebase()
{
    std::array<dpwim::SyncSourceTiming, dpwim::kSyncSourceCount>
        sources{};

    const auto baseline =
        dpwim::calculateSyncTimeline(10.0, sources);
    check(std::abs(baseline.effectiveLatencyMs - 10.0) < 1.0e-9,
          "zero offsets preserve the target latency");
    check(std::abs(baseline.syncAdditionMs) < 1.0e-9,
          "zero offsets add no sync latency");

    sources[0] = {true, -25.0, 0.0};
    sources[1] = {true, 10.0, 0.0};
    const auto rebased =
        dpwim::calculateSyncTimeline(10.0, sources);
    check(std::abs(rebased.syncAdditionMs - 25.0) < 1.0e-9,
          "most negative enabled offset determines rebase");
    check(std::abs(rebased.effectiveLatencyMs - 35.0) < 1.0e-9,
          "effective latency includes the minimal rebase");
    check(std::abs(rebased.sourceLatencyMs[0] - 10.0) < 1.0e-9,
          "advanced source retains the target safety buffer");
    check(std::abs(rebased.sourceLatencyMs[1] - 45.0) < 1.0e-9,
          "positive offset delays only that source");
    check(dpwim::latencySamples(35.0, 48000.0) == 1680,
          "effective milliseconds convert to host samples");

    sources[2] = {true, -40.0, 0.0};
    const auto multiple =
        dpwim::calculateSyncTimeline(10.0, sources);
    check(std::abs(multiple.effectiveLatencyMs - 50.0) < 1.0e-9,
          "most advanced source wins across multiple sources");

    sources[2].enabled = false;
    const auto disabled =
        dpwim::calculateSyncTimeline(10.0, sources);
    check(std::abs(disabled.effectiveLatencyMs - 35.0) < 1.0e-9,
          "disabled source does not increase output latency");

    sources[0] = {true, 20.0, 0.0};
    sources[1] = {true, 5.0, 0.0};
    const auto positiveOnly =
        dpwim::calculateSyncTimeline(10.0, sources);
    check(std::abs(positiveOnly.effectiveLatencyMs - 10.0) < 1.0e-9,
          "positive offsets do not delay dry or other sources");

    sources[0] = {true, 0.0, 32.0};
    sources[1] = {true, 10.0, 0.0};
    const auto processing =
        dpwim::calculateSyncTimeline(10.0, sources);
    check(std::abs(processing.effectiveLatencyMs - 42.0) < 1.0e-9,
          "source processing latency is added to common output latency");
    check(std::abs(processing.sourceLatencyMs[0] - 10.0) < 1.0e-9,
          "capture safety remains before delayed source processing");
    check(std::abs(
              processing.sourceLatencyMs[0] + 32.0
              - processing.effectiveLatencyMs)
              < 1.0e-9,
          "capture and processing delay equal the common timeline");
}

void testPllBounds()
{
    dpwim::AudioRingBuffer ring;
    ring.configure(4096, 2);
    std::vector<float> input(6000, 0.25f);
    ring.write(input.data(), 3000, 2, false);
    float left[32]{};
    float right[32]{};
    float* outputs[] = {left, right};
    const auto result =
        ring.renderAdd(outputs, 2, 32, 1.0f, 128.0);
    check(result.ratio >= 0.998 && result.ratio <= 1.002,
          "PLL ratio remains bounded");
    check(result.renderedFrames == 32,
          "primed FIFO renders the requested block");
}

void testReprimeFade()
{
    dpwim::AudioRingBuffer ring;
    ring.configure(128, 1);
    std::vector<float> input(104, 1.0f);
    ring.write(input.data(), 100, 1, false);

    float initial[4]{};
    float* initialOutputs[] = {initial};
    ring.renderAdd(initialOutputs, 1, 4, 1.0f, 16.0);
    check(std::abs(initial[0] - 1.0f) < 1.0e-6f,
          "primed FIFO renders at full gain");

    ring.reprime(4);
    ring.write(input.data() + 100, 4, 1, false);
    float faded[4]{};
    float* fadedOutputs[] = {faded};
    const auto result =
        ring.renderAdd(fadedOutputs, 1, 4, 1.0f, 16.0);
    check(result.renderedFrames == 4 && result.primed,
          "offset change re-primes against the new target");
    check(std::abs(faded[0] - 0.25f) < 1.0e-6f
              && std::abs(faded[1] - 0.5f) < 1.0e-6f
              && std::abs(faded[2] - 0.75f) < 1.0e-6f
              && std::abs(faded[3] - 1.0f) < 1.0e-6f,
          "re-prime fades captured audio back in");
}

void testWrapAndOverflowRecovery()
{
    dpwim::AudioRingBuffer ring;
    ring.configure(16, 2);
    std::vector<float> first(24);
    for (int frame = 0; frame < 12; ++frame) {
        first[static_cast<std::size_t>(frame) * 2] =
            static_cast<float>(frame);
        first[static_cast<std::size_t>(frame) * 2 + 1] =
            static_cast<float>(frame);
    }
    ring.write(first.data(), 12, 2, false);

    float left[8]{};
    float right[8]{};
    float* outputs[] = {left, right};
    const auto primed = ring.renderAdd(outputs, 2, 4, 1.0f, 8.0);
    check(primed.renderedFrames == 4 && primed.primed,
          "FIFO primes and renders before wrap");

    std::vector<float> overflow(64);
    for (int frame = 0; frame < 32; ++frame) {
        overflow[static_cast<std::size_t>(frame) * 2] =
            100.0f + static_cast<float>(frame);
        overflow[static_cast<std::size_t>(frame) * 2 + 1] =
            100.0f + static_cast<float>(frame);
    }
    ring.write(overflow.data(), 32, 2, false);
    std::fill(std::begin(left), std::end(left), 0.0f);
    std::fill(std::begin(right), std::end(right), 0.0f);
    const auto recovered =
        ring.renderAdd(outputs, 2, 4, 1.0f, 8.0);
    check(recovered.discontinuity,
          "FIFO reports overwritten unread audio");
    check(recovered.renderedFrames == 4 && recovered.primed,
          "FIFO re-primes from the newest bounded window");
    check(std::isfinite(left[0]) && left[0] >= 100.0f,
          "overflow recovery reads valid recent audio");
}

void testDiscardInvalidatesHistory()
{
    dpwim::AudioRingBuffer ring;
    ring.configure(32, 1);
    const std::array<float, 8> oldAudio{
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f};
    ring.write(
        oldAudio.data(),
        static_cast<std::uint32_t>(oldAudio.size()), 1);

    std::array<float, 4> output{};
    float* outputChannels[] = {output.data()};
    ring.renderAdd(outputChannels, 1, 4, 1.0f, 4.0);
    ring.discard();
    output.fill(0.0f);
    const auto afterDiscard =
        ring.renderAdd(outputChannels, 1, 4, 1.0f, 4.0);
    check(
        afterDiscard.renderedFrames == 0
            && std::all_of(
                output.begin(), output.end(),
                [](float sample) {
                    return std::abs(sample) < 1.0e-6f;
                }),
        "FIFO discard invalidates old audio on the consumer thread");
}

} // namespace

int main()
{
    testDryDelay();
    testDryDelayTransition();
    testDryDelayResetInvalidatesHistory();
    testOffsetDirection();
    testSyncTimelineRebase();
    testPllBounds();
    testReprimeFade();
    testWrapAndOverflowRecovery();
    testDiscardInvalidatesHistory();
    if (failures == 0) {
        std::cout << "DPWIM core tests passed\n";
        return 0;
    }
    std::cerr << failures << " DPWIM core test(s) failed\n";
    return 1;
}
