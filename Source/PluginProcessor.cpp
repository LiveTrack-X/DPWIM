#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <windows.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr float kMaximumBaseLatencyMs = 250.0f;
constexpr float kMaximumNegativeOffsetMs = 200.0f;
constexpr int kMinimumHostBlockSafetyFrames = 512;
constexpr int kRealtimeBlockReserveFrames = 8192;

float dbToGain(float db)
{
    return db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain(db);
}

} // namespace

DPWIMAudioProcessor::DPWIMAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input",
                                    juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output",
                                     juce::AudioChannelSet::stereo(), true))
    , apvts_(*this, nullptr, "DPWIM", createParameterLayout())
{
    targetLatencyParam_ = apvts_.getRawParameterValue("targetLatency");
    dryEnabledParam_ = apvts_.getRawParameterValue("dryEnabled");
    dryGainParam_ = apvts_.getRawParameterValue("dryGain");
    bypassParam_ = apvts_.getRawParameterValue("bypass");
    for (int index = 0; index < kSourceSlots; ++index) {
        slots_[static_cast<std::size_t>(index)] =
            std::make_unique<SourceSlot>();
        sourceGainParams_[static_cast<std::size_t>(index)] =
            apvts_.getRawParameterValue(gainParameter(index));
        sourceOffsetParams_[static_cast<std::size_t>(index)] =
            apvts_.getRawParameterValue(offsetParameter(index));
        sourceTransposeParams_[static_cast<std::size_t>(index)] =
            apvts_.getRawParameterValue(transposeParameter(index));
        sourceFinePitchParams_[static_cast<std::size_t>(index)] =
            apvts_.getRawParameterValue(finePitchParameter(index));
    }
    apvts_.addParameterListener("targetLatency", this);
    apvts_.addParameterListener("bypass", this);
    for (int index = 0; index < kSourceSlots; ++index) {
        apvts_.addParameterListener(offsetParameter(index), this);
        apvts_.addParameterListener(transposeParameter(index), this);
        apvts_.addParameterListener(finePitchParameter(index), this);
    }
    startTimerHz(60);
}

DPWIMAudioProcessor::~DPWIMAudioProcessor()
{
    stopTimer();
    apvts_.removeParameterListener("targetLatency", this);
    apvts_.removeParameterListener("bypass", this);
    for (int index = 0; index < kSourceSlots; ++index) {
        apvts_.removeParameterListener(offsetParameter(index), this);
        apvts_.removeParameterListener(transposeParameter(index), this);
        apvts_.removeParameterListener(finePitchParameter(index), this);
    }
    cancelPendingUpdate();
    stopAllSlots();
}

juce::String DPWIMAudioProcessor::gainParameter(int slot)
{
    return "sourceGain" + juce::String(slot);
}

juce::String DPWIMAudioProcessor::offsetParameter(int slot)
{
    return "sourceOffset" + juce::String(slot);
}

juce::String DPWIMAudioProcessor::transposeParameter(int slot)
{
    return "sourceTranspose" + juce::String(slot);
}

juce::String DPWIMAudioProcessor::finePitchParameter(int slot)
{
    return "sourceFinePitch" + juce::String(slot);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DPWIMAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"targetLatency", 1}, "Base Latency",
        juce::NormalisableRange<float>(
            10.0f, kMaximumBaseLatencyMs, 0.1f),
        10.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"dryEnabled", 1}, "Dry Input Enabled", true));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dryGain", 1}, "Dry Gain",
        juce::NormalisableRange<float>(-60.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    for (int slot = 0; slot < kSourceSlots; ++slot) {
        parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{gainParameter(slot), 1},
            "Source " + juce::String(slot + 1) + " Gain",
            juce::NormalisableRange<float>(-60.0f, 12.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));
        parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{offsetParameter(slot), 1},
            "Source " + juce::String(slot + 1) + " Offset",
            juce::NormalisableRange<float>(-200.0f, 200.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms")));
    }
    for (int slot = 0; slot < kSourceSlots; ++slot) {
        parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{transposeParameter(slot), 1},
            "Source " + juce::String(slot + 1) + " Transpose",
            juce::NormalisableRange<float>(-12.0f, 12.0f, 1.0f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel("st")));
        parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{finePitchParameter(slot), 1},
            "Source " + juce::String(slot + 1) + " Fine Pitch",
            juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel("ct")));
    }
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"bypass", 1}, "DPWIM Bypass", false));
    return {parameters.begin(), parameters.end()};
}

