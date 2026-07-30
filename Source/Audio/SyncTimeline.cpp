#include "SyncTimeline.h"

#include <algorithm>
#include <cmath>

namespace dpwim {

SyncTimelinePlan calculateSyncTimeline(
    double targetLatencyMs,
    const std::array<SyncSourceTiming, kSyncSourceCount>& sources,
    double minimumEnabledSourceLatencyMs) noexcept
{
    SyncTimelinePlan plan;
    plan.targetLatencyMs =
        std::isfinite(targetLatencyMs)
        ? std::max(0.0, targetLatencyMs)
        : 0.0;

    bool hasEnabledSource = false;
    double processingAndOffsetAdditionMs = 0.0;
    for (const auto& source : sources) {
        if (!source.enabled)
            continue;
        hasEnabledSource = true;
        const auto offset =
            std::isfinite(source.offsetMs) ? source.offsetMs : 0.0;
        const auto processingLatency =
            std::isfinite(source.processingLatencyMs)
            ? std::max(0.0, source.processingLatencyMs)
            : 0.0;
        processingAndOffsetAdditionMs =
            std::max(
                processingAndOffsetAdditionMs,
                processingLatency - offset);
    }

    const auto sourceSafetyFloor =
        std::isfinite(minimumEnabledSourceLatencyMs)
        ? std::max(0.0, minimumEnabledSourceLatencyMs)
        : 0.0;
    const auto effectiveBaseLatencyMs =
        hasEnabledSource
        ? std::max(plan.targetLatencyMs, sourceSafetyFloor)
        : plan.targetLatencyMs;
    processingAndOffsetAdditionMs =
        std::max(0.0, processingAndOffsetAdditionMs);
    plan.effectiveLatencyMs =
        effectiveBaseLatencyMs + processingAndOffsetAdditionMs;
    plan.syncAdditionMs =
        plan.effectiveLatencyMs - plan.targetLatencyMs;

    for (std::size_t index = 0; index < sources.size(); ++index) {
        const auto offset = std::isfinite(sources[index].offsetMs)
            ? sources[index].offsetMs
            : 0.0;
        const auto processingLatency =
            std::isfinite(sources[index].processingLatencyMs)
            ? std::max(0.0, sources[index].processingLatencyMs)
            : 0.0;
        plan.sourceLatencyMs[index] = std::max(
            effectiveBaseLatencyMs,
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
