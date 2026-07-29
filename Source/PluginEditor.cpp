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
constexpr auto updateAccent = 0xffffa726;
constexpr auto clipRed = 0xffef5350;
constexpr auto baseLatencyTooltip =
    "The base delay reserved for stable app capture and input alignment. "
    "Lower values reduce latency but may increase dropouts. Negative Sync "
    "Offsets automatically increase the OUT Latency.";
constexpr auto outputLatencyTooltip =
    "Output latency equals the Base Latency plus any automatic sync addition. "
    "The sample count is the latency reported to the host at the current "
    "sample rate. This is DPWIM latency, not end-to-end device latency.";
constexpr auto syncOffsetTooltip =
    "Moves this source relative to the shared timeline. Negative values make "
    "it earlier by delaying the other paths. Positive values delay only this "
    "source.";

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
        constexpr int iconColumnWidth = 42;
        label.setBounds(
            iconColumnWidth, (box.getHeight() - 22) / 2,
            box.getWidth() - iconColumnWidth - 26, 22);
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
        const bool toggled = button.getToggleState();
        const auto activeColour =
            button.getComponentID() == "globalBypass"
            ? juce::Colour(updateAccent)
            : juce::Colour(accent);
        const auto fill = toggled
            ? activeColour.withAlpha(down ? 0.26f : 0.16f)
            : down ? juce::Colour(raised) : juce::Colour(control);
        graphics.setColour(
            highlighted ? fill.brighter(0.08f) : fill);
        graphics.fillRoundedRectangle(bounds, 5.0f);
        graphics.setColour(
            toggled || highlighted ? activeColour
                                   : juce::Colour(border));
        graphics.drawRoundedRectangle(bounds, 5.0f, 1.0f);
    }

    juce::Font getTextButtonFont(juce::TextButton&,
                                 int) override
    {
        return juce::Font(15.0f);
    }
};

class SourceAdvancedPanel final : public juce::Component {
public:
    SourceAdvancedPanel(
        DPWIMAudioProcessor& processor, int sourceIndex)
        : processor_(processor)
        , sourceIndex_(sourceIndex)
    {
        configureLabel(
            title_,
            "Source " + juce::String(sourceIndex + 1) + " Advanced",
            18.0f, juce::Font::bold,
            juce::Justification::centredLeft);
        configureLabel(
            description_,
            "Live source pitch controls.",
            13.0f, juce::Font::plain,
            juce::Justification::centredLeft);
        configureLabel(
            transposeLabel_, "Transpose (semitones)",
            13.0f, juce::Font::plain,
            juce::Justification::centredLeft);
        configureLabel(
            finePitchLabel_, "Fine Pitch (cents)",
            13.0f, juce::Font::plain,
            juce::Justification::centredLeft);
        description_.setColour(
            juce::Label::textColourId, juce::Colour(secondary));
        transposeLabel_.setColour(
            juce::Label::textColourId, juce::Colour(secondary));
        finePitchLabel_.setColour(
            juce::Label::textColourId, juce::Colour(secondary));
        for (auto* slider : {&transpose_, &finePitch_}) {
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(
                juce::Slider::TextBoxRight, false, 74, 26);
            slider->setDoubleClickReturnValue(true, 0.0);
            slider->setColour(
                juce::Slider::trackColourId, juce::Colour(accent));
            slider->setColour(
                juce::Slider::backgroundColourId,
                juce::Colour(control));
            slider->setColour(
                juce::Slider::thumbColourId, juce::Colour(accent));
            slider->setColour(
                juce::Slider::textBoxTextColourId,
                juce::Colour(foreground));
            slider->setColour(
                juce::Slider::textBoxBackgroundColourId,
                juce::Colour(control));
            slider->setColour(
                juce::Slider::textBoxOutlineColourId,
                juce::Colour(border));
        }
        transpose_.setTextValueSuffix(" st");
        finePitch_.setTextValueSuffix(" ct");
        auto& state = processor_.parameters();
        transposeAttachment_ = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment>(
            state, "sourceTranspose" + juce::String(sourceIndex_),
            transpose_);
        finePitchAttachment_ = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment>(
            state, "sourceFinePitch" + juce::String(sourceIndex_),
            finePitch_);
        resetPitch_.setButtonText("Reset Pitch");
        resetPitch_.onClick = [this] {
            auto& state = processor_.parameters();
            for (const auto& id : {
                     "sourceTranspose" + juce::String(sourceIndex_),
                     "sourceFinePitch" + juce::String(sourceIndex_)}) {
                if (auto* parameter = state.getParameter(id))
                    parameter->setValueNotifyingHost(
                        parameter->convertTo0to1(0.0f));
            }
        };
        addAndMakeVisible(title_);
        addAndMakeVisible(description_);
        addAndMakeVisible(transposeLabel_);
        addAndMakeVisible(transpose_);
        addAndMakeVisible(finePitchLabel_);
        addAndMakeVisible(finePitch_);
        addAndMakeVisible(resetPitch_);
        setSize(360, 240);
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(raised));
        graphics.setColour(juce::Colour(divider));
        graphics.drawRoundedRectangle(
            getLocalBounds().toFloat().reduced(0.5f), 7.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(16, 12);
        title_.setBounds(area.removeFromTop(28));
        description_.setBounds(area.removeFromTop(24));
        area.removeFromTop(8);
        transposeLabel_.setBounds(area.removeFromTop(20));
        transpose_.setBounds(area.removeFromTop(38));
        finePitchLabel_.setBounds(area.removeFromTop(20));
        finePitch_.setBounds(area.removeFromTop(38));
        area.removeFromTop(6);
        auto actions = area.removeFromTop(34);
        resetPitch_.setBounds(actions.removeFromLeft(112));
    }

private:
    DPWIMAudioProcessor& processor_;
    int sourceIndex_ = 0;
    juce::Label title_;
    juce::Label description_;
    juce::Label transposeLabel_;
    juce::Slider transpose_;
    juce::Label finePitchLabel_;
    juce::Slider finePitch_;
    juce::TextButton resetPitch_;
    std::unique_ptr<
        juce::AudioProcessorValueTreeState::SliderAttachment>
        transposeAttachment_;
    std::unique_ptr<
        juce::AudioProcessorValueTreeState::SliderAttachment>
        finePitchAttachment_;
};

} // namespace