bool DPWIMAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    const bool supported =
        input == juce::AudioChannelSet::mono()
        || input == juce::AudioChannelSet::stereo();
    return supported && output == input;
}

void DPWIMAudioProcessor::prepareToPlay(double sampleRate,
                                        int samplesPerBlock)
{
    stopAllSlots();
    sampleRate_.store(sampleRate, std::memory_order_release);
    const auto preparedBlockFrames = std::max(samplesPerBlock, 1);
    const auto initialSafetyBlockFrames =
        std::max(
            preparedBlockFrames, kMinimumHostBlockSafetyFrames);
    const auto realtimeBlockCapacity =
        std::max(preparedBlockFrames, kRealtimeBlockReserveFrames);
    const auto initialSafetyFrames =
        static_cast<int>(
            dpwim::AudioRingBuffer::safeTargetFillFrames(
                static_cast<std::uint32_t>(
                    initialSafetyBlockFrames)));
    blockSafetyFrames_.store(
        initialSafetyFrames,
        std::memory_order_release);
    observedBlockSafetyFrames_.store(
        initialSafetyFrames,
        std::memory_order_release);
    pendingCommittedBlockSafetyFrames_.store(
        0, std::memory_order_release);
    realtimeBlockCapacityFrames_.store(
        realtimeBlockCapacity, std::memory_order_release);
    const auto safeSampleRate = std::max(1.0, sampleRate);
    const auto maximumBaseDelaySamples =
        static_cast<int>(std::ceil(
            safeSampleRate
            * static_cast<double>(kMaximumBaseLatencyMs)
            / 1000.0));
    const auto maximumNegativeOffsetSamples =
        static_cast<int>(std::ceil(
            safeSampleRate
            * static_cast<double>(kMaximumNegativeOffsetMs)
            / 1000.0));
    const auto maximumTimelineDelaySamples =
        std::max(
            maximumBaseDelaySamples,
            static_cast<int>(
                dpwim::AudioRingBuffer::safeTargetFillFrames(
                    static_cast<std::uint32_t>(
                        realtimeBlockCapacity))))
        + maximumNegativeOffsetSamples
        + dpwim::PhaseVocoderPitchShifter::kLatencySamples;
    dryDelay_.prepare(
        getTotalNumOutputChannels(), maximumTimelineDelaySamples);
    for (auto& slot : slots_) {
        slot->ring.configure(
            static_cast<std::size_t>(std::ceil(sampleRate * 5.0)), 2);
        slot->pitchShifter.prepare(sampleRate);
        slot->scratch.setSize(
            2, realtimeBlockCapacity,
            false, true, false);
        slot->lastPitchActive = false;
        slot->lastRequestedFillFrames = -1.0;
        for (auto& meter : slot->meterPeaks)
            meter.store(0.0f, std::memory_order_relaxed);
    }
    for (auto& meter : dryMeterPeaks_)
        meter.store(0.0f, std::memory_order_relaxed);
    for (auto& meter : mainOutputMeterPeaks_)
        meter.store(0.0f, std::memory_order_relaxed);
    audioPathMode_ = AudioPathMode::Uninitialised;
    lastAudioBlockTick_ = 0;
    prepared_.store(true, std::memory_order_release);
    updateLatencyReport();
    for (int slot = 0; slot < kSourceSlots; ++slot)
        startSlot(slot);
}

void DPWIMAudioProcessor::releaseResources()
{
    prepared_.store(false, std::memory_order_release);
    stopAllSlots();
    dryDelay_.reset();
    audioPathMode_ = AudioPathMode::Uninitialised;
    lastAudioBlockTick_ = 0;
    pendingCommittedBlockSafetyFrames_.store(
        0, std::memory_order_release);
    for (auto& meter : dryMeterPeaks_)
        meter.store(0.0f, std::memory_order_relaxed);
    for (auto& meter : mainOutputMeterPeaks_)
        meter.store(0.0f, std::memory_order_relaxed);
    for (auto& slot : slots_)
        for (auto& meter : slot->meterPeaks)
            meter.store(0.0f, std::memory_order_relaxed);
}

void DPWIMAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer&)
{
    processAudioBlock(buffer, false);
}

void DPWIMAudioProcessor::processBlockBypassed(
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processAudioBlock(buffer, true);
}

void DPWIMAudioProcessor::processAudioBlock(
    juce::AudioBuffer<float>& buffer, bool forceBypass)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels = buffer.getNumChannels();
    const int frames = buffer.getNumSamples();
    if (channels <= 0 || frames <= 0)
        return;
    applyPendingBlockSafety();
    const bool sourceBlockSafetyReady =
        observeHostBlockSize(frames);
    const auto committedBlockSafetyFrames =
        blockSafetyFrames_.load(std::memory_order_acquire);

    const double rate = sampleRate_.load(std::memory_order_acquire);
    const bool internalBypassed =
        bypassParam_->load(std::memory_order_relaxed) >= 0.5f;
    const auto mode = internalBypassed
        ? AudioPathMode::InternalBypass
        : forceBypass
            ? AudioPathMode::HostBypass
            : AudioPathMode::Normal;
    prepareAudioPath(mode, frames, rate);

    constexpr float releaseDbPerSecond = 18.0f;
    const auto release = std::pow(
        10.0f,
        -releaseDbPerSecond
            * static_cast<float>(frames)
            / static_cast<float>(std::max(1.0, rate))
            / 20.0f);
    if (mode != AudioPathMode::Normal) {
        if (mode == AudioPathMode::HostBypass) {
            const auto timeline =
                currentTimeline(committedBlockSafetyFrames);
            const int targetFrames = dpwim::latencySamples(
                timeline.effectiveLatencyMs, rate);
            std::array<float*, 2> outputs{
                buffer.getWritePointer(0),
                channels > 1 ? buffer.getWritePointer(1)
                             : buffer.getWritePointer(0)};
            dryDelay_.process(
                outputs.data(), channels, frames, targetFrames);
        }
        for (int meterChannel = 0; meterChannel < 2; ++meterChannel) {
            const auto channel = std::min(meterChannel, channels - 1);
            const auto magnitude =
                buffer.getMagnitude(channel, 0, frames);
            updateMeterPeak(
                dryMeterPeaks_[static_cast<std::size_t>(meterChannel)],
                magnitude, release);
            updateMeterPeak(
                mainOutputMeterPeaks_[
                    static_cast<std::size_t>(meterChannel)],
                magnitude, release);
        }
        for (auto& slot : slots_)
            for (auto& meter : slot->meterPeaks)
                updateMeterPeak(meter, 0.0f, release);
        return;
    }

    const bool dryEnabled =
        dryEnabledParam_->load(std::memory_order_relaxed) >= 0.5f;
    const auto timeline =
        currentTimeline(committedBlockSafetyFrames);
    const int targetFrames = dpwim::latencySamples(
        timeline.effectiveLatencyMs, rate);

    std::array<float*, 2> outputs{
        buffer.getWritePointer(0),
        channels > 1 ? buffer.getWritePointer(1)
                     : buffer.getWritePointer(0)};
    dryDelay_.process(outputs.data(), channels, frames, targetFrames);
    if (dryEnabled) {
        const float dryDb = dryGainParam_->load(std::memory_order_relaxed);
        buffer.applyGain(dbToGain(dryDb));
    } else {
        buffer.clear();
    }
    for (int meterChannel = 0; meterChannel < 2; ++meterChannel) {
        const auto channel = std::min(meterChannel, channels - 1);
        updateMeterPeak(
            dryMeterPeaks_[static_cast<std::size_t>(meterChannel)],
            buffer.getMagnitude(channel, 0, frames), release);
    }

    for (int index = 0; index < kSourceSlots; ++index) {
        auto& slot = *slots_[static_cast<std::size_t>(index)];
        if (!slot.enabled.load(std::memory_order_acquire)
            || slot.mode.load(std::memory_order_acquire)
                == static_cast<int>(SourceMode::Off)) {
            for (auto& meter : slot.meterPeaks)
                updateMeterPeak(meter, 0.0f, release);
            continue;
        }
        if (!sourceBlockSafetyReady) {
            slot.ring.reprime();
            slot.pitchShifter.reset();
            slot.lastRequestedFillFrames = -1.0;
            slot.fillFrames.store(0.0, std::memory_order_relaxed);
            slot.ratio.store(1.0, std::memory_order_relaxed);
            for (auto& meter : slot.meterPeaks)
                updateMeterPeak(meter, 0.0f, release);
            continue;
        }

        const float gainDb =
            sourceGainParams_[static_cast<std::size_t>(index)]
                ->load(std::memory_order_relaxed);
        const auto semitones =
            sourceTransposeParams_[static_cast<std::size_t>(index)]
                ->load(std::memory_order_relaxed)
            + sourceFinePitchParams_[static_cast<std::size_t>(index)]
                    ->load(std::memory_order_relaxed)
                / 100.0f;
        const bool pitchActive = std::abs(semitones) >= 0.005f;
        const double requestedFill =
            rate
            * timeline.sourceLatencyMs[static_cast<std::size_t>(index)]
            / 1000.0;
        bool timelineChanged = false;
        if (slot.lastRequestedFillFrames >= 0.0
            && std::abs(
                   requestedFill - slot.lastRequestedFillFrames)
                > 0.5) {
            slot.ring.reprime();
            timelineChanged = true;
        }
        slot.lastRequestedFillFrames = requestedFill;
        auto& scratch = slot.scratch;
        if (scratch.getNumSamples() < frames) {
            slot.ring.reprime();
            slot.pitchShifter.reset();
            slot.lastRequestedFillFrames = -1.0;
            slot.lastPitchActive = pitchActive;
            slot.fillFrames.store(0.0, std::memory_order_relaxed);
            slot.ratio.store(1.0, std::memory_order_relaxed);
            for (auto& meter : slot.meterPeaks)
                updateMeterPeak(meter, 0.0f, release);
            continue;
        }
        scratch.clear(0, frames);
        std::array<float*, 2> sourceOutputs{
            scratch.getWritePointer(0),
            channels > 1 ? scratch.getWritePointer(1)
                         : scratch.getWritePointer(0)};
        auto result = slot.ring.renderAdd(
            sourceOutputs.data(), channels,
            static_cast<std::uint32_t>(frames),
            1.0f, requestedFill);
        if (timelineChanged || result.discontinuity
            || pitchActive != slot.lastPitchActive) {
            slot.pitchShifter.reset();
        }
        slot.lastPitchActive = pitchActive;
        slot.fillFrames.store(
            result.fillFrames, std::memory_order_relaxed);
        slot.ratio.store(result.ratio, std::memory_order_relaxed);
        if (result.renderedFrames
            != static_cast<std::uint32_t>(frames)) {
            scratch.clear(0, frames);
            for (auto& meter : slot.meterPeaks)
                updateMeterPeak(meter, 0.0f, release);
            continue;
        }
        if (pitchActive) {
            const auto ratio =
                std::pow(2.0, static_cast<double>(semitones) / 12.0);
            slot.pitchShifter.process(
                sourceOutputs.data(), channels, frames, ratio);
        }
        const auto sourceGain = dbToGain(gainDb);
        for (int meterChannel = 0; meterChannel < 2; ++meterChannel) {
            const auto channel = std::min(meterChannel, channels - 1);
            updateMeterPeak(
                slot.meterPeaks[
                    static_cast<std::size_t>(meterChannel)],
                scratch.getMagnitude(channel, 0, frames)
                    * std::abs(sourceGain),
                release);
        }
        for (int channel = 0; channel < channels; ++channel)
            buffer.addFrom(
                channel, 0, scratch, channel, 0,
                frames, sourceGain);
    }
    for (int meterChannel = 0; meterChannel < 2; ++meterChannel) {
        const auto channel = std::min(meterChannel, channels - 1);
        updateMeterPeak(
            mainOutputMeterPeaks_[
                static_cast<std::size_t>(meterChannel)],
            buffer.getMagnitude(channel, 0, frames), release);
    }
}

