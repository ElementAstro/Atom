#include "os.hpp"

#include <array>
#include <chrono>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <sys/sysinfo.h>
#include <unistd.h>
#include <fstream>
#elif __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/utsname.h>
#endif

#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"

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

/**
 * @brief Retrieves the computer name from the system
 * @return Optional string containing the computer name, or nullopt if failed
 */
auto getComputerName() -> std::optional<std::string> {
    spdlog::debug("Retrieving computer name");
    constexpr size_t bufferSize = 256;
    std::array<char, bufferSize> buffer;

#if defined(_WIN32)
    auto size = static_cast<DWORD>(buffer.size());
    if (GetComputerNameA(buffer.data(), &size)) {
        spdlog::info("Successfully retrieved computer name: {}", buffer.data());
        return std::string(buffer.data());
    } else {
        spdlog::error("Failed to get computer name on Windows");
    }
#elif defined(__APPLE__)
    CFStringRef name = SCDynamicStoreCopyComputerName(nullptr, nullptr);
    if (name != nullptr) {
        CFStringGetCString(name, buffer.data(), buffer.size(),
                           kCFStringEncodingUTF8);
        CFRelease(name);
        spdlog::info("Successfully retrieved computer name: {}", buffer.data());
        return std::string(buffer.data());
    } else {
        spdlog::error("Failed to get computer name on macOS");
    }
#elif defined(__linux__) || defined(__linux)
    if (gethostname(buffer.data(), buffer.size()) == 0) {
        spdlog::info("Successfully retrieved computer name: {}", buffer.data());
        return std::string(buffer.data());
    } else {
        spdlog::error("Failed to get computer name on Linux");
    }
#elif defined(__ANDROID__)
    spdlog::warn("Getting computer name is not supported on Android");
    return std::nullopt;
#endif

    return std::nullopt;
}

/**
 * @brief Parses a configuration file for OS information
 * @param filePath Path to the file to parse
 * @return Pair containing OS name and version
 */
auto parseFile(const std::string& filePath)
    -> std::pair<std::string, std::string> {
    spdlog::debug("Parsing file: {}", filePath);
    std::ifstream file(filePath);
    if (!file.is_open()) {
        spdlog::error("Cannot open file: {}", filePath);
        THROW_FAIL_TO_OPEN_FILE("Cannot open file: " + filePath);
    }

    std::pair<std::string, std::string> osInfo;
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);

            if (!value.empty() && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }

            if (key == "PRETTY_NAME") {
                osInfo.first = value;
                spdlog::debug("Found PRETTY_NAME: {}", value);
            } else if (key == "VERSION") {
                osInfo.second = value;
                spdlog::debug("Found VERSION: {}", value);
            }
        }
    }

    return osInfo;
}