void DPWIMLevelMeter::setLevels(float left, float right)
{
    const std::array<float, 2> gains{left, right};
    for (std::size_t channel = 0; channel < gains.size(); ++channel) {
        const auto gain = std::max(0.0f, gains[channel]);
        levelsDb_[channel] =
            juce::Decibels::gainToDecibels(gain, -60.0f);
        if (levelsDb_[channel] >= heldDb_[channel]) {
            heldDb_[channel] = levelsDb_[channel];
            holdTicks_[channel] = kPeakHoldTicks;
        } else if (holdTicks_[channel] > 0) {
            --holdTicks_[channel];
        } else {
            heldDb_[channel] = std::max(
                levelsDb_[channel],
                heldDb_[channel]
                    - kPeakDecayDbPerSecond
                        / static_cast<float>(kRefreshHz));
        }
        if (gain >= 1.0f)
            clipTicks_[channel] = kClipHoldTicks;
        else if (clipTicks_[channel] > 0)
            --clipTicks_[channel];
    }
    repaint();
}

void DPWIMLevelMeter::paint(juce::Graphics& graphics)
{
    if (layout_ == Layout::Vertical)
        paintVertical(graphics);
    else
        paintHorizontal(graphics);
}

void DPWIMLevelMeter::paintHorizontal(juce::Graphics& graphics)
{
    constexpr float minimumDb = -60.0f;
    auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() < 24.0f || bounds.getHeight() < 20.0f)
        return;

    graphics.setOpacity(isEnabled() ? 1.0f : 0.38f);
    constexpr float channelLabelWidth = 10.0f;
    constexpr float barHeight = 4.0f;
    constexpr float barGap = 2.0f;
    constexpr float scaleHeight = 12.0f;
    const auto barLeft = bounds.getX() + channelLabelWidth;
    const auto barWidth = bounds.getWidth() - channelLabelWidth;
    const auto barTop = bounds.getY() + 1.0f;
    const auto positionForDb =
        [barLeft, barWidth, minimumDb](float db) {
        return barLeft
            + barWidth
                * juce::jlimit(0.0f, 1.0f,
                    (db - minimumDb) / -minimumDb);
    };

    graphics.setFont(juce::Font(8.5f));
    for (std::size_t channel = 0; channel < levelsDb_.size();
         ++channel) {
        const auto y = barTop
            + static_cast<float>(channel) * (barHeight + barGap);
        graphics.setColour(juce::Colour(secondary));
        graphics.drawText(
            channel == 0 ? "L" : "R",
            juce::Rectangle<float>(
                bounds.getX(), y - 2.0f,
                channelLabelWidth - 2.0f, barHeight + 4.0f),
            juce::Justification::centredLeft, false);

        const juce::Rectangle<float> track(
            barLeft, y, barWidth, barHeight);
        graphics.setColour(juce::Colour(control));
        graphics.fillRoundedRectangle(track, 1.5f);
        const auto levelRight = positionForDb(levelsDb_[channel]);
        if (levelRight > barLeft) {
            auto fill = track.withWidth(levelRight - barLeft);
            graphics.setColour(
                juce::Colour(
                    levelsDb_[channel] >= -6.0f
                        ? updateAccent : accent));
            graphics.fillRoundedRectangle(fill, 1.5f);
        }

        const auto heldX = positionForDb(heldDb_[channel]);
        if (heldDb_[channel] > minimumDb) {
            graphics.setColour(juce::Colour(foreground));
            graphics.drawVerticalLine(
                juce::roundToInt(heldX), y, y + barHeight);
        }
        if (clipTicks_[channel] > 0) {
            graphics.setColour(juce::Colour(clipRed));
            graphics.fillRect(juce::Rectangle<float>(
                track.getRight() - 2.0f, track.getY(),
                2.0f, track.getHeight()));
        }
    }

    const auto scaleY =
        bounds.getBottom() - scaleHeight;
    const std::array<float, 5> ticks{
        -48.0f, -24.0f, -12.0f, -6.0f, 0.0f};
    const bool showMinusSix = barWidth >= 170.0f;
    for (const auto db : ticks) {
        if (db == -6.0f && !showMinusSix)
            continue;
        const auto x = positionForDb(db);
        graphics.setColour(juce::Colour(divider));
        graphics.drawVerticalLine(
            juce::roundToInt(x), scaleY, scaleY + 2.0f);
        const auto text = juce::String(static_cast<int>(db));
        juce::Rectangle<float> textBounds(
            x - 11.0f, scaleY + 1.0f, 22.0f, scaleHeight - 1.0f);
        auto justification = juce::Justification::centred;
        if (db == 0.0f) {
            textBounds.setRight(barLeft + barWidth);
            justification = juce::Justification::centredRight;
        }
        graphics.setColour(juce::Colour(disabled));
        graphics.drawText(
            text, textBounds, justification, false);
    }
}

