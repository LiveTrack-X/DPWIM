#include "PluginEditor.h"

#include <juce_gui_basics/detail/juce_WindowingHelpers.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr auto background = 0xff11171b;
constexpr auto raised = 0xff182126;
constexpr auto control = 0xff1d272d;
constexpr auto border = 0xff4a555c;
constexpr auto divider = 0xff39434a;
constexpr auto foreground = 0xffeef3f5;
constexpr auto secondary = 0xffb7c0c5;
constexpr auto disabled = 0xff6e787e;
constexpr auto accent = 0xff41c7f2;

void configureLabel(juce::Label& label, const juce::String& text,
                    float size, int style,
                    juce::Justification justification)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(size, style));
    label.setJustificationType(justification);
    label.setColour(juce::Label::textColourId,
                    juce::Colour(foreground));
    label.setInterceptsMouseClicks(false, false);
}

void configureRotary(juce::Slider& slider, const juce::String& suffix,
                     int textWidth)
{
    slider.setSliderStyle(
        juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(
        juce::Slider::TextBoxBelow, false, textWidth, 28);
    slider.setTextValueSuffix(suffix);
    slider.setDoubleClickReturnValue(true, 0.0);
    slider.setMouseDragSensitivity(180);
    slider.setColour(juce::Slider::textBoxTextColourId,
                     juce::Colour(accent));
    slider.setColour(juce::Slider::textBoxBackgroundColourId,
                     juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxOutlineColourId,
                     juce::Colours::transparentBlack);
}

class MixerLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    MixerLookAndFeel()
    {
        setColour(juce::ComboBox::backgroundColourId,
                  juce::Colour(control));
        setColour(juce::ComboBox::outlineColourId,
                  juce::Colour(border));
        setColour(juce::ComboBox::textColourId,
                  juce::Colour(foreground));
        setColour(juce::ComboBox::arrowColourId,
                  juce::Colour(foreground));
        setColour(juce::PopupMenu::backgroundColourId,
                  juce::Colour(raised));
        setColour(juce::PopupMenu::textColourId,
                  juce::Colour(foreground));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,
                  juce::Colour(accent).withAlpha(0.18f));
        setColour(juce::PopupMenu::highlightedTextColourId,
                  juce::Colour(foreground));
        setColour(juce::TextButton::buttonColourId,
                  juce::Colour(control));
        setColour(juce::TextButton::buttonOnColourId,
                  juce::Colour(raised));
        setColour(juce::TextButton::textColourOffId,
                  juce::Colour(foreground));
    }

    void drawRotarySlider(juce::Graphics& graphics, int x, int y,
                          int width, int height, float sliderPosition,
                          float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        const auto size = static_cast<float>(
            std::min(width, height)) - 12.0f;
        const auto radius = size * 0.5f;
        const auto centre = juce::Point<float>(
            static_cast<float>(x) + static_cast<float>(width) * 0.5f,
            static_cast<float>(y) + static_cast<float>(height) * 0.5f);
        const auto knobBounds =
            juce::Rectangle<float>(size, size).withCentre(centre);
        const bool enabled = slider.isEnabled();
        const auto activeColour =
            juce::Colour(enabled ? accent : disabled);
        const auto angle = rotaryStartAngle
            + sliderPosition * (rotaryEndAngle - rotaryStartAngle);

        graphics.setColour(juce::Colour(0xff0b1013));
        graphics.fillEllipse(knobBounds);
        graphics.setColour(juce::Colour(0xff263137));
        graphics.fillEllipse(knobBounds.reduced(5.0f));
        graphics.setColour(juce::Colour(border)
                               .withAlpha(enabled ? 1.0f : 0.45f));
        graphics.drawEllipse(knobBounds.reduced(0.75f), 1.5f);

        juce::Path track;
        const auto trackBounds = knobBounds.expanded(3.0f);
        track.addCentredArc(
            centre.x, centre.y,
            trackBounds.getWidth() * 0.5f,
            trackBounds.getHeight() * 0.5f,
            0.0f, rotaryStartAngle, rotaryEndAngle, true);
        graphics.setColour(juce::Colour(border).withAlpha(0.75f));
        graphics.strokePath(
            track, juce::PathStrokeType(
                       3.0f, juce::PathStrokeType::curved,
                       juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc(
            centre.x, centre.y,
            trackBounds.getWidth() * 0.5f,
            trackBounds.getHeight() * 0.5f,
            0.0f, rotaryStartAngle, angle, true);
        graphics.setColour(activeColour);
        graphics.strokePath(
            value, juce::PathStrokeType(
                       3.0f, juce::PathStrokeType::curved,
                       juce::PathStrokeType::rounded));

        const auto pointerLength = radius * 0.66f;
        const auto pointerStart = centre
            + juce::Point<float>(
                std::sin(angle) * radius * 0.16f,
                -std::cos(angle) * radius * 0.16f);
        const auto pointerEnd = centre
            + juce::Point<float>(
                std::sin(angle) * pointerLength,
                -std::cos(angle) * pointerLength);
        graphics.setColour(activeColour);
        graphics.drawLine(
            {pointerStart.x, pointerStart.y,
             pointerEnd.x, pointerEnd.y},
            std::max(2.0f, size * 0.025f));
    }

    void drawComboBox(juce::Graphics& graphics, int width, int height,
                      bool isButtonDown, int, int, int, int,
                      juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float>(
            0.5f, 0.5f,
            static_cast<float>(width) - 1.0f,
            static_cast<float>(height) - 1.0f);
        const bool enabled = box.isEnabled();
        graphics.setColour(
            juce::Colour(isButtonDown ? raised : control)
                .withAlpha(enabled ? 1.0f : 0.55f));
        graphics.fillRoundedRectangle(bounds, 5.0f);
        graphics.setColour(
            juce::Colour(border)
                .withAlpha(enabled ? 1.0f : 0.45f));
        graphics.drawRoundedRectangle(bounds, 5.0f, 1.0f);

        juce::Path arrow;
        const auto centreX = static_cast<float>(width - 18);
        const auto centreY = static_cast<float>(height) * 0.5f;
        arrow.startNewSubPath(centreX - 4.0f, centreY - 2.0f);
        arrow.lineTo(centreX, centreY + 2.0f);
        arrow.lineTo(centreX + 4.0f, centreY - 2.0f);
        graphics.setColour(
            juce::Colour(enabled ? foreground : disabled));
        graphics.strokePath(
            arrow, juce::PathStrokeType(
                       1.6f, juce::PathStrokeType::curved,
                       juce::PathStrokeType::rounded));
    }

    void positionComboBoxText(juce::ComboBox& box,
                              juce::Label& label) override
    {
        const bool hasIcon =
            static_cast<bool>(
                box.getProperties().getWithDefault(
                    "hasIcon", false));
        const int left = hasIcon ? 42 : 12;
        label.setBounds(
            left, (box.getHeight() - 22) / 2,
            box.getWidth() - left - 26, 22);
        label.setFont(juce::Font(14.0f));
        label.setMinimumHorizontalScale(0.68f);
        label.setJustificationType(
            juce::Justification::centredLeft);
    }

    void drawButtonBackground(
        juce::Graphics& graphics, juce::Button& button,
        const juce::Colour&, bool highlighted,
        bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const auto fill = down ? juce::Colour(raised)
                              : juce::Colour(control);
        graphics.setColour(
            highlighted ? fill.brighter(0.08f) : fill);
        graphics.fillRoundedRectangle(bounds, 5.0f);
        graphics.setColour(
            highlighted ? juce::Colour(accent)
                        : juce::Colour(border));
        graphics.drawRoundedRectangle(bounds, 5.0f, 1.0f);
    }

    juce::Font getTextButtonFont(juce::TextButton&,
                                 int) override
    {
        return juce::Font(15.0f);
    }
};

} // namespace

