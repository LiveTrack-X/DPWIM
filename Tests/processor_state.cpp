#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/PhaseVocoderPitchShifter.h"

#include <JuceHeader.h>

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

void checkValue(DPWIMAudioProcessor& processor,
                const juce::String& parameterId,
                float expected)
{
    const auto* value =
        processor.parameters().getRawParameterValue(parameterId);
    check(value != nullptr, "expected parameter exists");
    if (value != nullptr)
        check(std::abs(value->load() - expected) < 0.01f,
              "parameter has the expected value");
}

void setPlainValue(DPWIMAudioProcessor& processor,
                   const juce::String& parameterId,
                   float plainValue)
{
    auto* parameter = processor.parameters().getParameter(parameterId);
    check(parameter != nullptr, "expected writable parameter exists");
    if (parameter != nullptr)
        parameter->setValueNotifyingHost(
            parameter->convertTo0to1(plainValue));
}

double toneMagnitude(const std::vector<float>& audio,
                     std::size_t start, std::size_t frames,
                     double frequency, double sampleRate)
{
    constexpr double twoPi = 6.28318530717958647692;
    double real = 0.0;
    double imaginary = 0.0;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const auto phase =
            twoPi * frequency * static_cast<double>(frame) / sampleRate;
        const auto sample = audio[start + frame];
        real += sample * std::cos(phase);
        imaginary -= sample * std::sin(phase);
    }
    return std::sqrt(real * real + imaginary * imaginary)
        / static_cast<double>(frames);
}