void DPWIMLevelMeter::paintVertical(juce::Graphics& graphics)
{
    constexpr float minimumDb = -60.0f;
    auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() < 30.0f || bounds.getHeight() < 60.0f)
        return;

    graphics.setOpacity(isEnabled() ? 1.0f : 0.38f);
    constexpr float scaleWidth = 19.0f;
    constexpr float channelLabelHeight = 12.0f;
    constexpr float barGap = 3.0f;
    auto meterBounds = bounds.withTrimmedBottom(channelLabelHeight);
    const auto barsLeft = meterBounds.getX() + scaleWidth + 2.0f;
    const auto barsWidth =
        meterBounds.getRight() - barsLeft;
    const auto barWidth = (barsWidth - barGap) * 0.5f;
    const auto barTop = meterBounds.getY() + 2.0f;
    const auto barBottom = meterBounds.getBottom() - 2.0f;
    const auto barHeight = barBottom - barTop;
    const auto positionForDb =
        [barTop, barBottom, barHeight, minimumDb](float db) {
            const auto normalized = juce::jlimit(
                0.0f, 1.0f, (db - minimumDb) / -minimumDb);
            return barBottom - normalized * barHeight;
        };

    const std::array<float, 5> ticks{
        -48.0f, -24.0f, -12.0f, -6.0f, 0.0f};
    const bool showMinusSix = barHeight >= 180.0f;
    graphics.setFont(juce::Font(8.5f));
    for (const auto db : ticks) {
        if (db == -6.0f && !showMinusSix)
            continue;
        const auto y = positionForDb(db);
        graphics.setColour(juce::Colour(divider));
        graphics.drawHorizontalLine(
            juce::roundToInt(y), barsLeft - 2.0f,
            barsLeft + barsWidth);
        graphics.setColour(juce::Colour(disabled));
        graphics.drawText(
            juce::String(static_cast<int>(db)),
            juce::Rectangle<float>(
                bounds.getX(), y - 6.0f,
                scaleWidth - 2.0f, 12.0f),
            juce::Justification::centredRight, false);
    }

    for (std::size_t channel = 0; channel < levelsDb_.size();
         ++channel) {
        const auto x = barsLeft
            + static_cast<float>(channel) * (barWidth + barGap);
        const juce::Rectangle<float> track(
            x, barTop, barWidth, barHeight);
        graphics.setColour(juce::Colour(control));
        graphics.fillRoundedRectangle(track, 1.5f);

        const auto levelTop = positionForDb(levelsDb_[channel]);
        if (levelTop < barBottom) {
            const juce::Rectangle<float> fill(
                x, levelTop, barWidth, barBottom - levelTop);
            graphics.setColour(
                juce::Colour(
                    levelsDb_[channel] >= -6.0f
                        ? updateAccent : accent));
            graphics.fillRoundedRectangle(fill, 1.5f);
        }

        const auto heldY = positionForDb(heldDb_[channel]);
        if (heldDb_[channel] > minimumDb) {
            graphics.setColour(juce::Colour(foreground));
            graphics.drawHorizontalLine(
                juce::roundToInt(heldY), x, x + barWidth);
        }
        if (clipTicks_[channel] > 0) {
            graphics.setColour(juce::Colour(clipRed));
            graphics.fillRect(x, barTop, barWidth, 2.0f);
        }

        graphics.setColour(juce::Colour(secondary));
        graphics.drawText(
            channel == 0 ? "L" : "R",
            juce::Rectangle<float>(
                x, meterBounds.getBottom(),
                barWidth, channelLabelHeight),
            juce::Justification::centred, false);
    }
}

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

    configureLabel(markLabel_, "DirectPipe", 27.0f, juce::Font::bold,
                   juce::Justification::centredLeft);
    markLabel_.setMinimumHorizontalScale(0.80f);
    markLabel_.setComponentID("brandMark");
    configureLabel(versionLabel_,
                   "v" + juce::String(ProjectInfo::versionString),
                   13.0f, juce::Font::plain,
                   juce::Justification::centred);
    versionLabel_.setColour(
        juce::Label::textColourId, juce::Colour(accent));
    versionLabel_.setComponentID("versionContext");
    configureLabel(productLabel_, "Windows Input Mixer (DPWIM)", 16.0f,
                   juce::Font::plain,
                   juce::Justification::centredLeft);
    productLabel_.setMinimumHorizontalScale(0.72f);
    productLabel_.setComponentID("productName");
    configureLabel(sampleRateLabel_, "48 kHz", 14.0f,
                   juce::Font::plain,
                   juce::Justification::centred);
    sampleRateLabel_.setColour(
        juce::Label::textColourId, juce::Colour(secondary));
    sampleRateLabel_.setComponentID("sampleRateContext");
    configureLabel(
        effectiveLatencyLabel_,
        formatLatencySummary(10.0, 0.0, 480), 12.0f,
                   juce::Font::plain,
                   juce::Justification::centred);
    effectiveLatencyLabel_.setMinimumHorizontalScale(0.55f);
    effectiveLatencyLabel_.setColour(
        juce::Label::textColourId, juce::Colour(secondary));
    effectiveLatencyLabel_.setComponentID("latencySummary");
    effectiveLatencyLabel_.setTooltip(outputLatencyTooltip);
    addAndMakeVisible(markLabel_);
    addAndMakeVisible(versionLabel_);
    addAndMakeVisible(productLabel_);
    addAndMakeVisible(sampleRateLabel_);
    addAndMakeVisible(effectiveLatencyLabel_);
    bypassButton_.setClickingTogglesState(true);
    bypassButton_.setComponentID("globalBypass");
    bypassButton_.setTooltip(
        "Immediately passes the raw host input at unity while muting captured "
        "app sources. DPWIM output latency becomes 0 samples; host and device "
        "buffer latency is not included.");
    bypassButton_.setColour(
        juce::TextButton::textColourOnId, juce::Colour(updateAccent));
    addAndMakeVisible(bypassButton_);
    addAndMakeVisible(refreshButton_);
    refreshButton_.setComponentID("refreshApps");

    createdByLink_.setFont(juce::Font(11.5f), false);
    createdByLink_.setColour(
        juce::HyperlinkButton::textColourId, juce::Colour(disabled));
    createdByLink_.setComponentID("createdByLiveTrack");
    addAndMakeVisible(createdByLink_);

    configureLabel(
        mainOutputLabel_, "MAIN\nOUT", 10.5f,
        juce::Font::bold, juce::Justification::centred);
    mainOutputLabel_.setColour(
        juce::Label::textColourId, juce::Colour(secondary));
    mainOutputLabel_.setComponentID("mainOutputLabel");
    mainOutputMeter_.setComponentID("mainOutputMeter");
    mainOutputMeter_.setTooltip(
        "Final post-mix stereo output peak level in dBFS.");
    addAndMakeVisible(mainOutputLabel_);
    addAndMakeVisible(mainOutputMeter_);

    configureLabel(dryTitle_, "Dry Input", 19.0f, juce::Font::bold,
                   juce::Justification::centred);
    configureLabel(dryGainLabel_, "Gain (dB)", 14.0f,
                   juce::Font::plain,
                   juce::Justification::centred);
    configureLabel(targetLabel_, "Base Latency", 14.0f,
                   juce::Font::plain,
                   juce::Justification::centred);
    targetLabel_.setComponentID("baseLatencyLabel");
    dryGainLabel_.setColour(
        juce::Label::textColourId, juce::Colour(secondary));
    targetLabel_.setColour(
        juce::Label::textColourId, juce::Colour(secondary));
    configureRotary(dryGain_, " dB", 104);
    configureRotary(targetLatency_, " ms", 104);
    targetLatency_.setComponentID("baseLatencyControl");
    targetLatency_.setTooltip(baseLatencyTooltip);
    addAndMakeVisible(dryTitle_);
    dryPower_.setClickingTogglesState(true);
    addAndMakeVisible(dryPower_);
    addAndMakeVisible(dryGainLabel_);
    addAndMakeVisible(dryGain_);
    addAndMakeVisible(targetLabel_);
    addAndMakeVisible(targetLatency_);
    dryMeter_.setComponentID("dryLevelMeter");
    dryMeter_.setTooltip(
        "Dry Input post-gain stereo peak level in dBFS.");
    addAndMakeVisible(dryMeter_);

    auto& state = processor_.parameters();
    targetAttachment_ = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "targetLatency", targetLatency_);
    dryEnabledAttachment_ = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, "dryEnabled", dryPower_);
    bypassAttachment_ = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, "bypass", bypassButton_);
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
                       juce::Justification::centredLeft);
        row.status.setBorderSize({0, 14, 0, 0});
        row.status.setMinimumHorizontalScale(0.72f);
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
        row.offset.setComponentID(
            "sourceSyncOffset" + juce::String(index));
        row.offset.setTooltip(syncOffsetTooltip);
        addAndMakeVisible(row.title);
        addAndMakeVisible(row.advanced);
        addAndMakeVisible(row.applicationLabel);
        addAndMakeVisible(row.selector);
        addAndMakeVisible(row.selectedIcon);
        addAndMakeVisible(row.power);
        addAndMakeVisible(row.status);
        addAndMakeVisible(row.gainLabel);
        addAndMakeVisible(row.gain);
        addAndMakeVisible(row.offsetLabel);
        addAndMakeVisible(row.offset);
        row.meter.setComponentID(
            "sourceLevelMeter" + juce::String(index));
        row.meter.setTooltip(
            "Source post-gain stereo peak level in dBFS.");
        addAndMakeVisible(row.meter);
        row.gainAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment>(
            state, "sourceGain" + juce::String(index), row.gain);
        row.offsetAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment>(
            state, "sourceOffset" + juce::String(index), row.offset);
        row.selector.onChange = [this, index] {
            applySelection(index);
        };
        row.power.onClick = [this, index] {
            const auto snapshot = processor_.sourceSnapshot(index);
            processor_.setSourceEnabled(index, !snapshot.enabled);
            refreshControlStateNow();
        };
        row.advanced.onClick = [this, index] {
            showAdvanced(index);
        };
        row.selectedIcon.setInterceptsMouseClicks(false, false);
        row.selectedIcon.setImagePlacement(
            juce::RectanglePlacement::centred
            | juce::RectanglePlacement::onlyReduceInSize);
    }

    dryPower_.onClick = [this] { refreshControlStateNow(); };
    bypassButton_.onClick = [this] { refreshControlStateNow(); };
    refreshButton_.onClick = [this] {
        refreshProcesses();
        refreshControlStateNow();
    };
    updateChecker_.start(ProjectInfo::versionString);
    refreshProcesses();
    refreshControlStateNow();
    startTimerHz(kMeterRefreshHz);
    resized();
}

