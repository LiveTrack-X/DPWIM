#include "PluginEditor.h"

#include <cmath>

namespace {

void configureSlider(juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 84, 22);
    slider.setTextValueSuffix(suffix);
}

} // namespace

DPWIMAudioProcessorEditor::DPWIMAudioProcessorEditor(
    DPWIMAudioProcessor& processor)
    : AudioProcessorEditor(&processor)
    , processor_(processor)
{
    setSize(860, 390);

    title_.setText("DirectPipe Windows Input Mixer (DPWIM)",
                   juce::dontSendNotification);
    title_.setFont(juce::Font(22.0f, juce::Font::bold));
    addAndMakeVisible(title_);

    targetLabel_.setText("Target", juce::dontSendNotification);
    dryLabel_.setText("Dry", juce::dontSendNotification);
    addAndMakeVisible(targetLabel_);
    addAndMakeVisible(dryLabel_);
    configureSlider(targetLatency_, " ms");
    configureSlider(dryGain_, " dB");
    addAndMakeVisible(targetLatency_);
    addAndMakeVisible(dryGain_);
    addAndMakeVisible(refreshButton_);

    auto& state = processor_.parameters();
    targetAttachment_ = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "targetLatency", targetLatency_);
    dryAttachment_ = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "dryGain", dryGain_);

    for (int index = 0; index < static_cast<int>(rows_.size()); ++index) {
        auto& row = rows_[static_cast<std::size_t>(index)];
        row.label.setText("Source " + juce::String(index + 1),
                          juce::dontSendNotification);
        configureSlider(row.gain, " dB");
        configureSlider(row.offset, " ms");
        row.status.setColour(juce::Label::textColourId,
                             juce::Colours::lightgrey);
        addAndMakeVisible(row.label);
        addAndMakeVisible(row.selector);
        addAndMakeVisible(row.gain);
        addAndMakeVisible(row.offset);
        addAndMakeVisible(row.status);
        row.gainAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment>(
            state, "sourceGain" + juce::String(index), row.gain);
        row.offsetAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment>(
            state, "sourceOffset" + juce::String(index), row.offset);
        row.selector.onChange = [this, index] { applySelection(index); };
    }

    refreshButton_.onClick = [this] { refreshProcesses(); };
    refreshProcesses();
    startTimerHz(5);
}

DPWIMAudioProcessorEditor::~DPWIMAudioProcessorEditor()
{
    stopTimer();
}

void DPWIMAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff171a20));
    graphics.setColour(juce::Colour(0xff2a303b));
    graphics.fillRoundedRectangle(
        getLocalBounds().toFloat().reduced(10.0f), 8.0f);
}

void DPWIMAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto header = area.removeFromTop(44);
    title_.setBounds(header.removeFromLeft(430));
    refreshButton_.setBounds(header.removeFromRight(120).reduced(2));

    auto controls = area.removeFromTop(42);
    targetLabel_.setBounds(controls.removeFromLeft(56));
    targetLatency_.setBounds(controls.removeFromLeft(250));
    controls.removeFromLeft(20);
    dryLabel_.setBounds(controls.removeFromLeft(40));
    dryGain_.setBounds(controls.removeFromLeft(250));
    area.removeFromTop(8);

    for (auto& row : rows_) {
        auto line = area.removeFromTop(60);
        row.label.setBounds(line.removeFromLeft(72));
        row.selector.setBounds(line.removeFromLeft(230).reduced(2, 12));
        row.gain.setBounds(line.removeFromLeft(170).reduced(2, 8));
        row.offset.setBounds(line.removeFromLeft(190).reduced(2, 8));
        row.status.setBounds(line.reduced(5, 8));
    }
}

void DPWIMAudioProcessorEditor::refreshProcesses()
{
    processes_ = processor_.enumerateProcesses();
    for (int rowIndex = 0; rowIndex < static_cast<int>(rows_.size());
         ++rowIndex) {
        auto& selector = rows_[static_cast<std::size_t>(rowIndex)].selector;
        const auto snapshot = processor_.sourceSnapshot(rowIndex);
        selector.clear(juce::dontSendNotification);
        selector.addItem("Off", 1);
        selector.addItem("Desktop (exclude host)", 2);
        int selectedId = snapshot.mode
                                 == DPWIMAudioProcessor::SourceMode::Desktop
                             ? 2
                             : 1;
        for (std::size_t index = 0; index < processes_.size(); ++index) {
            const int itemId = 100 + static_cast<int>(index);
            selector.addItem(
                juce::String(processes_[index].displayName.c_str())
                    + " [" + juce::String(processes_[index].pid) + "]",
                itemId);
            if (snapshot.mode
                    == DPWIMAudioProcessor::SourceMode::Application
                && snapshot.pid == processes_[index].pid)
                selectedId = itemId;
        }
        selector.setSelectedId(selectedId, juce::dontSendNotification);
    }
}

void DPWIMAudioProcessorEditor::applySelection(int row)
{
    const int selected =
        rows_[static_cast<std::size_t>(row)].selector.getSelectedId();
    if (selected == 1) {
        processor_.setSource(
            row, DPWIMAudioProcessor::SourceMode::Off, 0, {});
    } else if (selected == 2) {
        processor_.setSource(
            row, DPWIMAudioProcessor::SourceMode::Desktop, 0, "Desktop");
    } else if (selected >= 100) {
        const auto index = static_cast<std::size_t>(selected - 100);
        if (index < processes_.size()) {
            processor_.setSource(
                row, DPWIMAudioProcessor::SourceMode::Application,
                processes_[index].pid,
                juce::String(processes_[index].executable.c_str()));
        }
    }
}

void DPWIMAudioProcessorEditor::timerCallback()
{
    for (int index = 0; index < static_cast<int>(rows_.size()); ++index) {
        const auto snapshot = processor_.sourceSnapshot(index);
        juce::String text = snapshot.status;
        if (snapshot.running) {
            text += "  "
                + juce::String(snapshot.fillFrames, 0) + "f  x"
                + juce::String(snapshot.ratio, 5);
        }
        rows_[static_cast<std::size_t>(index)].status.setText(
            text, juce::dontSendNotification);
    }
}