void DPWIMAudioProcessor::prepareAudioPath(
    AudioPathMode mode, int frames, double rate) noexcept
{
    const auto now = juce::Time::getHighResolutionTicks();
    bool discontinuity = false;
    if (lastAudioBlockTick_ != 0) {
        const auto elapsed =
            juce::Time::highResolutionTicksToSeconds(
                now - lastAudioBlockTick_);
        const auto expected =
            static_cast<double>(frames) / std::max(1.0, rate);
        discontinuity =
            elapsed > std::max(0.05, expected * 4.0);
    }

    const bool previousUsesDryDelay =
        audioPathMode_ == AudioPathMode::Normal
        || audioPathMode_ == AudioPathMode::HostBypass;
    const bool nextUsesDryDelay =
        mode == AudioPathMode::Normal
        || mode == AudioPathMode::HostBypass;
    if (discontinuity
        || audioPathMode_ == AudioPathMode::Uninitialised
        || previousUsesDryDelay != nextUsesDryDelay)
        dryDelay_.reset();

    if (mode != audioPathMode_ || discontinuity) {
        for (auto& slot : slots_) {
            slot->ring.reprime();
            slot->pitchShifter.reset();
            slot->lastRequestedFillFrames = -1.0;
        }
    }

    audioPathMode_ = mode;
    lastAudioBlockTick_ = now;
}