DPWIMAudioProcessorEditor::~DPWIMAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

juce::Rectangle<int>
DPWIMAudioProcessorEditor::contentBounds() const
{
    return getLocalBounds()
        .withTrimmedBottom(26)
        .reduced(18);
}

juce::Rectangle<int>
DPWIMAudioProcessorEditor::footerBounds() const
{
    auto bounds = getLocalBounds();
    return bounds.removeFromBottom(26).reduced(18, 0);
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
    const auto footer = footerBounds();
    graphics.drawHorizontalLine(
        footer.getY(),
        static_cast<float>(footer.getX()),
        static_cast<float>(footer.getRight()));

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

    constexpr int outputStripWidth = 54;
    const auto outputStrip =
        body.removeFromRight(outputStripWidth);
    graphics.drawVerticalLine(
        outputStrip.getX(),
        static_cast<float>(outputStrip.getY() + 10),
        static_cast<float>(outputStrip.getBottom()));

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
        versionLabel_.getBounds().toFloat(), 4.0f);
    graphics.setColour(juce::Colour(divider));
    graphics.drawRoundedRectangle(
        versionLabel_.getBounds().toFloat(), 4.0f, 1.0f);

    graphics.setColour(juce::Colour(raised));
    graphics.fillRoundedRectangle(
        sampleRateLabel_.getBounds().toFloat(), 4.0f);
    graphics.setColour(juce::Colour(divider));
    graphics.drawRoundedRectangle(
        sampleRateLabel_.getBounds().toFloat(), 4.0f, 1.0f);

    graphics.setColour(juce::Colour(raised));
    graphics.fillRoundedRectangle(
        effectiveLatencyLabel_.getBounds().toFloat(), 4.0f);
    graphics.setColour(juce::Colour(divider));
    graphics.drawRoundedRectangle(
        effectiveLatencyLabel_.getBounds().toFloat(), 4.0f, 1.0f);

    for (const auto& row : rows_) {
        const auto bounds = row.status.getBounds();
        const auto centre = juce::Point<float>(
            static_cast<float>(bounds.getX() + 5),
            static_cast<float>(bounds.getCentreY()));
        graphics.setColour(
            juce::Colour(row.active ? accent : disabled));
        graphics.fillEllipse(
            centre.x - 3.5f, centre.y - 3.5f, 7.0f, 7.0f);
    }
}

