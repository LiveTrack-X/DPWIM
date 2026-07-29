#pragma once

#include <array>
#include <cstddef>

namespace dpwim {

constexpr std::size_t kSyncSourceCount = 4;

struct SyncSourceTiming {
    bool enabled = false;
    double offsetMs = 0.0;
    double processingLatencyMs = 0.0;
};

struct SyncTimelinePlan {
    double targetLatencyMs = 0.0;
    double syncAdditionMs = 0.0;
    double effectiveLatencyMs = 0.0;
    std::array<double, kSyncSourceCount> sourceLatencyMs{};
};

SyncTimelinePlan calculateSyncTimeline(
    double targetLatencyMs,
    const std::array<SyncSourceTiming, kSyncSourceCount>& sources) noexcept;

int latencySamples(double latencyMs, double sampleRate) noexcept;

} // namespace dpwim
