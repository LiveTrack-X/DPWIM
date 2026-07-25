#include "ProcessCatalog.h"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cwctype>
#include <unordered_set>

namespace dpwim {
namespace {

std::wstring lower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return value;
}

std::wstring queryProcessPath(DWORD pid)
{
    HANDLE process = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr)
        return {};
    std::wstring path(32768, L'\0');
    DWORD size = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(
            process, 0, path.data(), &size)) {
        CloseHandle(process);
        return {};
    }
    CloseHandle(process);
    path.resize(size);
    return path;
}

} // namespace

std::vector<ProcessInfo> ProcessCatalog::enumerate()
{
    std::vector<ProcessInfo> result;
    const DWORD currentPid = GetCurrentProcessId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return result;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    std::unordered_set<std::wstring> seen;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID == 0 || entry.th32ProcessID == 4
                || entry.th32ProcessID == currentPid)
                continue;

            const std::wstring executable(entry.szExeFile);
            const auto key = lower(executable);
            if (executable.empty() || !seen.insert(key).second)
                continue;

            auto displayName = executable;
            const auto lowered = lower(displayName);
            if (lowered.size() > 4
                && lowered.substr(lowered.size() - 4) == L".exe")
                displayName.resize(displayName.size() - 4);

            result.push_back({
                entry.th32ProcessID,
                executable,
                displayName,
                queryProcessPath(entry.th32ProcessID)});
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

    std::sort(result.begin(), result.end(),
              [](const ProcessInfo& a, const ProcessInfo& b) {
                  return lower(a.displayName) < lower(b.displayName);
              });
    return result;
}

std::uint32_t ProcessCatalog::findPidByExecutable(
    const std::wstring& executable)
{
    const auto wanted = lower(executable);
    for (const auto& process : enumerate()) {
        if (lower(process.executable) == wanted)
            return process.pid;
    }
    return 0;
}

} // namespace dpwim
