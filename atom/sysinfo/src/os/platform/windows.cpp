#include "windows.hpp"

#ifdef _WIN32

#include <array>
#include <format>
#include <memory>

#include <windows.h>

#include <spdlog/spdlog.h>
#include "os.hpp"

namespace atom::system {

auto getComputerNameWindows() -> std::optional<std::string> {
    spdlog::debug("Retrieving computer name on Windows");
    constexpr size_t bufferSize = 256;
    std::array<char, bufferSize> buffer;
    auto size = static_cast<DWORD>(buffer.size());

    if (GetComputerNameA(buffer.data(), &size)) {
        spdlog::info("Successfully retrieved computer name: {}", buffer.data());
        return std::string(buffer.data());
    } else {
        spdlog::error("Failed to get computer name on Windows");
        return std::nullopt;
    }
}

void getOperatingSystemInfoWindows(OperatingSystemInfo& osInfo) {
    spdlog::debug("Using Windows API for OS information");
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    if (GetVersionEx(reinterpret_cast<LPOSVERSIONINFO>(&osvi))) {
        osInfo.osName = "Windows";
        osInfo.osVersion = std::format("{}.{} (Build {})", osvi.dwMajorVersion,
                                       osvi.dwMinorVersion, osvi.dwBuildNumber);
        spdlog::info("Retrieved Windows OS info: {} {}", osInfo.osName,
                     osInfo.osVersion);
    } else {
        spdlog::error("Failed to get Windows OS version");
    }
}

auto getSystemUptimeWindows() -> std::chrono::seconds {
    spdlog::debug("Getting system uptime on Windows");
    return std::chrono::seconds(GetTickCount64() / 1000);
}

auto getSystemTimeZoneWindows() -> std::string {
    spdlog::debug("Getting system timezone on Windows");
    TIME_ZONE_INFORMATION tzi;
    if (GetTimeZoneInformation(&tzi) != TIME_ZONE_ID_INVALID) {
        std::wstring wstr(tzi.StandardName);
        return std::string(wstr.begin(), wstr.end());
    }
    return "Unknown";
}

auto getInstalledUpdatesWindows() -> std::vector<std::string> {
    spdlog::debug("Getting installed updates on Windows");
    std::vector<std::string> updates;

    std::array<char, 128> buffer;
    std::string command =
        "powershell -Command \"Get-HotFix | Select-Object HotFixID\"";
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(command.c_str(), "r"),
                                                   _pclose);

    if (pipe) {
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            std::string update(buffer.data());
            if (!update.empty() && update.back() == '\n') {
                update.pop_back();
            }
            if (!update.empty() &&
                update.find("HotFixID") == std::string::npos &&
                update.find("--------") == std::string::npos) {
                updates.push_back(update);
            }
        }
    }

    spdlog::info("Found {} installed updates on Windows", updates.size());
    return updates;
}

auto getSystemLanguageWindows() -> std::string {
    spdlog::debug("Getting system language on Windows");
    LCID lcid = GetSystemDefaultLCID();
    WCHAR lang[LOCALE_NAME_MAX_LENGTH];
    if (LCIDToLocaleName(lcid, lang, LOCALE_NAME_MAX_LENGTH, 0) > 0) {
        std::wstring wstr(lang);
        return std::string(wstr.begin(), wstr.end());
    }
    return "Unknown";
}

auto getSystemEncodingWindows() -> std::string {
    spdlog::debug("Getting system encoding on Windows");
    UINT codePage = GetACP();
    return std::format("CP{}", codePage);
}

auto isServerEditionWindows() -> bool {
    spdlog::debug("Checking if Windows OS is server edition");
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (GetVersionEx(reinterpret_cast<LPOSVERSIONINFO>(&osvi))) {
        return osvi.wProductType != VER_NT_WORKSTATION;
    }
    return false;
}

}  // namespace atom::system

#endif  // _WIN32
