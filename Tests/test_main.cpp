#include "Audio/AudioRingBuffer.h"
#include "Audio/DryDelay.h"

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

} // namespace

int main()
{
    testDryDelay();
    testOffsetDirection();
    testPllBounds();
    testWrapAndOverflowRecovery();
    if (failures == 0) {
        std::cout << "DPWIM core tests passed\n";
        return 0;
    }
    std::cerr << failures << " DPWIM core test(s) failed\n";
    return 1;
}