void DPWIMAudioProcessorEditor::resized()
{
    auto content = contentBounds();
    auto header = content.removeFromTop(56);
    const bool compactHeader = getWidth() < 960;
    markLabel_.setBounds(
        header.removeFromLeft(compactHeader ? 140 : 150));
    header.removeFromLeft(compactHeader ? 8 : 12);
    productLabel_.setBounds(
        header.removeFromLeft(compactHeader ? 260 : 300));
    refreshButton_.setBounds(
        header.removeFromRight(compactHeader ? 116 : 132)
            .reduced(0, 8));
    header.removeFromRight(compactHeader ? 6 : 8);
    bypassButton_.setBounds(
        header.removeFromRight(compactHeader ? 84 : 92)
            .reduced(0, 8));

    auto body = content;
    const int masterWidth = juce::jlimit(
        168, 226,
        static_cast<int>(
            std::round(body.getWidth() * 0.195)));
    auto master = body.removeFromLeft(masterWidth).reduced(14, 10);
    dryMeter_.setBounds(master.removeFromBottom(32));
    master.removeFromBottom(4);
    auto dryHeader = master.removeFromTop(32);
    dryPower_.setBounds(
        dryHeader.removeFromRight(52).reduced(0, 2));
    dryHeader.removeFromRight(4);
    dryTitle_.setBounds(dryHeader);
    master.removeFromTop(6);
    dryGainLabel_.setBounds(master.removeFromTop(20));
    dryGain_.setBounds(master.removeFromTop(
        juce::jlimit(120, 200, master.getHeight() / 2)));
    master.removeFromTop(8);
    targetLabel_.setBounds(master.removeFromTop(22));
    targetLatency_.setBounds(master.removeFromTop(
        std::min(142, master.getHeight())));

    constexpr int outputStripWidth = 54;
    auto outputStrip =
        body.removeFromRight(outputStripWidth).reduced(6, 10);
    mainOutputLabel_.setBounds(outputStrip.removeFromTop(34));
    outputStrip.removeFromTop(2);
    mainOutputMeter_.setBounds(outputStrip);

    const int channelWidth =
        body.getWidth()
        / DPWIMAudioProcessor::kSourceSlots;
    const bool compactSources = getWidth() < 960;
    for (int index = 0;
         index < DPWIMAudioProcessor::kSourceSlots; ++index) {
        auto strip = index
            == DPWIMAudioProcessor::kSourceSlots - 1
            ? body
            : body.removeFromLeft(channelWidth);
        auto& row = rows_[static_cast<std::size_t>(index)];
        auto inner = strip.reduced(compactSources ? 10 : 13, 10);
        row.meter.setBounds(inner.removeFromBottom(32));
        inner.removeFromBottom(4);
        auto titleRow = inner.removeFromTop(32);
        row.title.setFont(juce::Font(
            compactSources ? 17.0f : 19.0f, juce::Font::bold));
        row.advanced.setBounds(
            titleRow.removeFromRight(compactSources ? 42 : 46)
                .reduced(0, 3));
        titleRow.removeFromRight(compactSources ? 2 : 4);
        row.title.setBounds(titleRow);
        row.applicationLabel.setBounds(inner.removeFromTop(20));
        row.selector.setBounds(
            inner.removeFromTop(40).reduced(0, 3));
        const auto selector = row.selector.getBounds();
        row.selectedIcon.setBounds(
            selector.getX() + 10,
            selector.getY() + (selector.getHeight() - 22) / 2,
            22, 22);
        auto statusRow = inner.removeFromTop(28)
            .reduced(compactSources ? 4 : 8, 1);
        row.power.setBounds(
            statusRow.removeFromLeft(compactSources ? 48 : 52));
        statusRow.removeFromLeft(4);
        row.status.setBorderSize(
            {0, compactSources ? 10 : 14, 0, 0});
        row.status.setBounds(statusRow);
        row.gainLabel.setBounds(inner.removeFromTop(22));
        const int gainHeight = juce::jlimit(
            84, 178, inner.getHeight() - 114);
        row.gain.setBounds(inner.removeFromTop(gainHeight));
        inner.removeFromTop(8);
        row.offsetLabel.setBounds(inner.removeFromTop(22));
        row.offset.setBounds(
            inner.removeFromTop(std::min(132, inner.getHeight())));
    }

    const auto fullFooter = footerBounds().reduced(0, 3);
    auto footer = fullFooter;
    versionLabel_.setBounds(footer.removeFromLeft(58));
    if (availableVersion_.isNotEmpty()) {
        createdByLink_.setJustificationType(
            juce::Justification::centredRight);
        createdByLink_.setBounds(footer.removeFromRight(222));
    } else {
        createdByLink_.setJustificationType(
            juce::Justification::centredRight);
        createdByLink_.setBounds(footer.removeFromRight(118));
    }
    auto audioContext = fullFooter.withSizeKeepingCentre(
        compactHeader ? 278 : 310, fullFooter.getHeight());
    sampleRateLabel_.setBounds(
        audioContext.removeFromLeft(compactHeader ? 62 : 68));
    audioContext.removeFromLeft(6);
    effectiveLatencyLabel_.setBounds(audioContext);
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
        row.selectedIcon.setImage({});
        selector.clear(juce::dontSendNotification);
        selector.addItem("Select source", 1);
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
                    row.selectedIcon.setImage(
                        juce::detail::WindowingHelpers::
                            createIconForFile(
                                juce::File(
                                    juce::String(path.c_str()))));
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
    refreshProcesses();
    refreshControlStateNow();
}