void DPWIMAudioProcessor::updateMeterPeak(
    std::atomic<float>& meter, float peak, float release) noexcept
{
    const auto previous = meter.load(std::memory_order_relaxed);
    auto next = peak >= previous ? peak : previous * release;
    if (next < 0.000001f)
        next = 0.0f;
    meter.store(next, std::memory_order_relaxed);
}

void DPWIMAudioProcessor::timerCallback()
{
    updateLatencyReport();
}

void DPWIMAudioProcessor::parameterChanged(
    const juce::String&, float)
{
    requestLatencyReportUpdate();
}

void DPWIMAudioProcessor::requestLatencyReportUpdate()
{
    if (auto* messageManager =
            juce::MessageManager::getInstanceWithoutCreating();
        messageManager != nullptr
        && messageManager->isThisTheMessageThread()) {
        updateLatencyReport();
    } else {
        triggerAsyncUpdate();
    }
}

void DPWIMAudioProcessor::handleAsyncUpdate()
{
    updateLatencyReport();
}

void DPWIMAudioProcessor::updateLatencyReport()
{
    const auto observedSafetyFrames =
        observedBlockSafetyFrames_.load(std::memory_order_acquire);
    const auto committedSafetyFrames =
        blockSafetyFrames_.load(std::memory_order_acquire);
    const auto candidateSafetyFrames =
        std::max(observedSafetyFrames, committedSafetyFrames);
    const auto rate = sampleRate_.load(std::memory_order_acquire);
    const bool bypassed =
        bypassParam_->load(std::memory_order_relaxed) >= 0.5f;
    const auto latency = bypassed
        ? 0
        : dpwim::latencySamples(
              currentTimeline(candidateSafetyFrames)
                  .effectiveLatencyMs,
              rate);
    const bool hostNotificationRequired =
        latency
        != lastReportedLatency_.load(std::memory_order_acquire);
    const bool safetyPublicationRequired =
        candidateSafetyFrames > committedSafetyFrames;
    notifyHostThenPublishAudioSafety(
        hostNotificationRequired,
        [&] {
            setLatencySamples(latency);
            lastReportedLatency_.store(
                latency, std::memory_order_release);
        },
        safetyPublicationRequired,
        [&] {
            auto pendingSafetyFrames =
                pendingCommittedBlockSafetyFrames_.load(
                    std::memory_order_relaxed);
            while (pendingSafetyFrames < candidateSafetyFrames
                   && !pendingCommittedBlockSafetyFrames_
                           .compare_exchange_weak(
                               pendingSafetyFrames,
                               candidateSafetyFrames,
                               std::memory_order_release,
                               std::memory_order_relaxed)) {
            }
        });
}

