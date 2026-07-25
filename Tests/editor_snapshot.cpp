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

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 2 && argc != 4) {
        std::cerr
            << "usage: dpwim-editor-snapshot <output.png> [width height]\n";
        return 2;
    }
    const int width = argc == 4
        ? juce::jlimit(840, 1280, std::atoi(argv[2]))
        : 1000;
    const int height = argc == 4
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

    processor.prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(
        processor.createEditor());
    editor->setSize(width, height);
    editor->setVisible(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    juce::Timer::callPendingTimersSynchronously();

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
    processor.releaseResources();
    if (!written) {
        std::cerr << "Could not encode snapshot\n";
        return 1;
    }

    std::cout << output.getFullPathName() << '\n';
    return 0;
}
