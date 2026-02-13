#include "macos.hpp"

#ifdef __APPLE__

#include <array>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/utsname.h>

#include <spdlog/spdlog.h>
#include "os.hpp"

namespace atom::system {

auto getComputerNameMacOS() -> std::optional<std::string> {
    spdlog::debug("Retrieving computer name on macOS");
    constexpr size_t bufferSize = 256;
    std::array<char, bufferSize> buffer;

    CFStringRef name = SCDynamicStoreCopyComputerName(nullptr, nullptr);
    if (name != nullptr) {
        CFStringGetCString(name, buffer.data(), buffer.size(),
                           kCFStringEncodingUTF8);
        CFRelease(name);
        spdlog::info("Successfully retrieved computer name: {}", buffer.data());
        return std::string(buffer.data());
    } else {
        spdlog::error("Failed to get computer name on macOS");
        return std::nullopt;
    }
}

void getOperatingSystemInfoMacOS(OperatingSystemInfo& osInfo) {
    spdlog::debug("Using macOS API for OS information");
    struct utsname info;
    if (uname(&info) == 0) {
        osInfo.osName = info.sysname;
        osInfo.osVersion = info.release;
        osInfo.kernelVersion = info.version;
        spdlog::info("Retrieved macOS OS info: {} {} {}", info.sysname,
                     info.release, info.version);
    } else {
        spdlog::error("Failed to get OS info using uname");
    }
}

auto getSystemUptimeMacOS() -> std::chrono::seconds {
    spdlog::debug("Getting system uptime on macOS");
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    if (sysctl(mib, 2, &boottime, &len, nullptr, 0) == 0) {
        time_t now;
        time(&now);
        return std::chrono::seconds(now - boottime.tv_sec);
    }
    return std::chrono::seconds(0);
}

auto getSystemTimeZoneMacOS() -> std::string {
    spdlog::debug("Getting system timezone on macOS");
    // macOS timezone implementation would go here
    // For now, return a placeholder
    return "Unknown";
}

auto getInstalledUpdatesMacOS() -> std::vector<std::string> {
    spdlog::debug("Getting installed updates on macOS");
    std::vector<std::string> updates;
    // macOS update checking implementation would go here
    spdlog::info("Found {} installed updates on macOS", updates.size());
    return updates;
}

auto getSystemLanguageMacOS() -> std::string {
    spdlog::debug("Getting system language on macOS");
    // macOS language detection implementation would go here
    return "Unknown";
}

auto getSystemEncodingMacOS() -> std::string {
    spdlog::debug("Getting system encoding on macOS");
    // macOS encoding detection implementation would go here
    return "UTF-8";
}

}  // namespace atom::system

#endif  // __APPLE__
