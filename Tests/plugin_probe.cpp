#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include <cmath>
#include <iostream>
#include <memory>

namespace {

bool checkDefault(juce::AudioPluginInstance& instance,
                  const juce::String& parameterName,
                  float expectedNormalized)
{
    for (auto* parameter : instance.getParameters()) {
        if (parameter->getName(128) != parameterName)
            continue;
        const auto actual = parameter->getValue();
        if (std::abs(actual - expectedNormalized) < 0.002f)
            return true;
        std::cerr << parameterName << " normalized default was "
                  << actual << ", expected " << expectedNormalized
                  << '\n';
        return false;
    }
    std::cerr << "Missing parameter " << parameterName << '\n';
    return false;
}

bool setParameter(juce::AudioPluginInstance& instance,
                  const juce::String& parameterName,
                  float normalizedValue)
{
    for (auto* parameter : instance.getParameters()) {
        if (parameter->getName(128) != parameterName)
            continue;
        parameter->setValueNotifyingHost(normalizedValue);
        return true;
    }
    std::cerr << "Missing parameter " << parameterName << '\n';
    return false;
}

bool probe(juce::AudioPluginFormat& format, const juce::String& path)
{
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions, path);
    if (descriptions.isEmpty()) {
        std::cerr << "No plugin type found in " << path << '\n';
        return false;
    }

    juce::String error;
    auto instance = format.createInstanceFromDescription(
        *descriptions[0], 48000.0, 512, error);

    if (instance == nullptr) {
        std::cerr << "Instantiation failed for " << path << ": "
                  << error << '\n';
        return false;
    }

    instance->setPlayConfigDetails(2, 2, 48000.0, 512);
    bool defaultsOk = checkDefault(*instance, "Base Latency", 0.0f);
    defaultsOk =
        checkDefault(*instance, "Dry Input Enabled", 1.0f)
        && defaultsOk;
    defaultsOk =
        checkDefault(*instance, "DPWIM Bypass", 0.0f)
        && defaultsOk;
    for (int slot = 0; slot < 4; ++slot)
        defaultsOk = checkDefault(
            *instance,
            "Source " + juce::String(slot + 1) + " Gain",
            60.0f / 72.0f)
            && defaultsOk;
    for (int slot = 0; slot < 4; ++slot) {
        defaultsOk = checkDefault(
            *instance,
            "Source " + juce::String(slot + 1) + " Transpose",
            0.5f)
            && defaultsOk;
        defaultsOk = checkDefault(
            *instance,
            "Source " + juce::String(slot + 1) + " Fine Pitch",
            0.5f)
            && defaultsOk;
    }
    instance->prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;