DPWIMAudioProcessorEditor::DPWIMAudioProcessorEditor(
    DPWIMAudioProcessor& processor)
    : AudioProcessorEditor(&processor)
    , processor_(processor)
    , lookAndFeel_(std::make_unique<MixerLookAndFeel>())
{
    setLookAndFeel(lookAndFeel_.get());
    setSize(1000, 620);
    setResizable(true, false);
    setResizeLimits(840, 520, 1280, 800);

    configureLabel(markLabel_, "DPWIM", 29.0f, juce::Font::bold,
                   juce::Justification::centredLeft);
    configureLabel(productLabel_, "Windows Input Mixer", 18.0f,
                   juce::Font::plain,
                   juce::Justification::centredLeft);
    configureLabel(sampleRateLabel_, "48 kHz", 14.0f,
                   juce::Font::plain,
                   juce::Justification::centred);
    sampleRateLabel_.setColour(
        juce::Label::textColourId, juce::Colour(secondary));
    addAndMakeVisible(markLabel_);
    addAndMakeVisible(productLabel_);
    addAndMakeVisible(sampleRateLabel_);
    addAndMakeVisible(refreshButton_);

    configureLabel(dryTitle_, "Dry Input", 19.0f, juce::Font::bold,
                   juce::Justification::centred);
    configureLabel(dryGainLabel_, "Gain (dB)", 14.0f,
                   juce::Font::plain,
                   juce::Justification::centred);
    configureLabel(targetLabel_, "Target Latency", 14.0f,
                   juce::Font::plain,
                   juce::Justification::centred);
    dryGainLabel_.setColour(
        juce::Label::textColourId, juce::Colour(secondary));
    targetLabel_.setColour(
        juce::Label::textColourId, juce::Colour(secondary));
    configureRotary(dryGain_, " dB", 104);
    configureRotary(targetLatency_, " ms", 104);
    addAndMakeVisible(dryTitle_);
    addAndMakeVisible(dryGainLabel_);
    addAndMakeVisible(dryGain_);
    addAndMakeVisible(targetLabel_);
    addAndMakeVisible(targetLatency_);

    auto& state = processor_.parameters();
    targetAttachment_ = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "targetLatency", targetLatency_);
    dryAttachment_ = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "dryGain", dryGain_);

    for (int index = 0; index < static_cast<int>(rows_.size()); ++index) {
        auto& row = rows_[static_cast<std::size_t>(index)];
        configureLabel(
            row.title, "Source " + juce::String(index + 1),
            19.0f, juce::Font::bold,
            juce::Justification::centred);
        configureLabel(row.applicationLabel, "Application", 14.0f,
                       juce::Font::plain,
                       juce::Justification::centred);
        configureLabel(row.status, "Off", 14.0f, juce::Font::plain,
                       juce::Justification::centred);
        configureLabel(row.gainLabel, "Gain (dB)", 14.0f,
                       juce::Font::plain,
                       juce::Justification::centred);
        configureLabel(row.offsetLabel, "Sync Offset (ms)", 14.0f,
                       juce::Font::plain,
                       juce::Justification::centred);
        for (auto* label : {
                 &row.applicationLabel, &row.gainLabel,
                 &row.offsetLabel})
            label->setColour(
                juce::Label::textColourId, juce::Colour(secondary));

        configureRotary(row.gain, " dB", 104);
        configureRotary(row.offset, " ms", 104);
        addAndMakeVisible(row.title);
        addAndMakeVisible(row.applicationLabel);
        addAndMakeVisible(row.selector);
        addAndMakeVisible(row.status);
        addAndMakeVisible(row.gainLabel);
        addAndMakeVisible(row.gain);
        addAndMakeVisible(row.offsetLabel);
        addAndMakeVisible(row.offset);
        row.gainAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment>(
            state, "sourceGain" + juce::String(index), row.gain);
        row.offsetAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment>(
            state, "sourceOffset" + juce::String(index), row.offset);
        row.selector.onChange = [this, index] {
            applySelection(index);
        };
    }

    refreshButton_.onClick = [this] { refreshProcesses(); };
    refreshProcesses();
    timerCallback();
    startTimerHz(5);
}

