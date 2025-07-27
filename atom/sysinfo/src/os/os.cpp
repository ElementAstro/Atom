#include "os.hpp"

#include <chrono>
#include <format>
#include <sstream>

#include <spdlog/spdlog.h>

// Include platform-specific implementations
#ifdef _WIN32
#include "windows.hpp"
#elif defined(__linux__) || defined(__linux)
#include "linux.hpp"
#elif defined(__APPLE__)
#include "macos.hpp"
#endif

namespace atom::system {

auto OperatingSystemInfo::toJson() const -> std::string {
    spdlog::debug("Converting OperatingSystemInfo to JSON");
    std::stringstream ss;
    ss << "{\n";
    ss << R"(  "osName": ")" << osName << "\",\n";
    ss << R"(  "osVersion": ")" << osVersion << "\",\n";
    ss << R"(  "kernelVersion": ")" << kernelVersion << "\",\n";
    ss << R"(  "architecture": ")" << architecture << "\",\n";
    ss << R"(  "compiler": ")" << compiler << "\",\n";
    ss << R"(  "computerName": ")" << computerName << "\",\n";
    ss << R"(  "bootTime": ")" << bootTime << "\",\n";
    ss << R"(  "timeZone": ")" << timeZone << "\",\n";
    ss << R"(  "charSet": ")" << charSet << "\",\n";
    ss << R"(  "isServer": )" << (isServer ? "true" : "false") << "\n";
    ss << "}\n";
    return ss.str();
}

auto OperatingSystemInfo::toDetailedString() const -> std::string {
    spdlog::debug("Converting OperatingSystemInfo to detailed string");
    std::stringstream ss;
    ss << "Operating System Information:\n";
    ss << "  OS Name: " << osName << "\n";
    ss << "  OS Version: " << osVersion << "\n";
    ss << "  Kernel Version: " << kernelVersion << "\n";
    ss << "  Architecture: " << architecture << "\n";
    ss << "  Compiler: " << compiler << "\n";
    ss << "  Computer Name: " << computerName << "\n";
    ss << "  Boot Time: " << bootTime << "\n";
    ss << "  Time Zone: " << timeZone << "\n";
    ss << "  Character Set: " << charSet << "\n";
    ss << "  Server Edition: " << (isServer ? "Yes" : "No") << "\n";
    return ss.str();
}

auto OperatingSystemInfo::toJsonString() const -> std::string {
    return toJson();
}

auto getComputerName() -> std::optional<std::string> {
    spdlog::debug("Retrieving computer name");
#ifdef _WIN32
    return getComputerNameWindows();
#elif defined(__linux__) || defined(__linux)
    return getComputerNameLinux();
#elif defined(__APPLE__)
    return getComputerNameMacOS();
#elif defined(__ANDROID__)
    spdlog::warn("Getting computer name is not supported on Android");
    return std::nullopt;
#else
    return std::nullopt;
#endif
}

auto getOperatingSystemInfo() -> OperatingSystemInfo {
    spdlog::info("Retrieving operating system information");
    OperatingSystemInfo osInfo;

    // Get platform-specific OS information
#ifdef _WIN32
    getOperatingSystemInfoWindows(osInfo);
#elif defined(__linux__) || defined(__linux)
    getOperatingSystemInfoLinux(osInfo);
#elif defined(__APPLE__)
    getOperatingSystemInfoMacOS(osInfo);
#endif

    // Set architecture information
#if defined(__i386__) || defined(__i386)
    const std::string ARCHITECTURE = "x86";
#elif defined(__x86_64__)
    const std::string ARCHITECTURE = "x86_64";
#elif defined(__arm__)
    const std::string ARCHITECTURE = "ARM";
#elif defined(__aarch64__)
    const std::string ARCHITECTURE = "ARM64";
#else
    const std::string ARCHITECTURE = "Unknown";
#endif
    osInfo.architecture = ARCHITECTURE;
    spdlog::info("Detected architecture: {}", ARCHITECTURE);

    // Set compiler information
    const std::string COMPILER =
#if defined(__clang__)
        std::format("Clang {}.{}.{}", __clang_major__, __clang_minor__,
                    __clang_patchlevel__);
#elif defined(__GNUC__)
        std::format("GCC {}.{}.{}", __GNUC__, __GNUC_MINOR__,
                    __GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
        std::format("MSVC {}", _MSC_FULL_VER);
#else
        "Unknown";
#endif
    osInfo.compiler = COMPILER;
    spdlog::info("Detected compiler: {}", COMPILER);

    // Fill in remaining information
    osInfo.computerName = getComputerName().value_or("Unknown");
    osInfo.bootTime = getLastBootTime();
    osInfo.timeZone = getSystemTimeZone();
    osInfo.charSet = getSystemEncoding();
    osInfo.isServer = isServerEdition();
    osInfo.installedUpdates = getInstalledUpdates();

    spdlog::info(
        "Successfully retrieved complete operating system information");
    return osInfo;
}

auto getSystemUptime() -> std::chrono::seconds {
    spdlog::debug("Getting system uptime");
#ifdef _WIN32
    return getSystemUptimeWindows();
#elif defined(__linux__) || defined(__linux)
    return getSystemUptimeLinux();
#elif defined(__APPLE__)
    return getSystemUptimeMacOS();
#else
    return std::chrono::seconds(0);
#endif
}

auto getLastBootTime() -> std::string {
    spdlog::debug("Getting last boot time");
    auto uptime = getSystemUptime();
    auto now = std::chrono::system_clock::now();
    auto bootTime = now - uptime;
    auto bootTimeT = std::chrono::system_clock::to_time_t(bootTime);
    return std::string(std::ctime(&bootTimeT));
}

auto getSystemTimeZone() -> std::string {
    spdlog::debug("Getting system timezone");
#ifdef _WIN32
    return getSystemTimeZoneWindows();
#elif defined(__linux__) || defined(__linux)
    return getSystemTimeZoneLinux();
#elif defined(__APPLE__)
    return getSystemTimeZoneMacOS();
#else
    return "Unknown";
#endif
}

auto getInstalledUpdates() -> std::vector<std::string> {
    spdlog::debug("Getting installed updates");
#ifdef _WIN32
    return getInstalledUpdatesWindows();
#elif defined(__linux__) || defined(__linux)
    return getInstalledUpdatesLinux();
#elif defined(__APPLE__)
    return getInstalledUpdatesMacOS();
#else
    return {};
#endif
}

auto getSystemLanguage() -> std::string {
    spdlog::debug("Getting system language");
#ifdef _WIN32
    return getSystemLanguageWindows();
#elif defined(__linux__) || defined(__linux)
    return getSystemLanguageLinux();
#elif defined(__APPLE__)
    return getSystemLanguageMacOS();
#else
    return "Unknown";
#endif
}

auto getSystemEncoding() -> std::string {
    spdlog::debug("Getting system encoding");
#ifdef _WIN32
    return getSystemEncodingWindows();
#elif defined(__linux__) || defined(__linux)
    return getSystemEncodingLinux();
#elif defined(__APPLE__)
    return getSystemEncodingMacOS();
#else
    return "UTF-8";
#endif
}

auto isServerEdition() -> bool {
    spdlog::debug("Checking if OS is server edition");
#ifdef _WIN32
    return isServerEditionWindows();
#else
    return false;
#endif
}

}  // namespace atom::system