void checkPitchShifter()
{
    constexpr double sampleRate = 48000.0;
    constexpr std::size_t frames = 96000;
    constexpr double twoPi = 6.28318530717958647692;
    std::vector<float> audio(frames);
    for (std::size_t frame = 0; frame < frames; ++frame)
        audio[frame] = static_cast<float>(
            0.25 * std::sin(
                       twoPi * 440.0 * static_cast<double>(frame)
                       / sampleRate));

    dpwim::PhaseVocoderPitchShifter shifter;
    shifter.prepare(sampleRate);
    for (std::size_t start = 0; start < frames; start += 512) {
        const auto block = static_cast<int>(
            std::min<std::size_t>(512, frames - start));
        float* channel = audio.data() + start;
        float* channels[] = {channel};
        shifter.process(channels, 1, block, 2.0);
    }

    const auto analysisStart = frames - 24000;
    const auto shifted = toneMagnitude(
        audio, analysisStart, 24000, 880.0, sampleRate);
    const auto original = toneMagnitude(
        audio, analysisStart, 24000, 440.0, sampleRate);
    check(std::isfinite(shifted) && shifted > 0.01,
          "pitch shifter produces finite shifted audio");
    check(shifted > original * 2.0,
          "one-octave transpose moves energy from 440 to 880 Hz");
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    DPWIMAudioProcessor source;
    const auto* baseLatencyParameter =
        source.parameters().getParameter("targetLatency");
    check(baseLatencyParameter != nullptr,
          "base latency keeps the targetLatency parameter ID");
    if (baseLatencyParameter != nullptr)
        check(baseLatencyParameter->getName(64) == "Base Latency",
              "base latency exposes the revised host-visible name");
    check(
        DPWIMAudioProcessorEditor::formatLatencySummary(
            35.0, 25.0, 1680)
            == "OUT 35.0 ms | SYNC +25.0 ms | 1680 smp",
        "latency summary uses the documented display format");
    check(
        dpwim::UpdateChecker::isNewerVersion("0.2.3", "v0.2.4"),
        "update checker accepts a newer patch release");
    check(
        dpwim::UpdateChecker::isNewerVersion("0.9.9", "v0.10.0"),
        "update checker compares numeric version components");
    check(
        !dpwim::UpdateChecker::isNewerVersion("0.2.3", "v0.2.3")
            && !dpwim::UpdateChecker::isNewerVersion(
                "0.2.3", "v0.2.2"),
        "update checker rejects equal and older releases");

    checkValue(source, "targetLatency", 10.0f);
    checkValue(source, "dryEnabled", 1.0f);
    checkValue(source, "bypass", 0.0f);
    check(
        source.getBypassParameter()
            == source.parameters().getParameter("bypass"),
        "processor exposes the persistent bypass parameter to the host");
    const auto initialLevels = source.levelSnapshot();
    check(initialLevels.dry[0] == 0.0f
              && initialLevels.dry[1] == 0.0f,
          "level meters start silent");
    check(initialLevels.mainOutput[0] == 0.0f
              && initialLevels.mainOutput[1] == 0.0f,
          "main output meter starts silent");
    for (int slot = 0; slot < DPWIMAudioProcessor::kSourceSlots; ++slot)
        checkValue(
            source, "sourceGain" + juce::String(slot), 0.0f);
    for (int slot = 0; slot < DPWIMAudioProcessor::kSourceSlots; ++slot) {
        checkValue(
            source, "sourceTranspose" + juce::String(slot), 0.0f);
        checkValue(
            source, "sourceFinePitch" + juce::String(slot), 0.0f);
    }

    source.setSource(
        1, DPWIMAudioProcessor::SourceMode::Application,
        4242, "example.exe");
    setPlainValue(source, "sourceOffset1", -25.0f);
    const auto rebasedLatency = source.latencySnapshot();
    check(std::abs(rebasedLatency.targetMs - 10.0) < 0.01,
          "target latency remains the configured safety buffer");
    check(std::abs(rebasedLatency.syncAdditionMs - 25.0) < 0.01,
          "negative source offset adds common sync latency");
    check(std::abs(rebasedLatency.effectiveMs - 35.0) < 0.01,
          "effective latency includes negative-offset rebase");
    check(rebasedLatency.samples == 1680,
          "effective latency reports rounded host samples");
    source.setSourceEnabled(1, false);
    const auto disabledLatency = source.latencySnapshot();
    check(std::abs(disabledLatency.effectiveMs - 10.0) < 0.01,
          "disabled source does not increase effective latency");

    DPWIMAudioProcessor timingProcessor;
    timingProcessor.setSource(
        0, DPWIMAudioProcessor::SourceMode::Application,
        0, {});
    setPlainValue(timingProcessor, "sourceOffset0", -25.0f);
    timingProcessor.prepareToPlay(48000.0, 512);
    check(timingProcessor.getLatencySamples() == 1680,
          "processor reports effective rebased latency to the host");
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    int impulseFrame = -1;
    for (int block = 0; block < 5; ++block) {
        audio.clear();
        if (block == 0) {
            audio.setSample(0, 0, 0.5f);
            audio.setSample(1, 0, 0.5f);
        }
        timingProcessor.processBlock(audio, midi);
        for (int frame = 0; frame < audio.getNumSamples(); ++frame) {
            if (impulseFrame < 0
                && std::abs(audio.getSample(0, frame)) > 0.4f)
                impulseFrame = block * 512 + frame;
        }
    }
    check(impulseFrame == 1680,
          "dry path follows the effective rebased latency");
    const auto delayedLevels = timingProcessor.levelSnapshot();
    check(delayedLevels.dry[0] > 0.45f
              && delayedLevels.dry[1] > 0.45f,
          "dry level snapshot follows the delayed post-gain path");
    check(delayedLevels.mainOutput[0] > 0.45f
              && delayedLevels.mainOutput[1] > 0.45f,
          "main output snapshot follows the final normal mix");
    timingProcessor.releaseResources();

    DPWIMAudioProcessor bypassProcessor;
    setPlainValue(bypassProcessor, "dryEnabled", 0.0f);
    setPlainValue(bypassProcessor, "dryGain", -60.0f);
    setPlainValue(bypassProcessor, "bypass", 1.0f);
    bypassProcessor.prepareToPlay(48000.0, 512);
    check(bypassProcessor.getLatencySamples() == 0,
          "bypass reports zero output latency");
    const auto bypassLatency = bypassProcessor.latencySnapshot();
    check(bypassLatency.effectiveMs == 0.0
              && bypassLatency.syncAdditionMs == 0.0
              && bypassLatency.samples == 0,
          "bypass exposes zero OUT and SYNC latency");
    int bypassImpulseFrame = -1;
    float bypassImpulseValue = 0.0f;
    for (int block = 0; block < 3; ++block) {
        audio.clear();
        if (block == 0) {
            audio.setSample(0, 0, 0.5f);
            audio.setSample(1, 0, 0.5f);
        }
        bypassProcessor.processBlock(audio, midi);
        for (int frame = 0; frame < audio.getNumSamples(); ++frame) {
            if (bypassImpulseFrame < 0
                && std::abs(audio.getSample(0, frame)) > 0.4f) {
                bypassImpulseFrame = block * 512 + frame;
                bypassImpulseValue = audio.getSample(0, frame);
            }
        }
    }
    check(bypassImpulseFrame == 0,
          "bypass passes the raw host input immediately");
    check(std::abs(bypassImpulseValue - 0.5f) < 0.001f,
          "bypass ignores Dry Input enable and gain processing");
    const auto bypassLevels = bypassProcessor.levelSnapshot();
    check(bypassLevels.dry[0] > 0.45f
              && bypassLevels.dry[1] > 0.45f,
          "bypass meter follows the immediate raw host input");
    check(bypassLevels.mainOutput[0] > 0.45f
              && bypassLevels.mainOutput[1] > 0.45f,
          "main output meter follows immediate bypass output");
    check(bypassLevels.sources[0][0] == 0.0f
              && bypassLevels.sources[0][1] == 0.0f,
          "source meters remain silent during global bypass");
    bypassProcessor.releaseResources();

    setPlainValue(source, "targetLatency", 50.0f);
    setPlainValue(source, "dryEnabled", 0.0f);
    setPlainValue(source, "bypass", 1.0f);
    setPlainValue(source, "sourceGain1", -6.0f);
    setPlainValue(source, "sourceTranspose1", 7.0f);
    setPlainValue(source, "sourceFinePitch1", -25.0f);
    const auto disabled = source.sourceSnapshot(1);
    check(disabled.mode == DPWIMAudioProcessor::SourceMode::Application,
          "disabling preserves source mode");
    check(disabled.executable == "example.exe",
          "disabling preserves executable identity");
    check(!disabled.enabled, "source is disabled");

    juce::MemoryBlock state;
    source.getStateInformation(state);
    DPWIMAudioProcessor restored;
    restored.setStateInformation(
        state.getData(), static_cast<int>(state.getSize()));
    const auto restoredSnapshot = restored.sourceSnapshot(1);
    check(
        restoredSnapshot.mode
            == DPWIMAudioProcessor::SourceMode::Application,
        "source mode round-trips");
    check(restoredSnapshot.executable == "example.exe",
          "source executable round-trips");
    check(!restoredSnapshot.enabled,
          "disabled state round-trips");
    checkValue(restored, "targetLatency", 50.0f);
    checkValue(restored, "dryEnabled", 0.0f);
    checkValue(restored, "bypass", 1.0f);
    checkValue(restored, "sourceGain1", -6.0f);
    checkValue(restored, "sourceTranspose1", 7.0f);
    checkValue(restored, "sourceFinePitch1", -25.0f);

    auto legacyTree = source.parameters().copyState();
    if (const auto bypassState =
            legacyTree.getChildWithProperty("id", "bypass");
        bypassState.isValid())
        legacyTree.removeChild(bypassState, nullptr);
    juce::MemoryBlock legacyState;
    if (auto legacyXml = legacyTree.createXml())
        juce::AudioProcessor::copyXmlToBinary(
            *legacyXml, legacyState);
    DPWIMAudioProcessor legacyRestored;
    setPlainValue(legacyRestored, "bypass", 1.0f);
    legacyRestored.setStateInformation(
        legacyState.getData(),
        static_cast<int>(legacyState.getSize()));
    checkValue(legacyRestored, "bypass", 0.0f);

    restored.setSourceEnabled(1, true);
    check(restored.sourceSnapshot(1).enabled,
          "source can be re-enabled without reselection");

    DPWIMAudioProcessor pitchTimeline;
    pitchTimeline.setSource(
        0, DPWIMAudioProcessor::SourceMode::Application, 0, {});
    setPlainValue(pitchTimeline, "sourceTranspose0", 12.0f);
    pitchTimeline.prepareToPlay(48000.0, 512);
    const auto pitchLatency = pitchTimeline.latencySnapshot();
    check(std::abs(pitchLatency.effectiveMs - 42.0) < 0.01,
          "active pitch shifting adds its fixed processing latency");
    check(pitchLatency.samples == 2016,
          "pitch latency is included in host sample reporting");
    pitchTimeline.releaseResources();

    checkPitchShifter();

    if (failures == 0) {
        std::cout << "DPWIM processor state tests passed\n";
        return 0;
    }
    std::cerr << failures << " processor state test(s) failed\n";
    return 1;
}
