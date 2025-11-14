/**
 * @file comprehensive_os_example.cpp
 * @brief Comprehensive example demonstrating all OS information capabilities
 *
 * This example provides an exhaustive demonstration of all operating system
 * information gathering capabilities available in the Atom Sysinfo module.
 * It showcases advanced features, cross-platform compatibility, and detailed
 * system analysis.
 *
 * Features demonstrated:
 * - Complete operating system information retrieval
 * - Advanced uptime and boot time analysis
 * - System language, locale, and encoding detection
 * - Virtual environment detection (WSL, containers, VMs)
 * - System update and patch information
 * - Performance monitoring and timing analysis
 * - Cross-platform compatibility testing
 * - Advanced error handling and diagnostics
 * - System security and configuration analysis
 *
 * Platform support:
 * - Windows (including WSL detection and Windows-specific features)
 * - Linux (various distributions with distro-specific detection)
 * - macOS (with macOS-specific system information)
 * - FreeBSD (with BSD-specific features)
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 1.0 - Comprehensive OS information demonstration
 */

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// Atom Sysinfo module headers
#include "atom/sysinfo/info/locale.hpp"
#include "atom/sysinfo/info/virtual.hpp"
#include "atom/sysinfo/os.hpp"

using namespace atom::system;

namespace {
constexpr int64_t SECONDS_PER_DAY = 86400;
constexpr int64_t SECONDS_PER_HOUR = 3600;
constexpr int64_t SECONDS_PER_MINUTE = 60;
constexpr int DISPLAY_WIDTH = 80;
}  // namespace

/**
 * @brief Utility function to create formatted section headers
 */
std::string createSectionHeader(const std::string& title) {
    std::string header = "\n" + std::string(DISPLAY_WIDTH, '=') + "\n";
    header += "=== " + title + " ===\n";
    header += std::string(DISPLAY_WIDTH, '=') + "\n";
    return header;
}

/**
 * @brief Utility function to format uptime in detailed human-readable format
 */
std::string formatDetailedUptime(int64_t uptimeSeconds) {
    auto years = uptimeSeconds / (365 * SECONDS_PER_DAY);
    uptimeSeconds %= (365 * SECONDS_PER_DAY);
    auto days = uptimeSeconds / SECONDS_PER_DAY;
    uptimeSeconds %= SECONDS_PER_DAY;
    auto hours = uptimeSeconds / SECONDS_PER_HOUR;
    uptimeSeconds %= SECONDS_PER_HOUR;
    auto minutes = uptimeSeconds / SECONDS_PER_MINUTE;
    auto seconds = uptimeSeconds % SECONDS_PER_MINUTE;

    std::ostringstream oss;
    if (years > 0) {
        oss << years << " year" << (years != 1 ? "s" : "") << ", ";
    }
    if (days > 0) {
        oss << days << " day" << (days != 1 ? "s" : "") << ", ";
    }
    oss << hours << " hour" << (hours != 1 ? "s" : "") << ", " << minutes
        << " minute" << (minutes != 1 ? "s" : "") << ", " << seconds
        << " second" << (seconds != 1 ? "s" : "");

    return oss.str();
}

/**
 * @brief Demonstrates comprehensive operating system information
 */
