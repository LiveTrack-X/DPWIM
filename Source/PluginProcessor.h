#pragma once

#include <JuceHeader.h>

#include "Audio/AudioRingBuffer.h"
#include "Audio/DryDelay.h"
#include "Audio/SyncTimeline.h"
#include "DSP/PhaseVocoderPitchShifter.h"
#include "Platform/ProcessCatalog.h"
#include "Platform/ProcessLoopbackCapture.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>

class DPWIMAudioProcessor final : public juce::AudioProcessor,
                                  private juce::Timer,
                                  private juce::AudioProcessorValueTreeState::Listener,
                                  private juce::AsyncUpdater {
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
        bool enabled = false;
        bool running = false;
        double fillFrames = 0.0;
        double ratio = 1.0;
        std::uint64_t captureDiscontinuities = 0;
    };

    struct LatencySnapshot {
        double targetMs = 0.0;
        double syncAdditionMs = 0.0;
        double effectiveMs = 0.0;
        int samples = 0;
    };

    struct LevelSnapshot {
        std::array<float, 2> dry{};
        std::array<std::array<float, 2>, kSourceSlots> sources{};
        std::array<float, 2> mainOutput{};
    };

    DPWIMAudioProcessor();
    ~DPWIMAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(
        juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override
    {
        // Keep DPWIM's persistent in-editor bypass distinct from the
        // wrapper-provided host bypass. This lets external bypass use
        // processBlockBypassed() without changing the host latency contract.
        return nullptr;
    }

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
    LatencySnapshot latencySnapshot() const noexcept;
    LevelSnapshot levelSnapshot() const noexcept;
    std::vector<dpwim::ProcessInfo> enumerateProcesses() const;

    void setSource(int slot, SourceMode mode, std::uint32_t pid,
                   const juce::String& executable);
    void setSourceEnabled(int slot, bool enabled);
    SourceSnapshot sourceSnapshot(int slot) const;

private:
    friend struct DPWIMProcessorTestAccess;

    enum class AudioPathMode {
        Uninitialised,
        Normal,
        InternalBypass,
        HostBypass,
    };

    struct SourceSlot {
        dpwim::AudioRingBuffer ring;
        dpwim::PhaseVocoderPitchShifter pitchShifter;
        juce::AudioBuffer<float> scratch;
        dpwim::ProcessLoopbackCapture capture;
        std::atomic<int> mode{static_cast<int>(SourceMode::Off)};
        std::atomic<bool> enabled{false};
        std::atomic<std::uint32_t> pid{0};
        std::atomic<double> fillFrames{0.0};
        std::atomic<double> ratio{1.0};
        std::atomic<std::uint64_t> captureDiscontinuities{0};
        std::array<std::atomic<float>, 2> meterPeaks{};
        bool lastPitchActive = false;
        double lastRequestedFillFrames = -1.0;
        mutable std::mutex identityMutex;
        juce::String executable;
    };

    static void ingestCapturedPacket(
        SourceSlot&, const float*, std::uint32_t frames,
        std::uint32_t channels, bool silent,
        bool discontinuity) noexcept;
    static juce::AudioProcessorValueTreeState::ParameterLayout
    createParameterLayout();
    static juce::String gainParameter(int slot);
    static juce::String offsetParameter(int slot);
    static juce::String transposeParameter(int slot);
    static juce::String finePitchParameter(int slot);

    void startSlot(int slot);
    void stopAllSlots() noexcept;
    void restoreSources(const juce::ValueTree&);
    juce::ValueTree createSourceState() const;
    void timerCallback() override;
    void parameterChanged(
        const juce::String& parameterId, float newValue) override;
    void handleAsyncUpdate() override;
    void requestLatencyReportUpdate();
    void updateLatencyReport();
    dpwim::SyncTimelinePlan currentTimeline(
        int blockSafetyFrames) const noexcept;
    bool observeHostBlockSize(int frames) noexcept;
    void applyPendingBlockSafety() noexcept;
    void processAudioBlock(
        juce::AudioBuffer<float>&, bool forceBypass);
    void prepareAudioPath(
        AudioPathMode mode, int frames, double sampleRate) noexcept;
    static void updateMeterPeak(
        std::atomic<float>& meter, float peak, float release) noexcept;

    template <typename HostNotifier, typename SafetyPublisher>
    static void notifyHostThenPublishAudioSafety(
        bool hostNotificationRequired,
        HostNotifier&& notifyHost,
        bool safetyPublicationRequired,
        SafetyPublisher&& publishSafety)
    {
        // The audio path must never adopt a larger delay before the host has
        // been told to update its plug-in delay compensation.
        if (hostNotificationRequired)
            notifyHost();
        if (safetyPublicationRequired)
            publishSafety();
    }

    juce::AudioProcessorValueTreeState apvts_;
    std::atomic<float>* targetLatencyParam_ = nullptr;
    std::atomic<float>* dryEnabledParam_ = nullptr;
    std::atomic<float>* dryGainParam_ = nullptr;
    std::atomic<float>* bypassParam_ = nullptr;
    std::array<std::atomic<float>*, kSourceSlots> sourceGainParams_{};
    std::array<std::atomic<float>*, kSourceSlots> sourceOffsetParams_{};
    std::array<std::atomic<float>*, kSourceSlots> sourceTransposeParams_{};
    std::array<std::atomic<float>*, kSourceSlots> sourceFinePitchParams_{};
    std::array<std::unique_ptr<SourceSlot>, kSourceSlots> slots_;
    dpwim::DryDelay dryDelay_;
    std::array<std::atomic<float>, 2> dryMeterPeaks_{};
    std::array<std::atomic<float>, 2> mainOutputMeterPeaks_{};
    std::atomic<bool> prepared_{false};
    std::atomic<double> sampleRate_{48000.0};
    std::atomic<int> lastReportedLatency_{-1};
    std::atomic<int> blockSafetyFrames_{1};
    std::atomic<int> observedBlockSafetyFrames_{1};
    std::atomic<int> pendingCommittedBlockSafetyFrames_{0};
    std::atomic<int> realtimeBlockCapacityFrames_{1};
    AudioPathMode audioPathMode_ = AudioPathMode::Uninitialised;
    juce::int64 lastAudioBlockTick_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DPWIMAudioProcessor)
};
