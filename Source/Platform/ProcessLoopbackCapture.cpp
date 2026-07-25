#include "ProcessLoopbackCapture.h"

#include <windows.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propvarutil.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <algorithm>
#include <chrono>
#include <vector>

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::FtmBase;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::ClassicCom;

namespace dpwim {
namespace {

class ActivationHandler final
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, FtmBase,
                          IActivateAudioInterfaceCompletionHandler> {
public:
    ActivationHandler()
        : event_(CreateEventW(nullptr, TRUE, FALSE, nullptr))
    {
    }

    ~ActivationHandler() override
    {
        if (event_ != nullptr)
            CloseHandle(event_);
    }

    STDMETHODIMP ActivateCompleted(
        IActivateAudioInterfaceAsyncOperation* operation) override
    {
        HRESULT activationResult = E_UNEXPECTED;
        ComPtr<IUnknown> unknown;
        result_ = operation->GetActivateResult(&activationResult, &unknown);
        if (SUCCEEDED(result_))
            result_ = activationResult;
        if (SUCCEEDED(result_))
            result_ = unknown.As(&audioClient_);
        SetEvent(event_);
        return S_OK;
    }

    HANDLE event() const noexcept { return event_; }
    HRESULT result() const noexcept { return result_; }
    ComPtr<IAudioClient> client() const noexcept { return audioClient_; }

private:
    HANDLE event_ = nullptr;
    HRESULT result_ = E_PENDING;
    ComPtr<IAudioClient> audioClient_;
};

WAVEFORMATEXTENSIBLE makeFloatStereoFormat(std::uint32_t sampleRate)
{
    WAVEFORMATEXTENSIBLE format{};
    format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    format.Format.nChannels = 2;
    format.Format.nSamplesPerSec = sampleRate;
    format.Format.wBitsPerSample = 32;
    format.Format.nBlockAlign =
        format.Format.nChannels * format.Format.wBitsPerSample / 8;
    format.Format.nAvgBytesPerSec =
        format.Format.nSamplesPerSec * format.Format.nBlockAlign;
    format.Format.cbSize =
        sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    format.Samples.wValidBitsPerSample = 32;
    format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    return format;
}

} // namespace

ProcessLoopbackCapture::ProcessLoopbackCapture() = default;

ProcessLoopbackCapture::~ProcessLoopbackCapture()
{
    stop();
}

bool ProcessLoopbackCapture::start(std::uint32_t targetPid,
                                   bool includeProcessTree,
                                   std::uint32_t sampleRate,
                                   AudioCallback callback)
{
    stop();
    if (targetPid == 0 || sampleRate < 8000 || !callback)
        return false;

    callback_ = std::move(callback);
    stopRequested_.store(false, std::memory_order_release);
    setStatus("Starting");
    worker_ = std::thread(
        [this, targetPid, includeProcessTree, sampleRate] {
            workerMain(targetPid, includeProcessTree, sampleRate);
        });
    return true;
}

void ProcessLoopbackCapture::stop() noexcept
{
    stopRequested_.store(true, std::memory_order_release);
    if (worker_.joinable())
        worker_.join();
    running_.store(false, std::memory_order_release);
    callback_ = {};
    setStatus("Off");
}

std::string ProcessLoopbackCapture::status() const
{
    std::lock_guard<std::mutex> lock(statusMutex_);
    return status_;
}

void ProcessLoopbackCapture::setStatus(std::string value)
{
    std::lock_guard<std::mutex> lock(statusMutex_);
    status_ = std::move(value);
}

void ProcessLoopbackCapture::workerMain(std::uint32_t targetPid,
                                        bool includeProcessTree,
                                        std::uint32_t sampleRate)
{
    const HRESULT comResult =
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize =
        SUCCEEDED(comResult);
    if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE) {
        setStatus("COM initialization failed");
        return;
    }

    HANDLE stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE sampleEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (stopEvent == nullptr || sampleEvent == nullptr) {
        if (stopEvent) CloseHandle(stopEvent);
        if (sampleEvent) CloseHandle(sampleEvent);
        setStatus("Event creation failed");
        if (uninitialize) CoUninitialize();
        return;
    }

    AUDIOCLIENT_ACTIVATION_PARAMS activation{};
    activation.ActivationType =
        AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    activation.ProcessLoopbackParams.TargetProcessId = targetPid;
    activation.ProcessLoopbackParams.ProcessLoopbackMode =
        includeProcessTree
            ? PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE
            : PROCESS_LOOPBACK_MODE_EXCLUDE_TARGET_PROCESS_TREE;

    PROPVARIANT parameters{};
    parameters.vt = VT_BLOB;
    parameters.blob.cbSize = sizeof(activation);
    parameters.blob.pBlobData =
        reinterpret_cast<BYTE*>(&activation);

    auto handler = Make<ActivationHandler>();
    ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
    HRESULT result = ActivateAudioInterfaceAsync(
        VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
        __uuidof(IAudioClient), &parameters, handler.Get(), &operation);

    if (SUCCEEDED(result)) {
        while (!stopRequested_.load(std::memory_order_acquire)) {
            const DWORD wait = WaitForSingleObject(handler->event(), 50);
            if (wait == WAIT_OBJECT_0)
                break;
        }
        if (stopRequested_.load(std::memory_order_acquire))
            result = E_ABORT;
        else
            result = handler->result();
    }

    ComPtr<IAudioClient> audioClient;
    ComPtr<IAudioCaptureClient> captureClient;
    if (SUCCEEDED(result)) {
        audioClient = handler->client();
        auto format = makeFloatStereoFormat(sampleRate);
        result = audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK
                | AUDCLNT_STREAMFLAGS_EVENTCALLBACK
                | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
                | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            0, 0, &format.Format, nullptr);
    }
    if (SUCCEEDED(result))
        result = audioClient->SetEventHandle(sampleEvent);
    if (SUCCEEDED(result))
        result = audioClient->GetService(IID_PPV_ARGS(&captureClient));
    if (SUCCEEDED(result))
        result = audioClient->Start();

    if (FAILED(result)) {
        setStatus("Capture activation failed: 0x"
                  + std::to_string(static_cast<unsigned long>(result)));
    } else {
        running_.store(true, std::memory_order_release);
        setStatus("Capturing");
        std::vector<float> silence;
        HANDLE waits[2] = {sampleEvent, stopEvent};

        while (!stopRequested_.load(std::memory_order_acquire)) {
            const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, 100);
            if (wait != WAIT_OBJECT_0)
                continue;

            UINT32 packetFrames = 0;
            while (SUCCEEDED(captureClient->GetNextPacketSize(&packetFrames))
                   && packetFrames > 0) {
                BYTE* bytes = nullptr;
                DWORD flags = 0;
                UINT64 devicePosition = 0;
                UINT64 qpcPosition = 0;
                UINT32 frames = 0;
                result = captureClient->GetBuffer(
                    &bytes, &frames, &flags, &devicePosition, &qpcPosition);
                if (FAILED(result))
                    break;

                const bool silent =
                    (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
                callback_(silent ? nullptr
                                 : reinterpret_cast<const float*>(bytes),
                          frames, 2, silent);
                captureClient->ReleaseBuffer(frames);
            }
        }
        audioClient->Stop();
    }

    running_.store(false, std::memory_order_release);
    if (stopEvent) CloseHandle(stopEvent);
    if (sampleEvent) CloseHandle(sampleEvent);
    if (uninitialize) CoUninitialize();
}

} // namespace dpwim
