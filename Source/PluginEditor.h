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
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    struct SourceStrip {
        juce::Label title;
        juce::Label applicationLabel;
        juce::ComboBox selector;
        juce::Label status;
        juce::Label gainLabel;
        juce::Slider gain;
        juce::Label offsetLabel;
        juce::Slider offset;
        juce::Image selectedIcon;
        bool active = false;
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
    juce::Rectangle<int> contentBounds() const;

    DPWIMAudioProcessor& processor_;
    std::unique_ptr<juce::LookAndFeel_V4> lookAndFeel_;

    juce::Label markLabel_;
    juce::Label versionLabel_;
    juce::Label productLabel_;
    juce::Label sampleRateLabel_;
    juce::TextButton refreshButton_{"Refresh apps"};

    juce::Label dryTitle_;
    juce::Label dryGainLabel_;
    juce::Slider dryGain_;
    juce::Label targetLabel_;
    juce::Slider targetLatency_;

    std::unique_ptr<
        juce::AudioProcessorValueTreeState::SliderAttachment>
        targetAttachment_;
    std::unique_ptr<
        juce::AudioProcessorValueTreeState::SliderAttachment>
        dryAttachment_;
    std::array<SourceStrip, DPWIMAudioProcessor::kSourceSlots> rows_;
    std::vector<dpwim::ProcessInfo> processes_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DPWIMAudioProcessorEditor)
};
