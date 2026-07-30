#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <mutex>
#include <thread>

namespace dpwim {

class UpdateChecker {
public:
    enum class State {
        Idle,
        Checking,
        UpToDate,
        Available,
        Failed,
    };

    struct Snapshot {
        State state = State::Idle;
        juce::String latestVersion;
        juce::String releaseUrl;
    };

    UpdateChecker() = default;
    ~UpdateChecker();

    void start(const juce::String& currentVersion);
    Snapshot snapshot() const;

    static bool isNewerVersion(
        const juce::String& currentVersion,
        const juce::String& candidateVersion);

private:
    void run(juce::String currentVersion);
    void stop() noexcept;
    void publish(Snapshot snapshot);

    mutable std::mutex snapshotMutex_;
    std::mutex requestMutex_;
    Snapshot snapshot_;
    std::thread worker_;
    std::atomic<bool> stopRequested_{false};
    std::atomic<void*> activeRequest_{nullptr};
};

} // namespace dpwim