auto getOperatingSystemInfo() -> OperatingSystemInfo {
    spdlog::info("Retrieving operating system information");
    OperatingSystemInfo osInfo;

#ifdef _WIN32
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
#elif __linux__
    spdlog::debug("Using Linux API for OS information");
    auto osReleaseInfo = parseFile("/etc/os-release");
    if (!osReleaseInfo.first.empty()) {
        osInfo.osName = osReleaseInfo.first;
        osInfo.osVersion = osReleaseInfo.second;
    } else {
        auto lsbReleaseInfo = parseFile("/etc/lsb-release");
        if (!lsbReleaseInfo.first.empty()) {
            osInfo.osName = lsbReleaseInfo.first;
            osInfo.osVersion = lsbReleaseInfo.second;
        } else {
            std::ifstream redhatReleaseFile("/etc/redhat-release");
            if (redhatReleaseFile.is_open()) {
                std::string line;
                std::getline(redhatReleaseFile, line);
                osInfo.osName = line;
                redhatReleaseFile.close();
                spdlog::info("Retrieved OS info from /etc/redhat-release: {}",
                             line);
            }
        }
    }

    if (osInfo.osName.empty()) {
        spdlog::error("Failed to get OS name on Linux");
    }

    std::ifstream kernelVersionFile("/proc/version");
    if (kernelVersionFile.is_open()) {
        std::string line;
        std::getline(kernelVersionFile, line);
        osInfo.kernelVersion = line.substr(0, line.find(" "));
        kernelVersionFile.close();
        spdlog::info("Retrieved kernel version: {}", osInfo.kernelVersion);
    } else {
        spdlog::error("Failed to open /proc/version");
    }
#elif __APPLE__
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
#endif

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

auto isWsl() -> bool {
    spdlog::debug("Checking if running in WSL");
    std::ifstream procVersion("/proc/version");
    std::string line;
    if (procVersion.is_open()) {
        std::getline(procVersion, line);
        procVersion.close();
        bool isWslEnv = line.find("microsoft") != std::string::npos ||
                        line.find("WSL") != std::string::npos;
        spdlog::info("WSL detection result: {}", isWslEnv);
        return isWslEnv;
    } else {
        spdlog::error("Failed to open /proc/version for WSL detection");
    }
    return false;
}

auto getSystemUptime() -> std::chrono::seconds {
    spdlog::debug("Getting system uptime");
#ifdef _WIN32
    return std::chrono::seconds(GetTickCount64() / 1000);
#elif __linux__
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return std::chrono::seconds(si.uptime);
    }
#elif __APPLE__
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    if (sysctl(mib, 2, &boottime, &len, nullptr, 0) == 0) {
        time_t now;
        time(&now);
        return std::chrono::seconds(now - boottime.tv_sec);
    }
#endif
    return std::chrono::seconds(0);
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
    TIME_ZONE_INFORMATION tzi;
    if (GetTimeZoneInformation(&tzi) != TIME_ZONE_ID_INVALID) {
        std::wstring wstr(tzi.StandardName);
        return std::string(wstr.begin(), wstr.end());
    }
#elif __linux__
    std::ifstream tz("/etc/timezone");
    if (tz.is_open()) {
        std::string timezone;
        std::getline(tz, timezone);
        return timezone;
    }
#endif
    return "Unknown";
}

auto getInstalledUpdates() -> std::vector<std::string> {
    spdlog::debug("Getting installed updates");
    std::vector<std::string> updates;

#ifdef _WIN32
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
#elif __linux__
    std::ifstream log("/var/log/dpkg.log");
    if (log.is_open()) {
        std::string line;
        while (std::getline(log, line)) {
            if (line.find("install") != std::string::npos) {
                updates.push_back(line);
            }
        }
    }
#endif

    spdlog::info("Found {} installed updates", updates.size());
    return updates;
}

auto checkForUpdates() -> std::vector<std::string> {
    spdlog::debug("Checking for available updates");
    std::vector<std::string> updates;
    spdlog::warn("Update checking not implemented for this platform");
    return updates;
}

auto getSystemLanguage() -> std::string {
    spdlog::debug("Getting system language");
#ifdef _WIN32
    LCID lcid = GetSystemDefaultLCID();
    WCHAR lang[LOCALE_NAME_MAX_LENGTH];
    if (LCIDToLocaleName(lcid, lang, LOCALE_NAME_MAX_LENGTH, 0) > 0) {
        std::wstring wstr(lang);
        return std::string(wstr.begin(), wstr.end());
    }
#elif __linux__
    const char* lang = getenv("LANG");
    if (lang) {
        return std::string(lang);
    }
#endif
    return "Unknown";
}

auto getSystemEncoding() -> std::string {
    spdlog::debug("Getting system encoding");
#ifdef _WIN32
    UINT codePage = GetACP();
    return std::format("CP{}", codePage);
#elif __linux__
    const char* encoding = getenv("LC_CTYPE");
    if (encoding) {
        return std::string(encoding);
    }
#endif
    return "UTF-8";
}

auto isServerEdition() -> bool {
    spdlog::debug("Checking if OS is server edition");
#ifdef _WIN32
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (GetVersionEx(reinterpret_cast<LPOSVERSIONINFO>(&osvi))) {
        return osvi.wProductType != VER_NT_WORKSTATION;
    }
#endif
    return false;
}

}  // namespace atom::system