void DPWIMAudioProcessorEditor::showAdvanced(int row)
{
    if (!juce::isPositiveAndBelow(
            row, static_cast<int>(rows_.size())))
        return;
    auto content = std::make_unique<SourceAdvancedPanel>(
        processor_, row);
    const auto& button =
        rows_[static_cast<std::size_t>(row)].advanced;
    juce::CallOutBox::launchAsynchronously(
        std::move(content),
        getLocalArea(&button, button.getLocalBounds()),
        this);
}

void DPWIMAudioProcessorEditor::showAdvancedForTesting(int row)
{
    if (!juce::isPositiveAndBelow(
            row, static_cast<int>(rows_.size())))
        return;
    testingAdvancedPanel_ =
        std::make_unique<SourceAdvancedPanel>(processor_, row);
    addAndMakeVisible(*testingAdvancedPanel_);
    const auto anchor =
        rows_[static_cast<std::size_t>(row)].advanced.getBounds();
    const auto x = juce::jlimit(
        8, getWidth() - testingAdvancedPanel_->getWidth() - 8,
        anchor.getCentreX() - 180);
    const auto y = juce::jlimit(
        8, getHeight() - testingAdvancedPanel_->getHeight() - 8,
        anchor.getBottom() + 10);
    testingAdvancedPanel_->setTopLeftPosition(x, y);
    testingAdvancedPanel_->toFront(false);
}