    bool finite = true;
    int impulseOutputFrame = -1;
    for (int block = 0; block < 6; ++block) {
        audio.clear();
        if (block == 0) {
            audio.setSample(0, 0, 0.25f);
            audio.setSample(1, 0, 0.25f);
        }
        instance->processBlock(audio, midi);
        for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
            for (int frame = 0; frame < audio.getNumSamples(); ++frame) {
                const auto sample = audio.getSample(channel, frame);
                finite = finite && std::isfinite(sample);
                if (channel == 0 && impulseOutputFrame < 0
                    && std::abs(sample) > 0.2f)
                    impulseOutputFrame = block * 512 + frame;
            }
        }
    }

    const int latency = instance->getLatencySamples();
    const bool dryAligned = impulseOutputFrame == latency;

    const bool dryParameterFound =
        setParameter(*instance, "Dry Input Enabled", 0.0f);
    bool dryOffSilent = dryParameterFound;
    for (int block = 0; block < 2; ++block) {
        audio.clear();
        if (block == 0) {
            audio.setSample(0, 0, 0.25f);
            audio.setSample(1, 0, 0.25f);
        }
        instance->processBlock(audio, midi);
        for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
            for (int frame = 0; frame < audio.getNumSamples(); ++frame)
                dryOffSilent =
                    dryOffSilent
                    && std::abs(audio.getSample(channel, frame)) < 1.0e-6f;
        }
    }
    const bool bypassParameterFound =
        setParameter(*instance, "DPWIM Bypass", 1.0f);
    instance->releaseResources();
    instance->prepareToPlay(48000.0, 512);
    const int bypassLatency = instance->getLatencySamples();
    int bypassImpulseFrame = -1;
    float bypassImpulseValue = 0.0f;
    for (int block = 0; block < 2; ++block) {
        audio.clear();
        if (block == 0) {
            audio.setSample(0, 0, 0.25f);
            audio.setSample(1, 0, 0.25f);
        }
        instance->processBlock(audio, midi);
        for (int frame = 0; frame < audio.getNumSamples(); ++frame) {
            if (bypassImpulseFrame < 0
                && std::abs(audio.getSample(0, frame)) > 0.2f) {
                bypassImpulseFrame = block * 512 + frame;
                bypassImpulseValue = audio.getSample(0, frame);
            }
        }
    }
    const bool bypassImmediate =
        bypassParameterFound
        && bypassLatency == 0
        && bypassImpulseFrame == 0
        && std::abs(bypassImpulseValue - 0.25f) < 0.001f;
    setParameter(*instance, "DPWIM Bypass", 0.0f);
    setParameter(*instance, "Dry Input Enabled", 1.0f);
    instance->releaseResources();
    instance->prepareToPlay(48000.0, 512);
    const bool bypassLatencyRestored =
        instance->getLatencySamples() == latency;

    auto* hostBypass = instance->getBypassParameter();
    const bool hostBypassSeparated =
        hostBypass != nullptr
        && hostBypass->getName(128) != "DPWIM Bypass";
    if (hostBypass != nullptr)
        hostBypass->setValueNotifyingHost(1.0f);
    const int externalBypassLatency =
        instance->getLatencySamples();
    int externalBypassImpulseFrame = -1;
    for (int block = 0; block < 2; ++block) {
        audio.clear();
        if (block == 0) {
            audio.setSample(0, 0, 0.25f);
            audio.setSample(1, 0, 0.25f);
        }
        instance->processBlock(audio, midi);
        for (int frame = 0; frame < audio.getNumSamples(); ++frame)
            if (externalBypassImpulseFrame < 0
                && std::abs(audio.getSample(0, frame)) > 0.2f)
                externalBypassImpulseFrame = block * 512 + frame;
    }
    if (hostBypass != nullptr)
        hostBypass->setValueNotifyingHost(0.0f);
    const bool externalBypassStable =
        hostBypassSeparated
        && externalBypassLatency == latency
        && instance->getLatencySamples() == latency
        && externalBypassImpulseFrame == latency;

    bool stateRoundTrip = false;
    if (auto parameters = instance->getParameters();
        !parameters.isEmpty()) {
        auto* parameter = parameters[0];
        parameter->setValueNotifyingHost(0.25f);
        juce::MemoryBlock savedState;
        instance->getStateInformation(savedState);
        parameter->setValueNotifyingHost(0.75f);
        instance->setStateInformation(
            savedState.getData(),
            static_cast<int>(savedState.getSize()));
        stateRoundTrip =
            std::abs(parameter->getValue() - 0.25f) < 0.01f;
    }

    std::cout << format.getName() << ": "
              << descriptions[0]->name << ", latency="
              << latency << " samples, dry_impulse="
              << impulseOutputFrame << ", dry_off="
              << (dryOffSilent ? "silent" : "audible")
              << ", bypass="
              << (bypassImmediate ? "immediate" : "failed")
              << ", bypass_latency="
              << (bypassLatencyRestored ? "stable" : "failed")
              << ", host_bypass="
              << (externalBypassStable ? "separate/stable" : "failed")
              << ", state="
              << (stateRoundTrip ? "restored" : "failed") << '\n';
    instance->releaseResources();
    return finite && dryAligned && dryOffSilent && bypassImmediate
        && bypassLatencyRestored && externalBypassStable
        && stateRoundTrip && defaultsOk;
}

} // namespace

int main(int argc, char* argv[])
{
#if JUCE_PLUGINHOST_VST
    constexpr int expectedArguments = 3;
#else
    constexpr int expectedArguments = 2;
#endif
    if (argc != expectedArguments) {
        std::cerr
            << "usage: dpwim-plugin-probe <vst3 bundle>"
#if JUCE_PLUGINHOST_VST
            << " <vst2.dll>"
#endif
            << '\n';
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI juce;
    juce::VST3PluginFormat vst3;
    const bool vst3Ok = probe(vst3, juce::String(argv[1]));
#if JUCE_PLUGINHOST_VST
    juce::VSTPluginFormat vst2;
    const bool vst2Ok = probe(vst2, juce::String(argv[2]));
#else
    const bool vst2Ok = true;
#endif
    if (!vst3Ok || !vst2Ok)
        return 1;
    std::cout << "DPWIM plugin host probe passed\n";
    return 0;
}
