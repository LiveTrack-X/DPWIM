#include "PluginProcessor.h"

#include <JuceHeader.h>

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

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    DPWIMAudioProcessor source;

    checkValue(source, "targetLatency", 10.0f);
    checkValue(source, "dryEnabled", 1.0f);
    for (int slot = 0; slot < DPWIMAudioProcessor::kSourceSlots; ++slot)
        checkValue(
            source, "sourceGain" + juce::String(slot), 0.0f);

    source.setSource(
        1, DPWIMAudioProcessor::SourceMode::Application,
        4242, "example.exe");
    source.setSourceEnabled(1, false);
    setPlainValue(source, "targetLatency", 50.0f);
    setPlainValue(source, "dryEnabled", 0.0f);
    setPlainValue(source, "sourceGain1", -6.0f);
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
    checkValue(restored, "sourceGain1", -6.0f);

    restored.setSourceEnabled(1, true);
    check(restored.sourceSnapshot(1).enabled,
          "source can be re-enabled without reselection");

    if (failures == 0) {
        std::cout << "DPWIM processor state tests passed\n";
        return 0;
    }
    std::cerr << failures << " processor state test(s) failed\n";
    return 1;
}
