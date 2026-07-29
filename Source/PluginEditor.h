#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Platform/UpdateChecker.h"

#include <array>
#include <memory>

class DPWIMLevelMeter final
    : public juce::Component,
      public juce::SettableTooltipClient {
public:
    enum class Layout {
        Horizontal,
        Vertical,
    };

    explicit DPWIMLevelMeter(Layout layout = Layout::Horizontal)
        : layout_(layout)
    {
    }

    void setLevels(float left, float right);
    void paint(juce::Graphics&) override;

private:
    void paintHorizontal(juce::Graphics&);
    void paintVertical(juce::Graphics&);

    Layout layout_ = Layout::Horizontal;
    std::array<float, 2> levelsDb_{{-60.0f, -60.0f}};
    std::array<float, 2> heldDb_{{-60.0f, -60.0f}};
    std::array<int, 2> holdTicks_{};
    std::array<int, 2> clipTicks_{};
};

class DPWIMAudioProcessorEditor final
    : public juce::AudioProcessorEditor,
      private juce::Timer {
public:
    explicit DPWIMAudioProcessorEditor(DPWIMAudioProcessor&);
    ~DPWIMAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void showAdvancedForTesting(int row);
    void showAvailableUpdateForTesting(
        const juce::String& latestVersion);
    static juce::String formatLatencySummary(
        double outputMs, double syncAdditionMs, int samples);

private:
    struct SourceStrip {
        juce::Label title;
        juce::TextButton advanced{"ADV"};
        juce::Label applicationLabel;
        juce::ComboBox selector;
        juce::TextButton power{"OFF"};
        juce::Label status;
        juce::Label gainLabel;
        juce::Slider gain;
        juce::Label offsetLabel;
        juce::Slider offset;
        DPWIMLevelMeter meter;
        juce::ImageComponent selectedIcon;
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
    void showAdvanced(int row);
    void showAvailableUpdate(
        const juce::String& latestVersion,
        const juce::String& releaseUrl);
    juce::Rectangle<int> contentBounds() const;
    juce::Rectangle<int> footerBounds() const;

    DPWIMAudioProcessor& processor_;
    std::unique_ptr<juce::LookAndFeel_V4> lookAndFeel_;

    juce::Label markLabel_;
    juce::Label versionLabel_;
    juce::Label productLabel_;
    juce::Label sampleRateLabel_;
    juce::Label effectiveLatencyLabel_;
    juce::Label mainOutputLabel_;
    DPWIMLevelMeter mainOutputMeter_{
        DPWIMLevelMeter::Layout::Vertical};
    juce::TextButton bypassButton_{"BYPASS"};
    juce::TextButton refreshButton_{"Refresh apps"};
    juce::HyperlinkButton createdByLink_{
        "Created by LiveTrack",
        juce::URL("https://github.com/LiveTrack-X/DPWIM")};

    juce::Label dryTitle_;
    juce::TextButton dryPower_{"ON"};
    juce::Label dryGainLabel_;
    juce::Slider dryGain_;
    juce::Label targetLabel_;
    juce::Slider targetLatency_;
    DPWIMLevelMeter dryMeter_;

    std::unique_ptr<
        juce::AudioProcessorValueTreeState::SliderAttachment>
        targetAttachment_;
    std::unique_ptr<
        juce::AudioProcessorValueTreeState::ButtonAttachment>
        dryEnabledAttachment_;
    std::unique_ptr<
        juce::AudioProcessorValueTreeState::ButtonAttachment>
        bypassAttachment_;
    std::unique_ptr<
        juce::AudioProcessorValueTreeState::SliderAttachment>
        dryAttachment_;
    std::array<SourceStrip, DPWIMAudioProcessor::kSourceSlots> rows_;
    std::vector<dpwim::ProcessInfo> processes_;
    std::unique_ptr<juce::Component> testingAdvancedPanel_;
    dpwim::UpdateChecker updateChecker_;
    juce::String availableVersion_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DPWIMAudioProcessorEditor)
};