void DPWIMAudioProcessorEditor::showAvailableUpdate(
    const juce::String& latestVersion,
    const juce::String& releaseUrl)
{
    if (latestVersion.isEmpty()
        || latestVersion == availableVersion_)
        return;
    availableVersion_ = latestVersion;
    createdByLink_.setButtonText(
        "Update available | Created by LiveTrack");
    createdByLink_.setURL(juce::URL(
        releaseUrl.isNotEmpty()
            ? releaseUrl
            : "https://github.com/LiveTrack-X/DPWIM/releases/latest"));
    createdByLink_.setTooltip(
        latestVersion
        + " is available. Open this GitHub release.");
    createdByLink_.setColour(
        juce::HyperlinkButton::textColourId,
        juce::Colour(updateAccent));
    resized();
    repaint();
}

void DPWIMAudioProcessorEditor::showAvailableUpdateForTesting(
    const juce::String& latestVersion)
{
    showAvailableUpdate(
        latestVersion,
        "https://github.com/LiveTrack-X/DPWIM/releases/tag/"
            + latestVersion);
}

juce::String DPWIMAudioProcessorEditor::formatLatencySummary(
    double outputMs, double syncAdditionMs, int samples)
{
    return "OUT " + juce::String(outputMs, 1)
        + " ms | SYNC +" + juce::String(syncAdditionMs, 1)
        + " ms | " + juce::String(samples) + " smp";
}