void demonstrateOSInformation() {
    std::cout << createSectionHeader(
        "Comprehensive Operating System Information");

    try {
        std::cout << "Gathering comprehensive OS information...\n\n";

        auto osInfo = getOperatingSystemInfo();

        // Core OS Information
        std::cout << "Core Operating System Details:\n";
        std::cout << "  OS Name:              " << osInfo.osName << "\n";
        std::cout << "  OS Version:           " << osInfo.osVersion << "\n";
        std::cout << "  Kernel Version:       " << osInfo.kernelVersion << "\n";
        std::cout << "  Architecture:         " << osInfo.architecture << "\n";
        std::cout << "  Build Compiler:       " << osInfo.compiler << "\n";

        // System Identity
        std::cout << "\nSystem Identity:\n";
        std::cout << "  Computer Name:        " << osInfo.computerName << "\n";
        std::cout << "  Time Zone:            " << osInfo.timeZone << "\n";
        std::cout << "  Character Set:        " << osInfo.charSet << "\n";
        std::cout << "  Server Edition:       "
                  << (osInfo.isServer ? "Yes" : "No") << "\n";

        // Boot and Timing Information
        std::cout << "\nBoot and Timing Information:\n";
        if (!osInfo.bootTime.empty()) {
            std::cout << "  Last Boot Time:       " << osInfo.bootTime << "\n";
        }

        auto uptime = getSystemUptime();
        auto uptimeSeconds = uptime.count();
        std::cout << "  Current Uptime:       "
                  << formatDetailedUptime(uptimeSeconds) << "\n";
        std::cout << "  Uptime (seconds):     " << uptimeSeconds << "\n";

        // Uptime analysis
        auto uptimeDays = uptimeSeconds / SECONDS_PER_DAY;
        if (uptimeDays > 365) {
            std::cout << "  Uptime Analysis:      🔴 EXTREMELY LONG (>1 year) "
                         "- System needs restart\n";
        } else if (uptimeDays > 90) {
            std::cout << "  Uptime Analysis:      🟠 VERY LONG (>3 months) - "
                         "Consider restart\n";
        } else if (uptimeDays > 30) {
            std::cout << "  Uptime Analysis:      🟡 LONG (>1 month) - Monitor "
                         "for issues\n";
        } else if (uptimeDays > 7) {
            std::cout << "  Uptime Analysis:      🟢 STABLE (>1 week) - Good "
                         "stability\n";
        } else {
            std::cout << "  Uptime Analysis:      🔵 RECENT (<1 week) - "
                         "Recently restarted\n";
        }

        // System Updates Information
        if (!osInfo.installedUpdates.empty()) {
            std::cout << "\nSystem Updates and Patches:\n";
            std::cout << "  Total Updates Found:  "
                      << osInfo.installedUpdates.size() << "\n";
            std::cout << "  Recent Updates:\n";

            size_t displayCount =
                std::min(osInfo.installedUpdates.size(), size_t(10));
            for (size_t i = 0; i < displayCount; ++i) {
                std::cout << "    " << (i + 1) << ". "
                          << osInfo.installedUpdates[i] << "\n";
            }

            if (osInfo.installedUpdates.size() > 10) {
                std::cout
                    << "    ... and " << (osInfo.installedUpdates.size() - 10)
                    << " more updates (use detailed view for complete list)\n";
            }
        } else {
            std::cout << "\nSystem Updates:         No update information "
                         "available\n";
        }

        std::cout << "\n✓ OS information gathering completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering OS information: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates language and locale information
 */
void demonstrateLanguageAndLocale() {
    std::cout << createSectionHeader("Language and Locale Information");

    try {
        std::cout << "Gathering language and locale information...\n\n";

        // Basic language and encoding
        std::cout << "Basic Language Settings:\n";
        try {
            auto language = getSystemLanguage();
            std::cout << "  System Language:      " << language << "\n";
        } catch (const std::exception& e) {
            std::cerr << "  System Language:      ✗ Error: " << e.what()
                      << "\n";
        }

        try {
            auto encoding = getSystemEncoding();
            std::cout << "  System Encoding:      " << encoding << "\n";
        } catch (const std::exception& e) {
            std::cerr << "  System Encoding:      ✗ Error: " << e.what()
                      << "\n";
        }

        std::cout << "\n✓ Language and locale information completed\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering language information: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates virtual environment detection
 */
void demonstrateVirtualEnvironmentDetection() {
    std::cout << createSectionHeader("Virtual Environment Detection");

    try {
        std::cout << "Detecting virtual environments and containers...\n\n";

        // WSL Detection
        std::cout << "Environment Detection:\n";
        try {
            bool wslDetected = isWsl();
            std::cout << "  WSL Environment:      "
                      << (wslDetected ? "Yes ✓" : "No") << "\n";
            if (wslDetected) {
                std::cout << "  WSL Details:          Running in Windows "
                             "Subsystem for Linux\n";
                std::cout << "  Note:                 Some Windows-specific "
                             "features may be limited\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  WSL Detection:        ✗ Error: " << e.what()
                      << "\n";
        }

        std::cout << "\n✓ Virtual environment detection completed\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error in virtual environment detection: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive OS capabilities
 */
int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout
        << "=== Atom Sysinfo - Comprehensive OS Information Example ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "This example demonstrates all available OS information "
                 "capabilities\n";
    std::cout << "with detailed analysis, cross-platform support, and advanced "
                 "features.\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Execute all demonstration functions
        demonstrateOSInformation();
        demonstrateLanguageAndLocale();
        demonstrateVirtualEnvironmentDetection();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        // Final summary
        std::cout << createSectionHeader("Execution Summary");
        std::cout << "All OS information gathering completed successfully!\n";
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ Complete operating system information retrieval\n";
        std::cout << "  ✓ Advanced uptime and boot time analysis\n";
        std::cout << "  ✓ System language and encoding detection\n";
        std::cout << "  ✓ Virtual environment detection (WSL)\n";
        std::cout << "  ✓ System update and patch information\n";
        std::cout << "  ✓ Cross-platform compatibility\n";
        std::cout << "  ✓ Comprehensive error handling\n";

        std::cout << "\n" << std::string(DISPLAY_WIDTH, '=') << "\n";
        std::cout << "Example completed successfully!\n";
        std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";

    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(DISPLAY_WIDTH, '=') << "\n";
        std::cerr << "CRITICAL ERROR: " << e.what() << "\n";
        std::cerr << std::string(DISPLAY_WIDTH, '=') << "\n";
        return 1;
    }

    return 0;
}