DPWIMAudioProcessorEditor::~DPWIMAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

juce::Rectangle<int>
DPWIMAudioProcessorEditor::contentBounds() const
{
    return getLocalBounds().reduced(18);
}

void DPWIMAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(background));

    auto content = contentBounds();
    const auto header = content.removeFromTop(56);
    graphics.setColour(juce::Colour(divider));
    graphics.drawHorizontalLine(
        header.getBottom(),
        static_cast<float>(contentBounds().getX()),
        static_cast<float>(contentBounds().getRight()));

    auto body = content;
    const int masterWidth = juce::jlimit(
        168, 226,
        static_cast<int>(
            std::round(body.getWidth() * 0.195)));
    const auto master = body.removeFromLeft(masterWidth);
    graphics.drawVerticalLine(
        master.getRight(),
        static_cast<float>(body.getY() + 10),
        static_cast<float>(body.getBottom()));

    const int channelWidth =
        body.getWidth()
        / DPWIMAudioProcessor::kSourceSlots;
    for (int index = 1;
         index < DPWIMAudioProcessor::kSourceSlots; ++index) {
        const int separatorX =
            body.getX() + channelWidth * index;
        graphics.drawVerticalLine(
            separatorX,
            static_cast<float>(body.getY() + 10),
            static_cast<float>(body.getBottom()));
    }

    graphics.setColour(juce::Colour(raised));
    graphics.fillRoundedRectangle(
        sampleRateLabel_.getBounds().toFloat(), 4.0f);
    graphics.setColour(juce::Colour(divider));
    graphics.drawRoundedRectangle(
        sampleRateLabel_.getBounds().toFloat(), 4.0f, 1.0f);

    for (const auto& row : rows_) {
        const auto bounds = row.status.getBounds();
        const auto centre = juce::Point<float>(
            static_cast<float>(bounds.getX() + 9),
            static_cast<float>(bounds.getCentreY()));
        graphics.setColour(
            juce::Colour(row.active ? accent : disabled));
        graphics.fillEllipse(
            centre.x - 3.5f, centre.y - 3.5f, 7.0f, 7.0f);
    }
}