bool DPWIMAudioProcessor::observeHostBlockSize(
    int frames) noexcept
{
    const auto capacity =
        realtimeBlockCapacityFrames_.load(std::memory_order_acquire);
    const auto supportedFrames = std::min(frames, capacity);
    const auto requiredSafetyFrames =
        static_cast<int>(
            dpwim::AudioRingBuffer::safeTargetFillFrames(
                static_cast<std::uint32_t>(
                    supportedFrames)));
    auto previous =
        observedBlockSafetyFrames_.load(std::memory_order_relaxed);
    while (previous < requiredSafetyFrames
           && !observedBlockSafetyFrames_.compare_exchange_weak(
               previous, requiredSafetyFrames,
               std::memory_order_release,
               std::memory_order_relaxed)) {
    }
    return frames <= capacity
        && requiredSafetyFrames
            <= blockSafetyFrames_.load(std::memory_order_acquire);
}

void DPWIMAudioProcessor::applyPendingBlockSafety() noexcept
{
    const auto pendingSafetyFrames =
        pendingCommittedBlockSafetyFrames_.exchange(
            0, std::memory_order_acq_rel);
    if (pendingSafetyFrames
        > blockSafetyFrames_.load(std::memory_order_relaxed)) {
        blockSafetyFrames_.store(
            pendingSafetyFrames, std::memory_order_release);
    }
}

dpwim::SyncTimelinePlan
DPWIMAudioProcessor::currentTimeline(
    int blockSafetyFrames) const noexcept
{
    std::array<dpwim::SyncSourceTiming, dpwim::kSyncSourceCount>
        sources{};
    for (int index = 0; index < kSourceSlots; ++index) {
        const auto slotIndex = static_cast<std::size_t>(index);
        const auto& slot = *slots_[slotIndex];
        const auto mode = static_cast<SourceMode>(
            slot.mode.load(std::memory_order_acquire));
        sources[slotIndex].enabled =
            mode != SourceMode::Off
            && slot.enabled.load(std::memory_order_acquire);
        sources[slotIndex].offsetMs =
            sourceOffsetParams_[slotIndex]->load(
                std::memory_order_relaxed);
        const auto semitones =
            sourceTransposeParams_[slotIndex]->load(
                std::memory_order_relaxed)
            + sourceFinePitchParams_[slotIndex]->load(
                  std::memory_order_relaxed)
                / 100.0f;
        if (sources[slotIndex].enabled
            && std::abs(semitones) >= 0.005f) {
            const auto rate =
                sampleRate_.load(std::memory_order_acquire);
            sources[slotIndex].processingLatencyMs =
                static_cast<double>(
                    dpwim::PhaseVocoderPitchShifter::kLatencySamples)
                * 1000.0 / std::max(1.0, rate);
        }
    }
    const auto rate =
        sampleRate_.load(std::memory_order_acquire);
    const auto blockSafetyMs =
        static_cast<double>(
            std::max(blockSafetyFrames, 1))
        * 1000.0 / std::max(1.0, rate);
    return dpwim::calculateSyncTimeline(
        targetLatencyParam_->load(std::memory_order_relaxed),
        sources, blockSafetyMs);
}

DPWIMAudioProcessor::LatencySnapshot
DPWIMAudioProcessor::latencySnapshot() const noexcept
{
    const auto visibleSafetyFrames =
        std::max(
            blockSafetyFrames_.load(std::memory_order_acquire),
            pendingCommittedBlockSafetyFrames_.load(
                std::memory_order_acquire));
    const auto timeline = currentTimeline(visibleSafetyFrames);
    const auto rate = sampleRate_.load(std::memory_order_acquire);
    const bool bypassed =
        bypassParam_->load(std::memory_order_relaxed) >= 0.5f;
    if (bypassed)
        return {timeline.targetLatencyMs, 0.0, 0.0, 0};
    return {
        timeline.targetLatencyMs,
        timeline.syncAdditionMs,
        timeline.effectiveLatencyMs,
        dpwim::latencySamples(timeline.effectiveLatencyMs, rate)};
}

