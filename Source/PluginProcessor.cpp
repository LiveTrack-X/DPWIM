#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <windows.h>

#include <algorithm>
#include <cmath>

namespace {

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
    dryGainParam_ = apvts_.getRawParameterValue("dryGain");
    for (int index = 0; index < kSourceSlots; ++index) {
        slots_[static_cast<std::size_t>(index)] =
            std::make_unique<SourceSlot>();
        sourceGainParams_[static_cast<std::size_t>(index)] =
            apvts_.getRawParameterValue(gainParameter(index));
        sourceOffsetParams_[static_cast<std::size_t>(index)] =
            apvts_.getRawParameterValue(offsetParameter(index));
    }
    startTimerHz(10);
}

DPWIMAudioProcessor::~DPWIMAudioProcessor()
{
    stopTimer();
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

juce::AudioProcessorValueTreeState::ParameterLayout
DPWIMAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"targetLatency", 1}, "Target Latency",
        juce::NormalisableRange<float>(10.0f, 250.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dryGain", 1}, "Dry Gain",
        juce::NormalisableRange<float>(-60.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    for (int slot = 0; slot < kSourceSlots; ++slot) {
        parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{gainParameter(slot), 1},
            "Source " + juce::String(slot + 1) + " Gain",
            juce::NormalisableRange<float>(-60.0f, 12.0f, 0.1f), -6.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));
        parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{offsetParameter(slot), 1},
            "Source " + juce::String(slot + 1) + " Offset",
            juce::NormalisableRange<float>(-200.0f, 200.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms")));
    }
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
    juce::ignoreUnused(samplesPerBlock);
    stopAllSlots();
    sampleRate_.store(sampleRate, std::memory_order_release);
    dryDelay_.prepare(getTotalNumOutputChannels(),
                      static_cast<int>(std::ceil(sampleRate * 0.5)));
    for (auto& slot : slots_)
        slot->ring.configure(
            static_cast<std::size_t>(std::ceil(sampleRate * 5.0)), 2);
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
}

void DPWIMAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels = buffer.getNumChannels();
    const int frames = buffer.getNumSamples();
    if (channels <= 0 || frames <= 0)
        return;

    const float dryDb = dryGainParam_->load(std::memory_order_relaxed);
    buffer.applyGain(dbToGain(dryDb));

    const float targetMs =
        targetLatencyParam_->load(std::memory_order_relaxed);
    const double rate = sampleRate_.load(std::memory_order_acquire);
    const int targetFrames =
        static_cast<int>(std::llround(rate * targetMs / 1000.0));

    std::array<float*, 2> outputs{
        buffer.getWritePointer(0),
        channels > 1 ? buffer.getWritePointer(1)
                     : buffer.getWritePointer(0)};
    dryDelay_.process(outputs.data(), channels, frames, targetFrames);

    for (int index = 0; index < kSourceSlots; ++index) {
        auto& slot = *slots_[static_cast<std::size_t>(index)];
        if (slot.mode.load(std::memory_order_acquire)
            == static_cast<int>(SourceMode::Off))
            continue;

        const float gainDb =
            sourceGainParams_[static_cast<std::size_t>(index)]
                ->load(std::memory_order_relaxed);
        const float offsetMs =
            sourceOffsetParams_[static_cast<std::size_t>(index)]
                ->load(std::memory_order_relaxed);
        const double requestedFill =
            rate * std::max(2.0f, targetMs + offsetMs) / 1000.0;
        auto result = slot.ring.renderAdd(
            outputs.data(), channels, static_cast<std::uint32_t>(frames),
            dbToGain(gainDb), requestedFill);
        slot.fillFrames.store(result.fillFrames, std::memory_order_relaxed);
        slot.ratio.store(result.ratio, std::memory_order_relaxed);
    }
}

void DPWIMAudioProcessor::timerCallback()
{
    updateLatencyReport();
}

void DPWIMAudioProcessor::updateLatencyReport()
{
    const auto rate = sampleRate_.load(std::memory_order_acquire);
    const auto targetMs =
        targetLatencyParam_->load(std::memory_order_relaxed);
    const auto latency = static_cast<int>(
        std::llround(rate * targetMs / 1000.0));
    if (latency
        != lastReportedLatency_.exchange(
            latency, std::memory_order_acq_rel)) {
        setLatencySamples(latency);
    }
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
    slot.ring.reset();
    {
        std::lock_guard<std::mutex> lock(slot.identityMutex);
        slot.executable = executable;
    }
    slot.pid.store(pid, std::memory_order_release);
    slot.mode.store(static_cast<int>(mode), std::memory_order_release);
    startSlot(slotIndex);
}

void DPWIMAudioProcessor::startSlot(int slotIndex)
{
    if (!prepared_.load(std::memory_order_acquire)
        || !juce::isPositiveAndBelow(slotIndex, kSourceSlots))
        return;

    auto& slot = *slots_[static_cast<std::size_t>(slotIndex)];
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
                      std::uint32_t channels, bool silent) {
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
    sources.setProperty("schemaVersion", 1, nullptr);
    for (int index = 0; index < kSourceSlots; ++index) {
        const auto snapshot = sourceSnapshot(index);
        juce::ValueTree child("SOURCE");
        child.setProperty("slot", index, nullptr);
        child.setProperty("mode", static_cast<int>(snapshot.mode), nullptr);
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
        if (mode == SourceMode::Application && executable.isNotEmpty()) {
            const auto livePid = dpwim::ProcessCatalog::findPidByExecutable(
                executable.toWideCharPointer());
            if (livePid != 0)
                pid = livePid;
        }
        setSource(slotIndex, mode, pid, executable);
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