void DPWIMAudioProcessorEditor::paintOverChildren(
    juce::Graphics& graphics)
{
    for (const auto& row : rows_) {
        if (row.selectedIcon.isNull())
            continue;
        const auto selector = row.selector.getBounds();
        graphics.drawImageWithin(
            row.selectedIcon,
            selector.getX() + 10,
            selector.getY()
                + (selector.getHeight() - 22) / 2,
            22, 22,
            juce::RectanglePlacement::centred
                | juce::RectanglePlacement::onlyReduceInSize,
            false);
    }
}

void DPWIMAudioProcessorEditor::resized()
{
    auto content = contentBounds();
    auto header = content.removeFromTop(56);
    markLabel_.setBounds(header.removeFromLeft(128));
    header.removeFromLeft(12);
    productLabel_.setBounds(header.removeFromLeft(220));
    header.removeFromLeft(10);
    sampleRateLabel_.setBounds(
        header.removeFromLeft(76).reduced(0, 11));
    refreshButton_.setBounds(
        header.removeFromRight(132).reduced(0, 8));

    auto body = content;
    const int masterWidth = juce::jlimit(
        168, 226,
        static_cast<int>(
            std::round(body.getWidth() * 0.195)));
    auto master = body.removeFromLeft(masterWidth).reduced(14, 10);
    dryTitle_.setBounds(master.removeFromTop(34));
    master.removeFromTop(8);
    dryGainLabel_.setBounds(master.removeFromTop(22));
    dryGain_.setBounds(master.removeFromTop(
        juce::jlimit(150, 200, master.getHeight() / 2)));
    master.removeFromTop(18);
    targetLabel_.setBounds(master.removeFromTop(24));
    targetLatency_.setBounds(master.removeFromTop(
        std::min(142, master.getHeight())));

    const int channelWidth =
        body.getWidth()
        / DPWIMAudioProcessor::kSourceSlots;
    for (int index = 0;
         index < DPWIMAudioProcessor::kSourceSlots; ++index) {
        auto strip = index
            == DPWIMAudioProcessor::kSourceSlots - 1
            ? body
            : body.removeFromLeft(channelWidth);
        auto& row = rows_[static_cast<std::size_t>(index)];
        auto inner = strip.reduced(13, 10);
        row.title.setBounds(inner.removeFromTop(34));
        row.applicationLabel.setBounds(inner.removeFromTop(22));
        row.selector.setBounds(
            inner.removeFromTop(42).reduced(0, 3));
        row.status.setBounds(
            inner.removeFromTop(32).reduced(18, 2));
        row.gainLabel.setBounds(inner.removeFromTop(24));
        row.gain.setBounds(inner.removeFromTop(
            juce::jlimit(142, 178, inner.getHeight() / 2)));
        inner.removeFromTop(12);
        row.offsetLabel.setBounds(inner.removeFromTop(24));
        row.offset.setBounds(inner);
    }
}

