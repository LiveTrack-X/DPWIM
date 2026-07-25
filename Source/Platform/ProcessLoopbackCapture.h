#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace dpwim {

class ProcessLoopbackCapture {
public:
    using AudioCallback = std::function<void(
        const float*, std::uint32_t, std::uint32_t, bool)>;

    ProcessLoopbackCapture();
    ~ProcessLoopbackCapture();

    ProcessLoopbackCapture(const ProcessLoopbackCapture&) = delete;
    ProcessLoopbackCapture& operator=(const ProcessLoopbackCapture&) = delete;

    bool start(std::uint32_t targetPid, bool includeProcessTree,
               std::uint32_t sampleRate, AudioCallback callback);
    void stop() noexcept;

    bool isRunning() const noexcept
    {
        return running_.load(std::memory_order_acquire);
    }

    std::string status() const;

private:
    void workerMain(std::uint32_t targetPid, bool includeProcessTree,
                    std::uint32_t sampleRate);
    void setStatus(std::string value);

    AudioCallback callback_;
    std::thread worker_;
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> running_{false};
    mutable std::mutex statusMutex_;
    std::string status_{"Off"};
};

} // namespace dpwim
