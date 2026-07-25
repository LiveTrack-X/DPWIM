#include <windows.h>
#include <mmsystem.h>

#include "Platform/ProcessLoopbackCapture.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace {

constexpr std::uint32_t kSampleRate = 48000;

bool playTestTone()
{
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = kSampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign =
        format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec =
        format.nSamplesPerSec * format.nBlockAlign;

    HWAVEOUT output = nullptr;
    if (waveOutOpen(&output, WAVE_MAPPER, &format, 0, 0,
                    CALLBACK_NULL) != MMSYSERR_NOERROR)
        return false;

    std::vector<std::int16_t> samples(kSampleRate * 2);
    for (std::uint32_t frame = 0; frame < kSampleRate; ++frame) {
        const auto value = static_cast<std::int16_t>(
            std::sin(2.0 * 3.14159265358979323846 * 440.0
                     * static_cast<double>(frame) / kSampleRate)
            * 8000.0);
        samples[static_cast<std::size_t>(frame) * 2] = value;
        samples[static_cast<std::size_t>(frame) * 2 + 1] = value;
    }

    WAVEHDR header{};
    header.lpData = reinterpret_cast<LPSTR>(samples.data());
    header.dwBufferLength =
        static_cast<DWORD>(samples.size() * sizeof(std::int16_t));
    bool ok =
        waveOutPrepareHeader(output, &header, sizeof(header))
            == MMSYSERR_NOERROR
        && waveOutWrite(output, &header, sizeof(header))
            == MMSYSERR_NOERROR;

    for (int attempt = 0; ok && attempt < 100
         && (header.dwFlags & WHDR_DONE) == 0; ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

    ok = ok && (header.dwFlags & WHDR_DONE) != 0;
    waveOutReset(output);
    waveOutUnprepareHeader(output, &header, sizeof(header));
    waveOutClose(output);
    return ok;
}

} // namespace

int main()
{
    std::atomic<std::uint64_t> capturedFrames{0};
    std::atomic<double> capturedEnergy{0.0};

    dpwim::ProcessLoopbackCapture capture;
    if (!capture.start(
            GetCurrentProcessId(), true, kSampleRate,
            [&capturedFrames, &capturedEnergy](
                const float* data, std::uint32_t frames,
                std::uint32_t channels, bool silent) {
                capturedFrames.fetch_add(frames, std::memory_order_relaxed);
                if (silent || data == nullptr)
                    return;
                double energy = 0.0;
                const auto samples =
                    static_cast<std::size_t>(frames) * channels;
                for (std::size_t index = 0; index < samples; ++index)
                    energy += std::abs(data[index]);
                double current =
                    capturedEnergy.load(std::memory_order_relaxed);
                while (!capturedEnergy.compare_exchange_weak(
                    current, current + energy,
                    std::memory_order_relaxed)) {
                }
            })) {
        std::cerr << "Could not start capture worker\n";
        return 1;
    }

    for (int attempt = 0; attempt < 100 && !capture.isRunning(); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (!capture.isRunning()) {
        std::cerr << "Capture did not activate: " << capture.status() << '\n';
        capture.stop();
        return 1;
    }

    if (!playTestTone()) {
        std::cerr << "Could not play test tone\n";
        capture.stop();
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    capture.stop();

    const auto frames = capturedFrames.load(std::memory_order_relaxed);
    const auto energy = capturedEnergy.load(std::memory_order_relaxed);
    std::cout << "captured_frames=" << frames
              << " absolute_energy=" << energy << '\n';
    if (frames == 0 || energy <= 1.0)
        return 1;

    std::cout << "DPWIM process-loopback smoke test passed\n";
    return 0;
}
