#include "Audio/AudioRingBuffer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void replaceCaptureHistory(dpwim::AudioRingBuffer& ring) noexcept
{
    ring.discard(0);
    const std::array<float, 16> freshAudio{
        -1.0f, -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f};
    ring.write(
        freshAudio.data(),
        static_cast<std::uint32_t>(freshAudio.size()), 1);
}

void testMidRenderDiscardInvalidatesWholeBlock()
{
    dpwim::AudioRingBuffer ring;
    ring.configure(16, 1);
    const std::array<float, 12> oldAudio{
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f};
    ring.write(
        oldAudio.data(),
        static_cast<std::uint32_t>(oldAudio.size()), 1);
    ring.setAfterFirstRenderedFrameHookForTesting(
        replaceCaptureHistory);

    std::array<float, 4> output{};
    float* outputChannels[] = {output.data()};
    const auto interrupted =
        ring.renderAdd(outputChannels, 1, 4, 1.0f, 8.0);
    check(
        interrupted.discontinuity
            && interrupted.renderedFrames == 0
            && !interrupted.primed,
        "mid-render discard marks the entire rendered block invalid");
    check(
        output.front() > 0.9f
            && std::all_of(
                output.begin() + 1, output.end(),
                [](float sample) {
                    return std::abs(sample) < 1.0e-6f;
                }),
        "render stops before old and replacement generations can mix");

    output.fill(0.0f);
    const auto recovered =
        ring.renderAdd(outputChannels, 1, 4, 1.0f, 8.0);
    check(
        recovered.renderedFrames == 4
            && recovered.primed
            && !recovered.discontinuity
            && std::all_of(
                output.begin(), output.end(),
                [](float sample) {
                    return std::abs(sample + 1.0f) < 1.0e-6f;
                }),
        "the next block renders only fresh post-discontinuity audio");
}

} // namespace

int main()
{
    testMidRenderDiscardInvalidatesWholeBlock();
    if (failures == 0) {
        std::cout
            << "DPWIM ring-buffer generation tests passed\n";
        return 0;
    }
    std::cerr << failures
              << " DPWIM ring-buffer generation test(s) failed\n";
    return 1;
}
