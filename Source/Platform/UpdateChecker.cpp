#include "UpdateChecker.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <string>
#include <vector>

namespace dpwim {
namespace {

struct InternetHandle {
    HINTERNET value = nullptr;

    ~InternetHandle()
    {
        if (value != nullptr)
            WinHttpCloseHandle(value);
    }
};

std::vector<int> versionParts(juce::String version)
{
    version = version.trim();
    while (version.isNotEmpty()
           && !juce::CharacterFunctions::isDigit(version[0]))
        version = version.substring(1);

    std::vector<int> parts;
    juce::String current;
    for (const auto character : version) {
        if (juce::CharacterFunctions::isDigit(character)) {
            current += character;
            continue;
        }
        if (character != '.')
            break;
        parts.push_back(current.getIntValue());
        current.clear();
    }
    if (current.isNotEmpty())
        parts.push_back(current.getIntValue());
    return parts;
}

} // namespace

UpdateChecker::~UpdateChecker()
{
    stop();
}

void UpdateChecker::start(const juce::String& currentVersion)
{
    if (worker_.joinable())
        return;
    stopRequested_.store(false, std::memory_order_release);
    publish({State::Checking, {}, {}});
    worker_ = std::thread(
        [this, currentVersion] { run(currentVersion); });
}

UpdateChecker::Snapshot UpdateChecker::snapshot() const
{
    std::lock_guard<std::mutex> lock(snapshotMutex_);
    return snapshot_;
}

bool UpdateChecker::isNewerVersion(
    const juce::String& currentVersion,
    const juce::String& candidateVersion)
{
    auto current = versionParts(currentVersion);
    auto candidate = versionParts(candidateVersion);
    const auto count = std::max(current.size(), candidate.size());
    current.resize(count, 0);
    candidate.resize(count, 0);
    return std::lexicographical_compare(
        current.begin(), current.end(),
        candidate.begin(), candidate.end());
}

void UpdateChecker::run(juce::String currentVersion)
{
    InternetHandle session{
        WinHttpOpen(
            L"DPWIM update check",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0)};
    if (session.value == nullptr) {
        publish({State::Failed, {}, {}});
        return;
    }
    WinHttpSetTimeouts(session.value, 1000, 1000, 1000, 1500);

    InternetHandle connection{
        WinHttpConnect(
            session.value, L"api.github.com",
            INTERNET_DEFAULT_HTTPS_PORT, 0)};
    if (connection.value == nullptr) {
        publish({State::Failed, {}, {}});
        return;
    }

    InternetHandle request{
        WinHttpOpenRequest(
            connection.value, L"GET",
            L"/repos/LiveTrack-X/DPWIM/releases/latest",
            nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)};
    if (request.value == nullptr) {
        publish({State::Failed, {}, {}});
        return;
    }
    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        if (stopRequested_.load(std::memory_order_acquire))
            return;
        activeRequest_.store(request.value, std::memory_order_release);
    }

    constexpr auto headers =
        L"Accept: application/vnd.github+json\r\n"
        L"X-GitHub-Api-Version: 2022-11-28\r\n";
    const bool sent = WinHttpSendRequest(
        request.value, headers, static_cast<DWORD>(-1L),
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        != FALSE;
    const bool received =
        sent && WinHttpReceiveResponse(request.value, nullptr) != FALSE;
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    const bool statusOk =
        received
        && WinHttpQueryHeaders(
               request.value,
               WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
               WINHTTP_HEADER_NAME_BY_INDEX,
               &status, &statusSize,
               WINHTTP_NO_HEADER_INDEX)
            != FALSE
        && status == 200;

    std::string response;
    while (statusOk
           && !stopRequested_.load(std::memory_order_acquire)) {
        DWORD available = 0;
        if (WinHttpQueryDataAvailable(
                request.value, &available)
                == FALSE
            || available == 0)
            break;
        if (response.size() + available > 65536)
            break;
        const auto offset = response.size();
        response.resize(offset + available);
        DWORD read = 0;
        if (WinHttpReadData(
                request.value, response.data() + offset,
                available, &read)
                == FALSE) {
            response.clear();
            break;
        }
        response.resize(offset + read);
    }

    void* expected = request.value;
    if (!activeRequest_.compare_exchange_strong(
            expected, nullptr, std::memory_order_acq_rel))
        request.value = nullptr;

    if (stopRequested_.load(std::memory_order_acquire))
        return;
    const auto parsed = juce::JSON::parse(
        juce::String::fromUTF8(
            response.data(), static_cast<int>(response.size())));
    const auto* object = parsed.getDynamicObject();
    if (!statusOk || object == nullptr) {
        publish({State::Failed, {}, {}});
        return;
    }

    const auto latest =
        object->getProperty("tag_name").toString().trim();
    const auto release =
        object->getProperty("html_url").toString().trim();
    if (latest.isEmpty() || release.isEmpty()) {
        publish({State::Failed, {}, {}});
        return;
    }
    publish({
        isNewerVersion(currentVersion, latest)
            ? State::Available
            : State::UpToDate,
        latest, release});
}

void UpdateChecker::stop() noexcept
{
    void* request = nullptr;
    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        stopRequested_.store(true, std::memory_order_release);
        request = activeRequest_.exchange(
            nullptr, std::memory_order_acq_rel);
    }
    if (request != nullptr)
        WinHttpCloseHandle(static_cast<HINTERNET>(request));
    if (worker_.joinable())
        worker_.join();
}

void UpdateChecker::publish(Snapshot snapshot)
{
    if (stopRequested_.load(std::memory_order_acquire))
        return;
    std::lock_guard<std::mutex> lock(snapshotMutex_);
    snapshot_ = std::move(snapshot);
}

} // namespace dpwim