DPWIMAudioProcessor::LevelSnapshot
DPWIMAudioProcessor::levelSnapshot() const noexcept
{
    LevelSnapshot snapshot;
    for (std::size_t channel = 0; channel < snapshot.dry.size();
         ++channel)
        snapshot.dry[channel] =
            dryMeterPeaks_[channel].load(std::memory_order_relaxed);
    for (std::size_t slotIndex = 0;
         slotIndex < snapshot.sources.size(); ++slotIndex)
        for (std::size_t channel = 0;
             channel < snapshot.sources[slotIndex].size(); ++channel)
            snapshot.sources[slotIndex][channel] =
                slots_[slotIndex]->meterPeaks[channel].load(
                    std::memory_order_relaxed);
    for (std::size_t channel = 0;
         channel < snapshot.mainOutput.size(); ++channel)
        snapshot.mainOutput[channel] =
            mainOutputMeterPeaks_[channel].load(
                std::memory_order_relaxed);
    return snapshot;
}

std::vector<dpwim::ProcessInfo>
DPWIMAudioProcessor::enumerateProcesses() const
{
    return dpwim::ProcessCatalog::enumerate();
}

void DPWIMAudioProcessor::setSource(int slotIndex, SourceMode mode,
                                    std::uint32_t pid,
                                    const juce::String& executable)
{
    if (!juce::isPositiveAndBelow(slotIndex, kSourceSlots))
        return;
    auto& slot = *slots_[static_cast<std::size_t>(slotIndex)];
    slot.capture.stop();
    slot.ring.discard();
    {
        std::lock_guard<std::mutex> lock(slot.identityMutex);
        slot.executable = executable;
    }
    slot.pid.store(pid, std::memory_order_release);
    slot.mode.store(static_cast<int>(mode), std::memory_order_release);
    slot.enabled.store(
        mode != SourceMode::Off, std::memory_order_release);
    requestLatencyReportUpdate();
    startSlot(slotIndex);
}

void DPWIMAudioProcessor::setSourceEnabled(int slotIndex, bool enabled)
{
    if (!juce::isPositiveAndBelow(slotIndex, kSourceSlots))
        return;
    auto& slot = *slots_[static_cast<std::size_t>(slotIndex)];
    const auto mode = static_cast<SourceMode>(
        slot.mode.load(std::memory_order_acquire));
    const bool shouldEnable = enabled && mode != SourceMode::Off;
    const bool wasEnabled =
        slot.enabled.exchange(shouldEnable, std::memory_order_acq_rel);
    if (wasEnabled == shouldEnable)
        return;
    requestLatencyReportUpdate();
    if (!shouldEnable) {
        slot.capture.stop();
        slot.ring.discard();
        slot.fillFrames.store(0.0, std::memory_order_relaxed);
        slot.ratio.store(1.0, std::memory_order_relaxed);
        return;
    }
    startSlot(slotIndex);
}

void DPWIMAudioProcessor::startSlot(int slotIndex)
{
    if (!prepared_.load(std::memory_order_acquire)
        || !juce::isPositiveAndBelow(slotIndex, kSourceSlots))
        return;

    auto& slot = *slots_[static_cast<std::size_t>(slotIndex)];
    if (!slot.enabled.load(std::memory_order_acquire))
        return;
    const auto mode =
        static_cast<SourceMode>(slot.mode.load(std::memory_order_acquire));
    if (mode == SourceMode::Off)
        return;

    std::uint32_t pid = slot.pid.load(std::memory_order_acquire);
    if (mode == SourceMode::Desktop) {
        pid = GetCurrentProcessId();
    } else if (pid == 0) {
        juce::String executable;
        {
            std::lock_guard<std::mutex> lock(slot.identityMutex);
            executable = slot.executable;
        }
        pid = dpwim::ProcessCatalog::findPidByExecutable(
            executable.toWideCharPointer());
        slot.pid.store(pid, std::memory_order_release);
    }

    if (pid == 0)
        return;

    const auto rate = static_cast<std::uint32_t>(
        std::llround(sampleRate_.load(std::memory_order_acquire)));
    auto* const slotPointer = &slot;
    slot.capture.start(
        pid, mode == SourceMode::Application, rate,
        [slotPointer](const float* data, std::uint32_t frames,
                      std::uint32_t channels, bool silent,
                      bool discontinuity) {
            if (discontinuity)
                slotPointer->ring.discard();
            slotPointer->ring.write(data, frames, channels, silent);
        });
}

