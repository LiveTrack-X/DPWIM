#pragma once

#include <JuceHeader.h>

#include "Audio/AudioRingBuffer.h"
#include "Audio/DryDelay.h"
#include "Platform/ProcessCatalog.h"
#include "Platform/ProcessLoopbackCapture.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>

class DPWIMAudioProcessor final : public juce::AudioProcessor,
                                  private juce::Timer {
public:
    static constexpr int kSourceSlots = 4;

    enum class SourceMode : int {
        Off = 0,
        Application = 1,
        Desktop = 2,
    };

    struct SourceSnapshot {
        SourceMode mode = SourceMode::Off;
        std::uint32_t pid = 0;
        juce::String executable;
        juce::String status;
        bool running = false;
        double fillFrames = 0.0;
        double ratio = 1.0;
    };

    DPWIMAudioProcessor();
    ~DPWIMAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override
    {
        return "DirectPipe Windows Input Mixer";
    }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& parameters() noexcept { return apvts_; }
    double currentSampleRate() const noexcept
    {
        return sampleRate_.load(std::memory_order_acquire);
    }
    std::vector<dpwim::ProcessInfo> enumerateProcesses() const;

    void setSource(int slot, SourceMode mode, std::uint32_t pid,
                   const juce::String& executable);
    SourceSnapshot sourceSnapshot(int slot) const;

private:
    struct SourceSlot {
        dpwim::AudioRingBuffer ring;
        dpwim::ProcessLoopbackCapture capture;
        std::atomic<int> mode{static_cast<int>(SourceMode::Off)};
        std::atomic<std::uint32_t> pid{0};
        std::atomic<double> fillFrames{0.0};
        std::atomic<double> ratio{1.0};
        mutable std::mutex identityMutex;
        juce::String executable;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout
    createParameterLayout();
    static juce::String gainParameter(int slot);
    static juce::String offsetParameter(int slot);

    void startSlot(int slot);
    void stopAllSlots() noexcept;
    void restoreSources(const juce::ValueTree&);
    juce::ValueTree createSourceState() const;
    void timerCallback() override;
    void updateLatencyReport();

    juce::AudioProcessorValueTreeState apvts_;
    std::atomic<float>* targetLatencyParam_ = nullptr;
    std::atomic<float>* dryGainParam_ = nullptr;
    std::array<std::atomic<float>*, kSourceSlots> sourceGainParams_{};
    std::array<std::atomic<float>*, kSourceSlots> sourceOffsetParams_{};
    std::array<std::unique_ptr<SourceSlot>, kSourceSlots> slots_;
    dpwim::DryDelay dryDelay_;
    std::atomic<bool> prepared_{false};
    std::atomic<double> sampleRate_{48000.0};
    std::atomic<int> lastReportedLatency_{-1};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DPWIMAudioProcessor)
};
