#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

#include <array>
#include <memory>

class DPWIMAudioProcessorEditor final
    : public juce::AudioProcessorEditor,
      private juce::Timer {
public:
    explicit DPWIMAudioProcessorEditor(DPWIMAudioProcessor&);
    ~DPWIMAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct SourceRow {
        juce::Label label;
        juce::ComboBox selector;
        juce::Slider gain;
        juce::Slider offset;
        juce::Label status;
        std::unique_ptr<
            juce::AudioProcessorValueTreeState::SliderAttachment>
            gainAttachment;
        std::unique_ptr<
            juce::AudioProcessorValueTreeState::SliderAttachment>
            offsetAttachment;
    };

    void timerCallback() override;
    void refreshProcesses();
    void applySelection(int row);

    DPWIMAudioProcessor& processor_;
    juce::Label title_;
    juce::Label targetLabel_;
    juce::Slider targetLatency_;
    juce::Label dryLabel_;
    juce::Slider dryGain_;
    juce::TextButton refreshButton_{"Refresh apps"};
    std::unique_ptr<
        juce::AudioProcessorValueTreeState::SliderAttachment>
        targetAttachment_;
    std::unique_ptr<
        juce::AudioProcessorValueTreeState::SliderAttachment>
        dryAttachment_;
    std::array<SourceRow, DPWIMAudioProcessor::kSourceSlots> rows_;
    std::vector<dpwim::ProcessInfo> processes_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DPWIMAudioProcessorEditor)
};