void DPWIMAudioProcessorEditor::timerCallback()
{
    const auto levels = processor_.levelSnapshot();
    dryMeter_.setLevels(levels.dry[0], levels.dry[1]);
    mainOutputMeter_.setLevels(
        levels.mainOutput[0], levels.mainOutput[1]);
    for (int index = 0;
         index < static_cast<int>(rows_.size()); ++index) {
        auto& row = rows_[static_cast<std::size_t>(index)];
        row.meter.setLevels(
            levels.sources[static_cast<std::size_t>(index)][0],
            levels.sources[static_cast<std::size_t>(index)][1]);
    }

    if (controlRefreshCountdown_ > 0) {
        --controlRefreshCountdown_;
        return;
    }
    controlRefreshCountdown_ =
        kMeterRefreshHz / kControlRefreshHz - 1;

    if (!updateCheckHandled_) {
        const auto update = updateChecker_.snapshot();
        if (update.state == dpwim::UpdateChecker::State::Available)
            showAvailableUpdate(
                update.latestVersion, update.releaseUrl);
        updateCheckHandled_ =
            update.state == dpwim::UpdateChecker::State::Available
            || update.state == dpwim::UpdateChecker::State::UpToDate
            || update.state == dpwim::UpdateChecker::State::Failed;
    }

    const auto rate = processor_.currentSampleRate();
    sampleRateLabel_.setText(
        juce::String(static_cast<int>(
            std::llround(rate / 1000.0)))
            + " kHz",
        juce::dontSendNotification);

    const auto latency = processor_.latencySnapshot();
    effectiveLatencyLabel_.setText(
        formatLatencySummary(
            latency.effectiveMs, latency.syncAdditionMs,
            latency.samples),
        juce::dontSendNotification);
    effectiveLatencyLabel_.setColour(
        juce::Label::textColourId,
        juce::Colour(latency.syncAdditionMs > 0.05
                         ? accent
                         : secondary));

    const bool bypassed = bypassButton_.getToggleState();
    bypassButton_.setButtonText(bypassed ? "BYPASSED" : "BYPASS");

    const bool dryEnabled = dryPower_.getToggleState();
    dryMeter_.setEnabled(bypassed || dryEnabled);
    dryPower_.setButtonText(dryEnabled ? "ON" : "OFF");
    dryPower_.setColour(
        juce::TextButton::textColourOffId,
        juce::Colour(dryEnabled ? accent : secondary));
    dryGain_.setEnabled(dryEnabled);
    dryGainLabel_.setColour(
        juce::Label::textColourId,
        juce::Colour(dryEnabled ? secondary : disabled));

    for (int index = 0;
         index < static_cast<int>(rows_.size()); ++index) {
        auto& row = rows_[static_cast<std::size_t>(index)];
        const auto snapshot = processor_.sourceSnapshot(index);
        const bool hasSource =
            snapshot.mode != DPWIMAudioProcessor::SourceMode::Off;
        const bool enabled = hasSource && snapshot.enabled;
        row.meter.setEnabled(enabled && !bypassed);
        row.active = enabled && snapshot.running;
        row.advanced.setEnabled(hasSource);
        row.power.setEnabled(hasSource);
        row.power.setToggleState(
            enabled, juce::dontSendNotification);
        row.power.setButtonText(enabled ? "ON" : "OFF");
        row.power.setColour(
            juce::TextButton::textColourOffId,
            juce::Colour(enabled ? accent : secondary));
        row.gain.setEnabled(enabled);
        row.offset.setEnabled(enabled);
        row.status.setColour(
            juce::Label::textColourId,
            juce::Colour(snapshot.running ? accent : disabled));
        row.status.setText(
            !hasSource ? "No source"
                : !enabled ? "Disabled"
                : snapshot.running ? "Capturing"
                                   : snapshot.status,
            juce::dontSendNotification);
    }
    repaint();
}

void DPWIMAudioProcessorEditor::refreshControlStateNow()
{
    controlRefreshCountdown_ = 0;
    timerCallback();
}
