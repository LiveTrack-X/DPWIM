#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <JuceHeader.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cwctype>
#include <iostream>
#include <thread>

namespace {

void setParameter(DPWIMAudioProcessor& processor,
                  const juce::String& id, float plainValue)
{
    if (auto* parameter = processor.parameters().getParameter(id))
        parameter->setValueNotifyingHost(
            parameter->convertTo0to1(plainValue));
}

std::vector<dpwim::ProcessInfo> choosePreviewProcesses(
    const std::vector<dpwim::ProcessInfo>& processes)
{
    std::vector<dpwim::ProcessInfo> selected;
    constexpr std::array<const wchar_t*, 3> preferred{
        L"spotify", L"chrome", L"discord"};

    for (const auto* needle : preferred) {
        const auto found = std::find_if(
            processes.begin(), processes.end(),
            [needle](const dpwim::ProcessInfo& process) {
                auto name = process.displayName;
                std::transform(
                    name.begin(), name.end(), name.begin(),
                    [](wchar_t value) {
                        return static_cast<wchar_t>(
                            std::towlower(value));
                    });
                return name.find(needle) != std::wstring::npos;
            });
        if (found != processes.end())
            selected.push_back(*found);
    }

    for (const auto& process : processes) {
        if (selected.size() >= 3)
            break;
        const auto duplicate = std::any_of(
            selected.begin(), selected.end(),
            [&process](const dpwim::ProcessInfo& item) {
                return item.pid == process.pid;
            });
        if (!duplicate)
            selected.push_back(process);
    }
    return selected;
}

bool validateLatencyUi(juce::AudioProcessorEditor& editor,
                       DPWIMAudioProcessor& processor)
{
    constexpr auto baseTooltip =
        "The base delay reserved for stable app capture and input alignment. "
        "Lower values reduce latency but may increase dropouts. Negative Sync "
        "Offsets automatically increase the OUT Latency.";
    constexpr auto outputTooltip =
        "Output latency equals the Base Latency plus any automatic sync "
        "addition. The sample count is the latency reported to the host at "
        "the current sample rate. This is DPWIM latency, not end-to-end "
        "device latency.";
    constexpr auto offsetTooltip =
        "Moves this source relative to the shared timeline. Negative values "
        "make it earlier by delaying the other paths. Positive values delay "
        "only this source.";

    auto* baseLabel = dynamic_cast<juce::Label*>(
        editor.findChildWithID("baseLatencyLabel"));
    auto* baseControl = dynamic_cast<juce::Slider*>(
        editor.findChildWithID("baseLatencyControl"));
    auto* summary = dynamic_cast<juce::Label*>(
        editor.findChildWithID("latencySummary"));
    auto* sampleRate = dynamic_cast<juce::Label*>(
        editor.findChildWithID("sampleRateContext"));
    auto* version = dynamic_cast<juce::Label*>(
        editor.findChildWithID("versionContext"));
    auto* offset = dynamic_cast<juce::Slider*>(
        editor.findChildWithID("sourceSyncOffset0"));
    auto* refresh =
        editor.findChildWithID("refreshApps");
    if (baseLabel == nullptr || baseControl == nullptr
        || summary == nullptr || sampleRate == nullptr
        || version == nullptr || offset == nullptr
        || refresh == nullptr)
        return false;

    const auto latency = processor.latencySnapshot();
    const auto expected =
        DPWIMAudioProcessorEditor::formatLatencySummary(
            latency.effectiveMs, latency.syncAdditionMs,
            latency.samples);
    return baseLabel->getText() == "Base Latency"
        && baseControl->getTooltip() == baseTooltip
        && summary->getText() == expected
        && summary->getTooltip() == outputTooltip
        && offset->getTooltip() == offsetTooltip
        && offset->getHeight() >= 80
        && offset->getHeight() <= 132
        && editor.getLocalBounds().contains(summary->getBounds())
        && editor.getLocalBounds().contains(sampleRate->getBounds())
        && editor.getLocalBounds().contains(version->getBounds())
        && summary->getBounds().getCentreY()
            == sampleRate->getBounds().getCentreY()
        && summary->getBounds().getCentreY()
            == version->getBounds().getCentreY()
        && summary->getBounds().getCentreX()
            > editor.getWidth() / 2
        && sampleRate->getBounds().getCentreX()
            < editor.getWidth() / 2
        && !summary->getBounds().intersects(refresh->getBounds());
}

bool validateBranding(juce::AudioProcessorEditor& editor)
{
    auto* brand = dynamic_cast<juce::Label*>(
        editor.findChildWithID("brandMark"));
    auto* product = dynamic_cast<juce::Label*>(
        editor.findChildWithID("productName"));
    auto* refresh = editor.findChildWithID("refreshApps");
    if (brand == nullptr || product == nullptr || refresh == nullptr)
        return false;

    return brand->getText() == "DirectPipe"
        && product->getText() == "Windows Input Mixer (DPWIM)"
        && editor.getLocalBounds().contains(brand->getBounds())
        && editor.getLocalBounds().contains(product->getBounds())
        && !brand->getBounds().intersects(product->getBounds())
        && !product->getBounds().intersects(refresh->getBounds());
}

bool validateBypassUi(
    juce::AudioProcessorEditor& editor, bool expectBypassed)
{
    constexpr auto bypassTooltip =
        "Immediately passes the raw host input at unity while muting captured "
        "app sources. DPWIM output latency becomes 0 samples; host and device "
        "buffer latency is not included.";
    auto* bypass = dynamic_cast<juce::TextButton*>(
        editor.findChildWithID("globalBypass"));
    auto* refresh = editor.findChildWithID("refreshApps");
    if (bypass == nullptr || refresh == nullptr)
        return false;

    return bypass->getToggleState() == expectBypassed
        && bypass->getButtonText()
            == (expectBypassed ? "BYPASSED" : "BYPASS")
        && bypass->getTooltip() == bypassTooltip
        && editor.getLocalBounds().contains(bypass->getBounds())
        && bypass->getBounds().getRight()
            <= refresh->getBounds().getX()
        && refresh->getBounds().getX()
                - bypass->getBounds().getRight()
            <= 8
        && bypass->getBounds().getCentreY()
            == refresh->getBounds().getCentreY();
}

bool validateLevelMeters(juce::AudioProcessorEditor& editor)
{
    auto* dryMeter = dynamic_cast<DPWIMLevelMeter*>(
        editor.findChildWithID("dryLevelMeter"));
    auto* baseControl = editor.findChildWithID("baseLatencyControl");
    auto* version = editor.findChildWithID("versionContext");
    auto* sampleRate = editor.findChildWithID("sampleRateContext");
    auto* latency = editor.findChildWithID("latencySummary");
    auto* footerLink = editor.findChildWithID("createdByLiveTrack");
    if (dryMeter == nullptr || baseControl == nullptr
        || version == nullptr || sampleRate == nullptr
        || latency == nullptr || footerLink == nullptr)
        return false;

    const auto dryBounds = dryMeter->getBounds();
    if (dryMeter->getTooltip()
            != "Dry Input post-gain stereo peak level in dBFS."
        || dryBounds.getHeight() != 32
        || dryBounds.getY() < baseControl->getBounds().getBottom()
        || !editor.getLocalBounds().contains(dryBounds))
        return false;

    const std::array<juce::Component*, 4> footerComponents{
        version, sampleRate, latency, footerLink};
    for (const auto* footer : footerComponents)
        if (dryBounds.intersects(footer->getBounds()))
            return false;

    for (int index = 0;
         index < DPWIMAudioProcessor::kSourceSlots; ++index) {
        auto* meter = dynamic_cast<DPWIMLevelMeter*>(
            editor.findChildWithID(
                "sourceLevelMeter" + juce::String(index)));
        auto* offset = editor.findChildWithID(
            "sourceSyncOffset" + juce::String(index));
        if (meter == nullptr || offset == nullptr)
            return false;
        const auto meterBounds = meter->getBounds();
        if (meter->getTooltip()
                != "Source post-gain stereo peak level in dBFS."
            || meterBounds.getHeight() != 32
            || meterBounds.getWidth() < 100
            || meterBounds.getY() < offset->getBounds().getBottom()
            || meterBounds.getCentreY() != dryBounds.getCentreY()
            || !editor.getLocalBounds().contains(meterBounds))
            return false;
        for (const auto* footer : footerComponents)
            if (meterBounds.intersects(footer->getBounds()))
                return false;
    }

    auto* mainLabel = dynamic_cast<juce::Label*>(
        editor.findChildWithID("mainOutputLabel"));
    auto* mainMeter = dynamic_cast<DPWIMLevelMeter*>(
        editor.findChildWithID("mainOutputMeter"));
    auto* sourceFour = editor.findChildWithID("sourceLevelMeter3");
    if (mainLabel == nullptr || mainMeter == nullptr
        || sourceFour == nullptr)
        return false;
    const auto mainBounds = mainMeter->getBounds();
    if (mainLabel->getText() != "MAIN\nOUT"
        || mainMeter->getTooltip()
            != "Final post-mix stereo output peak level in dBFS."
        || mainBounds.getWidth() < 30
        || mainBounds.getHeight() < 300
        || mainBounds.getX() <= sourceFour->getBounds().getRight()
        || mainBounds.getY() < mainLabel->getBounds().getBottom()
        || !editor.getLocalBounds().contains(mainBounds)
        || mainBounds.intersects(footerLink->getBounds()))
        return false;
    return true;
}

void primeLevelMetersForPreview(
    juce::AudioProcessorEditor& editor, bool bypassed)
{
    if (auto* dry = dynamic_cast<DPWIMLevelMeter*>(
            editor.findChildWithID("dryLevelMeter")))
        dry->setLevels(0.42f, 0.28f);
    if (auto* mainOutput = dynamic_cast<DPWIMLevelMeter*>(
            editor.findChildWithID("mainOutputMeter")))
        mainOutput->setLevels(
            bypassed ? 0.42f : 0.86f,
            bypassed ? 0.28f : 0.64f);

    if (bypassed)
        return;
    constexpr std::array<std::array<float, 2>, 2> previewLevels{{
        {0.72f, 0.48f},
        {0.31f, 0.22f},
    }};
    for (std::size_t index = 0; index < previewLevels.size(); ++index)
        if (auto* meter = dynamic_cast<DPWIMLevelMeter*>(
                editor.findChildWithID(
                    "sourceLevelMeter"
                    + juce::String(static_cast<int>(index)))))
            meter->setLevels(
                previewLevels[index][0], previewLevels[index][1]);
}

bool validateFooter(
    juce::AudioProcessorEditor& editor, bool expectUpdate)
{
    auto* link = dynamic_cast<juce::HyperlinkButton*>(
        editor.findChildWithID("createdByLiveTrack"));
    if (link == nullptr
        || !editor.getLocalBounds().contains(link->getBounds()))
        return false;
    const bool updateContract =
        link->getButtonText()
            == "Update available | Created by LiveTrack"
        && link->getURL().toString(false).contains(
            "/DPWIM/releases/")
        && link->findColour(
               juce::HyperlinkButton::textColourId)
            == juce::Colour(0xffffa726);
    if (expectUpdate)
        return updateContract
            && link->getURL().toString(false).contains(
                "/tag/v99.0.0")
            && link->getJustificationType()
                == juce::Justification::centredRight
            && link->getBounds().getRight()
                == editor.getWidth() - 18;
    const bool currentContract =
        link->getButtonText() == "Created by LiveTrack"
        && link->getURL().toString(false)
            == "https://github.com/LiveTrack-X/DPWIM"
        && link->findColour(
               juce::HyperlinkButton::textColourId)
            == juce::Colour(0xff6e787e)
        && link->getJustificationType()
            == juce::Justification::centredRight
        && link->getBounds().getRight()
            == editor.getWidth() - 18;
    return currentContract || updateContract;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 2 && argc != 3 && argc != 4 && argc != 5) {
        std::cerr
            << "usage: dpwim-editor-snapshot <output.png> "
               "[width height] [advanced|update]\n";
        return 2;
    }
    const bool customSize = argc == 4 || argc == 5;
    const auto mode = argc == 3
        ? juce::String(argv[2])
        : argc == 5 ? juce::String(argv[4]) : juce::String{};
    const bool showAdvanced =
        mode == "advanced";
    const bool showUpdate = mode == "update";
    const bool showBypass = mode == "bypass";
    const int width = customSize
        ? juce::jlimit(840, 1280, std::atoi(argv[2]))
        : 1000;
    const int height = customSize
        ? juce::jlimit(520, 800, std::atoi(argv[3]))
        : 620;

    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    DPWIMAudioProcessor processor;
    const auto selected =
        choosePreviewProcesses(processor.enumerateProcesses());
    const std::array<float, 3> gains{-6.0f, -12.0f, 3.0f};
    const std::array<float, 3> offsets{15.0f, -25.0f, 8.0f};

