#include "SyncTimeline.h"

#include <algorithm>
#include <cmath>

namespace dpwim {

SyncTimelinePlan calculateSyncTimeline(
    double targetLatencyMs,
    const std::array<SyncSourceTiming, kSyncSourceCount>& sources) noexcept
{
    SyncTimelinePlan plan;
    plan.targetLatencyMs =
        std::isfinite(targetLatencyMs)
        ? std::max(0.0, targetLatencyMs)
        : 0.0;

    for (const auto& source : sources) {
        if (!source.enabled || !std::isfinite(source.offsetMs))
            continue;
        const auto processingLatency =
            std::isfinite(source.processingLatencyMs)
            ? std::max(0.0, source.processingLatencyMs)
            : 0.0;
        plan.syncAdditionMs =
            std::max(
                plan.syncAdditionMs,
                processingLatency - source.offsetMs);
    }
    plan.syncAdditionMs = std::max(0.0, plan.syncAdditionMs);
    plan.effectiveLatencyMs =
        plan.targetLatencyMs + plan.syncAdditionMs;

    for (std::size_t index = 0; index < sources.size(); ++index) {
        const auto offset = std::isfinite(sources[index].offsetMs)
            ? sources[index].offsetMs
            : 0.0;
        const auto processingLatency =
            std::isfinite(sources[index].processingLatencyMs)
            ? std::max(0.0, sources[index].processingLatencyMs)
            : 0.0;
        plan.sourceLatencyMs[index] = std::max(
            plan.targetLatencyMs,
            plan.effectiveLatencyMs + offset - processingLatency);
    }
    return plan;
}

int latencySamples(double latencyMs, double sampleRate) noexcept
{
    if (!std::isfinite(latencyMs) || !std::isfinite(sampleRate)
        || latencyMs <= 0.0 || sampleRate <= 0.0)
        return 0;
    return static_cast<int>(
        std::llround(sampleRate * latencyMs / 1000.0));
}

} // namespace dpwim