void DPWIMAudioProcessorEditor::refreshProcesses()
{
    processes_ = processor_.enumerateProcesses();
    for (int rowIndex = 0; rowIndex < static_cast<int>(rows_.size());
         ++rowIndex) {
        auto& selector =
            rows_[static_cast<std::size_t>(rowIndex)].selector;
        const auto snapshot = processor_.sourceSnapshot(rowIndex);
        auto& row = rows_[static_cast<std::size_t>(rowIndex)];
        row.selectedIcon = {};
        selector.getProperties().set("hasIcon", false);
        selector.clear(juce::dontSendNotification);
        selector.addItem("Off", 1);
        selector.addItem("Desktop (exclude host)", 2);
        int selectedId = snapshot.mode
                                 == DPWIMAudioProcessor::SourceMode::Desktop
                             ? 2
                             : 1;
        for (std::size_t index = 0; index < processes_.size(); ++index) {
            const int itemId = 100 + static_cast<int>(index);
            juce::String display(
                processes_[index].displayName.c_str());
            const auto duplicateCount = std::count_if(
                processes_.begin(), processes_.end(),
                [&process = processes_[index]](
                    const dpwim::ProcessInfo& candidate) {
                    return candidate.displayName
                        == process.displayName;
                });
            if (duplicateCount > 1)
                display += " [" + juce::String(processes_[index].pid)
                    + "]";
            selector.addItem(display, itemId);
            if (snapshot.mode
                    == DPWIMAudioProcessor::SourceMode::Application
                && snapshot.pid == processes_[index].pid) {
                selectedId = itemId;
                const auto& path = processes_[index].path;
                if (!path.empty()) {
                    row.selectedIcon =
                        juce::detail::WindowingHelpers::
                            createIconForFile(
                                juce::File(
                                    juce::String(path.c_str())));
                    selector.getProperties().set(
                        "hasIcon", !row.selectedIcon.isNull());
                }
            }
        }
        selector.setSelectedId(
            selectedId, juce::dontSendNotification);
    }
}

void DPWIMAudioProcessorEditor::applySelection(int row)
{
    const int selected =
        rows_[static_cast<std::size_t>(row)]
            .selector.getSelectedId();
    if (selected == 1) {
        processor_.setSource(
            row, DPWIMAudioProcessor::SourceMode::Off, 0, {});
    } else if (selected == 2) {
        processor_.setSource(
            row, DPWIMAudioProcessor::SourceMode::Desktop, 0,
            "Desktop");
    } else if (selected >= 100) {
        const auto index =
            static_cast<std::size_t>(selected - 100);
        if (index < processes_.size()) {
            processor_.setSource(
                row, DPWIMAudioProcessor::SourceMode::Application,
                processes_[index].pid,
                juce::String(
                    processes_[index].executable.c_str()));
        }
    }
    timerCallback();
}

void DPWIMAudioProcessorEditor::timerCallback()
{
    const auto rate = processor_.currentSampleRate();
    sampleRateLabel_.setText(
        juce::String(static_cast<int>(
            std::llround(rate / 1000.0)))
            + " kHz",
        juce::dontSendNotification);

    for (int index = 0;
         index < static_cast<int>(rows_.size()); ++index) {
        auto& row = rows_[static_cast<std::size_t>(index)];
        const auto snapshot = processor_.sourceSnapshot(index);
        const bool enabled =
            snapshot.mode != DPWIMAudioProcessor::SourceMode::Off;
        row.active = snapshot.running;
        row.gain.setEnabled(enabled);
        row.offset.setEnabled(enabled);
        row.status.setColour(
            juce::Label::textColourId,
            juce::Colour(snapshot.running ? accent : disabled));
        row.status.setText(
            snapshot.mode == DPWIMAudioProcessor::SourceMode::Off
                ? "Off"
                : snapshot.running ? "Capturing"
                                   : snapshot.status,
            juce::dontSendNotification);
    }
    repaint();
}