    for (int index = 0;
         index < 3 && index < static_cast<int>(selected.size());
         ++index) {
        processor.setSource(
            index, DPWIMAudioProcessor::SourceMode::Application,
            selected[static_cast<std::size_t>(index)].pid,
            juce::String(
                selected[static_cast<std::size_t>(index)]
                    .executable.c_str()));
        setParameter(processor,
                     "sourceGain" + juce::String(index),
                     gains[static_cast<std::size_t>(index)]);
        setParameter(processor,
                     "sourceOffset" + juce::String(index),
                     offsets[static_cast<std::size_t>(index)]);
    }
    if (selected.size() >= 3)
        processor.setSourceEnabled(2, false);
    if (showBypass)
        setParameter(processor, "bypass", 1.0f);

    processor.prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(
        processor.createEditor());
    editor->setSize(width, height);
    editor->setVisible(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    juce::Timer::callPendingTimersSynchronously();
    if (!validateLatencyUi(*editor, processor)) {
        std::cerr << "Latency UI contract failed\n";
        editor.reset();
        processor.releaseResources();
        return 1;
    }
    if (!validateBranding(*editor)) {
        std::cerr << "Branding UI contract failed\n";
        editor.reset();
        processor.releaseResources();
        return 1;
    }
    if (!validateBypassUi(*editor, showBypass)) {
        std::cerr << "Bypass UI contract failed\n";
        editor.reset();
        processor.releaseResources();
        return 1;
    }
    if (!validateLevelMeters(*editor)) {
        std::cerr << "Level meter UI contract failed\n";
        editor.reset();
        processor.releaseResources();
        return 1;
    }
    if (showUpdate) {
        if (auto* dpwimEditor =
                dynamic_cast<DPWIMAudioProcessorEditor*>(
                    editor.get()))
            dpwimEditor->showAvailableUpdateForTesting("v99.0.0");
    }
    if (!validateFooter(*editor, showUpdate)) {
        std::cerr << "Footer contract failed\n";
        editor.reset();
        processor.releaseResources();
        return 1;
    }
    if (showAdvanced) {
        if (auto* dpwimEditor =
                dynamic_cast<DPWIMAudioProcessorEditor*>(
                    editor.get()))
            dpwimEditor->showAdvancedForTesting(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        juce::Timer::callPendingTimersSynchronously();
    }
    primeLevelMetersForPreview(*editor, showBypass);

    const auto snapshot = editor->createComponentSnapshot(
        editor->getLocalBounds(), true, 1.0f);
    juce::File output(
        juce::String::fromUTF8(argv[1]));
    output.getParentDirectory().createDirectory();
    output.deleteFile();
    juce::FileOutputStream stream(output);
    if (!stream.openedOk()) {
        std::cerr << "Could not open output file\n";
        processor.releaseResources();
        return 1;
    }

    juce::PNGImageFormat png;
    const bool written = png.writeImageToStream(snapshot, stream);
    stream.flush();
    editor.reset();
    processor.releaseResources();
    if (!written) {
        std::cerr << "Could not encode snapshot\n";
        return 1;
    }

    std::cout << output.getFullPathName() << '\n';
    return 0;
}