void DPWIMAudioProcessor::stopAllSlots() noexcept
{
    for (auto& slot : slots_)
        slot->capture.stop();
}

DPWIMAudioProcessor::SourceSnapshot
DPWIMAudioProcessor::sourceSnapshot(int slotIndex) const
{
    SourceSnapshot snapshot;
    if (!juce::isPositiveAndBelow(slotIndex, kSourceSlots))
        return snapshot;
    const auto& slot = *slots_[static_cast<std::size_t>(slotIndex)];
    snapshot.mode =
        static_cast<SourceMode>(slot.mode.load(std::memory_order_acquire));
    snapshot.enabled =
        slot.enabled.load(std::memory_order_acquire);
    snapshot.pid = slot.pid.load(std::memory_order_acquire);
    {
        std::lock_guard<std::mutex> lock(slot.identityMutex);
        snapshot.executable = slot.executable;
    }
    snapshot.status = slot.capture.status();
    snapshot.running = slot.capture.isRunning();
    snapshot.fillFrames = slot.fillFrames.load(std::memory_order_relaxed);
    snapshot.ratio = slot.ratio.load(std::memory_order_relaxed);
    return snapshot;
}

juce::ValueTree DPWIMAudioProcessor::createSourceState() const
{
    juce::ValueTree sources("SOURCES");
    sources.setProperty("schemaVersion", 2, nullptr);
    for (int index = 0; index < kSourceSlots; ++index) {
        const auto snapshot = sourceSnapshot(index);
        juce::ValueTree child("SOURCE");
        child.setProperty("slot", index, nullptr);
        child.setProperty("mode", static_cast<int>(snapshot.mode), nullptr);
        child.setProperty("enabled", snapshot.enabled, nullptr);
        child.setProperty("pid", static_cast<juce::int64>(snapshot.pid), nullptr);
        child.setProperty("executable", snapshot.executable, nullptr);
        sources.addChild(child, -1, nullptr);
    }
    return sources;
}

void DPWIMAudioProcessor::restoreSources(const juce::ValueTree& sources)
{
    for (int index = 0; index < sources.getNumChildren(); ++index) {
        const auto child = sources.getChild(index);
        const int slotIndex = child.getProperty("slot", -1);
        if (!juce::isPositiveAndBelow(slotIndex, kSourceSlots))
            continue;
        const auto mode = static_cast<SourceMode>(
            static_cast<int>(child.getProperty("mode", 0)));
        auto pid = static_cast<std::uint32_t>(
            static_cast<juce::int64>(child.getProperty("pid", 0)));
        const juce::String executable =
            child.getProperty("executable").toString();
        const bool enabled = child.hasProperty("enabled")
            ? static_cast<bool>(child.getProperty("enabled"))
            : mode != SourceMode::Off;
        if (mode == SourceMode::Application && executable.isNotEmpty()) {
            const auto livePid = dpwim::ProcessCatalog::findPidByExecutable(
                executable.toWideCharPointer());
            if (livePid != 0)
                pid = livePid;
        }
        setSource(slotIndex, mode, pid, executable);
        setSourceEnabled(slotIndex, enabled);
    }
}

void DPWIMAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = apvts_.copyState();
    state.addChild(createSourceState(), -1, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destination);
}

void DPWIMAudioProcessor::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr)
        return;
    auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid() || state.getType() != apvts_.state.getType())
        return;
    if (!state.getChildWithProperty("id", "bypass").isValid()) {
        juce::ValueTree bypassState("PARAM");
        bypassState.setProperty("id", "bypass", nullptr);
        bypassState.setProperty("value", 0.0f, nullptr);
        state.addChild(bypassState, -1, nullptr);
    }

    auto sources = state.getChildWithName("SOURCES").createCopy();
    if (auto liveSources = state.getChildWithName("SOURCES");
        liveSources.isValid())
        state.removeChild(liveSources, nullptr);
    apvts_.replaceState(state);
    if (sources.isValid())
        restoreSources(sources);
}

juce::AudioProcessorEditor* DPWIMAudioProcessor::createEditor()
{
    return new DPWIMAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DPWIMAudioProcessor();
}
