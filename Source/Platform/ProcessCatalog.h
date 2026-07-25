#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dpwim {

struct ProcessInfo {
    std::uint32_t pid = 0;
    std::wstring executable;
    std::wstring displayName;
    std::wstring path;
};

class ProcessCatalog {
public:
    static std::vector<ProcessInfo> enumerate();
    static std::uint32_t findPidByExecutable(const std::wstring& executable);
};

} // namespace dpwim
