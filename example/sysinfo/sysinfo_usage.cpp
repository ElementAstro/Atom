/**
 * @file basic_sysinfo_example.cpp
 * @brief Enhanced basic example demonstrating system information gathering
 *
 * This example shows how to:
 * - Get comprehensive operating system information
 * - Retrieve system uptime and boot time
 * - Check system language, encoding, and locale settings
 * - Get detailed memory information and monitoring
 * - Handle errors gracefully across different platforms
 * - Demonstrate proper usage patterns and best practices
 *
 * Features demonstrated:
 * - Cross-platform OS detection and information retrieval
 * - Memory usage monitoring with health checks
 * - System uptime calculation and formatting
 * - Language and encoding detection
 * - WSL (Windows Subsystem for Linux) detection
 * - Comprehensive error handling and logging
 * - Performance-conscious information gathering
 *
 * Platform support:
 * - Windows (including WSL detection)
 * - Linux (various distributions)
 * - macOS
 * - FreeBSD
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 2.0 - Enhanced with comprehensive error handling and documentation
 */

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Atom Sysinfo module headers
#include "atom/sysinfo/hardware/memory.hpp"
#include "atom/sysinfo/os.hpp"

using namespace atom::system;

// Constants for formatting and thresholds
namespace {
constexpr int64_t SECONDS_PER_DAY = 86400;
constexpr int64_t SECONDS_PER_HOUR = 3600;
constexpr int64_t SECONDS_PER_MINUTE = 60;
constexpr double BYTES_TO_GB = 1024.0 * 1024.0 * 1024.0;
}  // namespace

/**
 * @brief Utility function to format bytes to human-readable format
 * @param bytes Number of bytes to format
 * @return Formatted string with appropriate unit (B, KB, MB, GB, TB)
 */
std::string formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " "
        << units[unitIndex];
    return oss.str();
}

/**
 * @brief Utility function to format uptime in human-readable format
 * @param uptimeSeconds Total uptime in seconds
 * @return Formatted string showing days, hours, minutes, and seconds
 */
std::string formatUptime(int64_t uptimeSeconds) {
    auto days = uptimeSeconds / SECONDS_PER_DAY;
    uptimeSeconds %= SECONDS_PER_DAY;
    auto hours = uptimeSeconds / SECONDS_PER_HOUR;
    uptimeSeconds %= SECONDS_PER_HOUR;
    auto minutes = uptimeSeconds / SECONDS_PER_MINUTE;
    auto seconds = uptimeSeconds % SECONDS_PER_MINUTE;

    std::ostringstream oss;
    if (days > 0) {
        oss << days << " day" << (days != 1 ? "s" : "") << ", ";
    }
    oss << hours << " hour" << (hours != 1 ? "s" : "") << ", " << minutes
        << " minute" << (minutes != 1 ? "s" : "") << ", " << seconds
        << " second" << (seconds != 1 ? "s" : "");

    return oss.str();
}

/**
 * @brief Demonstrates comprehensive operating system information gathering
 *
 * This function showcases how to retrieve and display detailed operating system
 * information using the atom::system APIs. It includes proper error handling,
 * data validation, and user-friendly formatting.
 *
 * Features demonstrated:
 * - Complete OS information retrieval
 * - System uptime calculation and formatting
 * - Language and encoding detection
 * - WSL detection for Windows environments
 * - Boot time information
 * - Installed updates enumeration (where available)
 * - Comprehensive error handling with specific error types
 */
void enhancedOsInfoExample() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "=== Enhanced Operating System Information ===\n";
    std::cout << std::string(60, '=') << "\n";

    try {
        std::cout << "Gathering operating system information...\n\n";

        // Get comprehensive OS information with error checking
        OperatingSystemInfo osInfo;
        try {
            osInfo = getOperatingSystemInfo();
            std::cout << "✓ Successfully retrieved OS information\n\n";
        } catch (const std::exception& e) {
            std::cerr << "✗ Failed to retrieve OS information: " << e.what()
                      << "\n";
            return;
        }

        // Display core OS information
        std::cout << "Core System Information:\n";
        std::cout << "  OS Name:           "
                  << (osInfo.osName.empty() ? "Unknown" : osInfo.osName)
                  << "\n";
        std::cout << "  OS Version:        "
                  << (osInfo.osVersion.empty() ? "Unknown" : osInfo.osVersion)
                  << "\n";
        std::cout << "  Kernel Version:    "
                  << (osInfo.kernelVersion.empty() ? "Unknown"
                                                   : osInfo.kernelVersion)
                  << "\n";
        std::cout << "  Architecture:      "
                  << (osInfo.architecture.empty() ? "Unknown"
                                                  : osInfo.architecture)
                  << "\n";
        std::cout << "  Compiler:          "
                  << (osInfo.compiler.empty() ? "Unknown" : osInfo.compiler)
                  << "\n";

        // Display system identification
        std::cout << "\nSystem Identification:\n";
        std::cout << "  Computer Name:     "
                  << (osInfo.computerName.empty() ? "Unknown"
                                                  : osInfo.computerName)
                  << "\n";
        std::cout << "  Time Zone:         "
                  << (osInfo.timeZone.empty() ? "Unknown" : osInfo.timeZone)
                  << "\n";
        std::cout << "  Character Set:     "
                  << (osInfo.charSet.empty() ? "Unknown" : osInfo.charSet)
                  << "\n";
        std::cout << "  Server Edition:    " << (osInfo.isServer ? "Yes" : "No")
                  << "\n";

        // Display boot and timing information
        std::cout << "\nTiming Information:\n";
        if (!osInfo.bootTime.empty()) {
            std::cout << "  Boot Time:         " << osInfo.bootTime << "\n";
        }

        // Get and display system uptime with enhanced formatting
        try {
            auto uptime = getSystemUptime();
            auto uptimeSeconds = uptime.count();

            std::cout << "  System Uptime:     " << uptimeSeconds
                      << " seconds\n";
            std::cout << "  Uptime (formatted): " << formatUptime(uptimeSeconds)
                      << "\n";

            // Provide uptime assessment
            if (uptimeSeconds > 30 * SECONDS_PER_DAY) {
                std::cout << "  Uptime Status:     ⚠️  Long uptime (>30 days) - "
                             "consider reboot for updates\n";
            } else if (uptimeSeconds > 7 * SECONDS_PER_DAY) {
                std::cout << "  Uptime Status:     ✓ Stable uptime (>1 week)\n";
            } else if (uptimeSeconds > SECONDS_PER_DAY) {
                std::cout << "  Uptime Status:     ✓ Good uptime (>1 day)\n";
            } else {
                std::cout
                    << "  Uptime Status:     ℹ️  Recently restarted (<1 day)\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  Uptime:            ✗ Failed to retrieve: "
                      << e.what() << "\n";
        }

        // Display localization information
        std::cout << "\nLocalization Information:\n";
        try {
            auto language = getSystemLanguage();
            std::cout << "  System Language:   "
                      << (language.empty() ? "Unknown" : language) << "\n";
        } catch (const std::exception& e) {
            std::cerr << "  System Language:   ✗ Failed to retrieve: "
                      << e.what() << "\n";
        }

        try {
            auto encoding = getSystemEncoding();
            std::cout << "  System Encoding:   "
                      << (encoding.empty() ? "Unknown" : encoding) << "\n";
        } catch (const std::exception& e) {
            std::cerr << "  System Encoding:   ✗ Failed to retrieve: "
                      << e.what() << "\n";
        }

        // Check for special environments
        std::cout << "\nEnvironment Detection:\n";
        try {
            bool wslDetected = isWsl();
            std::cout << "  WSL Environment:   "
                      << (wslDetected ? "Yes ✓" : "No") << "\n";
            if (wslDetected) {
                std::cout << "  Note:              Running in Windows "
                             "Subsystem for Linux\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  WSL Detection:     ✗ Failed: " << e.what() << "\n";
        }

        // Display installed updates if available
        if (!osInfo.installedUpdates.empty()) {
            std::cout << "\nRecent System Updates:\n";
            size_t updateCount =
                std::min(osInfo.installedUpdates.size(), size_t(5));
            for (size_t i = 0; i < updateCount; ++i) {
                std::cout << "  " << (i + 1) << ". "
                          << osInfo.installedUpdates[i] << "\n";
            }
            if (osInfo.installedUpdates.size() > 5) {
                std::cout << "  ... and "
                          << (osInfo.installedUpdates.size() - 5)
                          << " more updates\n";
            }
        } else {
            std::cout << "\nSystem Updates:      No recent update information "
                         "available\n";
        }

        std::cout << "\n✓ Operating system information gathering completed "
                     "successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "\n✗ Critical error in OS information gathering: "
                  << e.what() << "\n";
        std::cerr << "This may indicate a system compatibility issue or "
                     "insufficient permissions.\n";
    }
}

/**
 * @brief Demonstrates comprehensive memory information and monitoring
 *
 * This function showcases how to retrieve and analyze detailed memory
 * information using the atom::system memory APIs. It includes health
 * monitoring, performance analysis, and user-friendly formatting.
 *
 * Features demonstrated:
 * - Basic and detailed memory statistics
 * - Memory usage percentage calculation
 * - Physical and virtual memory analysis
 * - Memory health assessment and warnings
 * - Human-readable memory size formatting
 * - Performance monitoring capabilities
 * - Cross-platform memory information retrieval
 */
void enhancedMemoryInfoExample() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "=== Enhanced Memory Information and Monitoring ===\n";
    std::cout << std::string(60, '=') << "\n";

    try {
        std::cout << "Gathering memory information...\n\n";

        // Get basic memory usage percentage first
        double memoryUsagePercent = 0.0;
        try {
            memoryUsagePercent = getMemoryUsage();
            std::cout << "✓ Successfully retrieved basic memory usage\n";
        } catch (const std::exception& e) {
            std::cerr << "✗ Failed to retrieve basic memory usage: " << e.what()
                      << "\n";
            return;
        }

        // Display basic memory usage with visual indicator
        std::cout << "\nBasic Memory Usage:\n";
        std::cout << "  Memory Usage:      " << std::fixed
                  << std::setprecision(1) << memoryUsagePercent << "%\n";

        // Create a simple text-based progress bar
        int barWidth = 40;
        int filledWidth =
            static_cast<int>((memoryUsagePercent / 100.0) * barWidth);
        std::cout << "  Usage Bar:         [";
        for (int i = 0; i < barWidth; ++i) {
            if (i < filledWidth) {
                if (memoryUsagePercent > 90.0)
                    std::cout << "█";
                else if (memoryUsagePercent > 80.0)
                    std::cout << "▓";
                else
                    std::cout << "▒";
            } else {
                std::cout << "░";
            }
        }
        std::cout << "] " << std::fixed << std::setprecision(1)
                  << memoryUsagePercent << "%\n";

        // Try to get detailed memory information
        try {
            auto memInfo = getDetailedMemoryStats();
            std::cout
                << "\n✓ Successfully retrieved detailed memory statistics\n";

            // Display physical memory information
            std::cout << "\nPhysical Memory Information:\n";
            std::cout << "  Total Physical:    "
                      << formatBytes(memInfo.totalPhysicalMemory) << " ("
                      << (memInfo.totalPhysicalMemory / BYTES_TO_GB)
                      << " GB)\n";
            std::cout << "  Available Physical:"
                      << formatBytes(memInfo.availablePhysicalMemory) << " ("
                      << (memInfo.availablePhysicalMemory / BYTES_TO_GB)
                      << " GB)\n";

            uint64_t usedPhysical =
                memInfo.totalPhysicalMemory - memInfo.availablePhysicalMemory;
            std::cout << "  Used Physical:     " << formatBytes(usedPhysical)
                      << " (" << (usedPhysical / BYTES_TO_GB) << " GB)\n";

            std::cout << "  Memory Load:       " << std::fixed
                      << std::setprecision(1) << memInfo.memoryLoadPercentage
                      << "%\n";

            // Display virtual memory information
            std::cout << "\nVirtual Memory Information:\n";
            std::cout << "  Total Virtual:     "
                      << formatBytes(memInfo.virtualMemoryMax) << " ("
                      << (memInfo.virtualMemoryMax / BYTES_TO_GB) << " GB)\n";
            std::cout << "  Used Virtual:      "
                      << formatBytes(memInfo.virtualMemoryUsed) << " ("
                      << (memInfo.virtualMemoryUsed / BYTES_TO_GB) << " GB)\n";

            uint64_t availableVirtual =
                memInfo.virtualMemoryMax - memInfo.virtualMemoryUsed;
            std::cout << "  Available Virtual: "
                      << formatBytes(availableVirtual) << " ("
                      << (availableVirtual / BYTES_TO_GB) << " GB)\n";

            // Memory efficiency analysis
            std::cout << "\nMemory Efficiency Analysis:\n";
            double physicalUsagePercent = (static_cast<double>(usedPhysical) /
                                           memInfo.totalPhysicalMemory) *
                                          100.0;
            double virtualUsagePercent =
                (static_cast<double>(memInfo.virtualMemoryUsed) /
                 memInfo.virtualMemoryMax) *
                100.0;

            std::cout << "  Physical Usage:    " << std::fixed
                      << std::setprecision(1) << physicalUsagePercent << "%\n";
            std::cout << "  Virtual Usage:     " << std::fixed
                      << std::setprecision(1) << virtualUsagePercent << "%\n";

            // Memory health assessment with detailed recommendations
            std::cout << "\nMemory Health Assessment:\n";
            if (memInfo.memoryLoadPercentage > 95.0) {
                std::cout << "  Status:            🔴 CRITICAL - Memory usage "
                             "extremely high!\n";
                std::cout << "  Recommendation:    Immediately close "
                             "unnecessary applications\n";
                std::cout << "                     Consider adding more RAM or "
                             "restarting system\n";
            } else if (memInfo.memoryLoadPercentage > 90.0) {
                std::cout << "  Status:            🟠 WARNING - Memory usage "
                             "very high\n";
                std::cout << "  Recommendation:    Close some applications to "
                             "free memory\n";
                std::cout << "                     Monitor for performance "
                             "degradation\n";
            } else if (memInfo.memoryLoadPercentage > 80.0) {
                std::cout << "  Status:            🟡 CAUTION - Memory usage "
                             "elevated\n";
                std::cout << "  Recommendation:    Monitor memory usage and "
                             "consider optimization\n";
            } else if (memInfo.memoryLoadPercentage > 60.0) {
                std::cout
                    << "  Status:            🟢 GOOD - Memory usage normal\n";
                std::cout
                    << "  Recommendation:    System running efficiently\n";
            } else {
                std::cout
                    << "  Status:            🔵 EXCELLENT - Low memory usage\n";
                std::cout << "  Recommendation:    Plenty of memory available "
                             "for applications\n";
            }

            // Memory pressure indicators
            if (memInfo.memoryLoadPercentage > 85.0) {
                std::cout << "\n⚠️  Memory Pressure Detected:\n";
                std::cout << "  - System may experience slowdowns\n";
                std::cout << "  - Applications may be swapped to disk\n";
                std::cout << "  - Consider closing unnecessary programs\n";
            }

        } catch (const std::exception& e) {
            std::cerr << "\n✗ Failed to retrieve detailed memory information: "
                      << e.what() << "\n";
            std::cerr << "This may indicate insufficient permissions or "
                         "unsupported platform features.\n";

            // Fallback to basic information only
            std::cout << "\nFalling back to basic memory information:\n";
            std::cout << "  Memory Usage:      " << std::fixed
                      << std::setprecision(1) << memoryUsagePercent << "%\n";
        }

        std::cout
            << "\n✓ Memory information gathering completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "\n✗ Critical error in memory information gathering: "
                  << e.what() << "\n";
        std::cerr << "This may indicate a system compatibility issue or "
                     "insufficient permissions.\n";
    }
}

/**
 * @brief Demonstrates comprehensive system summary and health assessment
 *
 * This function provides a high-level overview of the system status by
 * combining information from multiple sources. It's designed to give users a
 * quick but comprehensive view of their system's current state and health.
 *
 * Features demonstrated:
 * - Multi-component system overview
 * - Health assessment across different system areas
 * - Performance indicators and recommendations
 * - System stability analysis
 * - Quick diagnostic information
 */
void enhancedSystemSummaryExample() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "=== Enhanced System Summary and Health Assessment ===\n";
    std::cout << std::string(60, '=') << "\n";

    try {
        std::cout << "Generating comprehensive system summary...\n\n";

        // Gather all required information with error handling
        OperatingSystemInfo osInfo;
        std::chrono::seconds uptime{0};
        double memoryUsage = 0.0;
        bool osInfoAvailable = false;
        bool uptimeAvailable = false;
        bool memoryInfoAvailable = false;

        // Get OS information
        try {
            osInfo = getOperatingSystemInfo();
            osInfoAvailable = true;
        } catch (const std::exception& e) {
            std::cerr << "⚠️  OS information unavailable: " << e.what() << "\n";
        }

        // Get uptime information
        try {
            uptime = getSystemUptime();
            uptimeAvailable = true;
        } catch (const std::exception& e) {
            std::cerr << "⚠️  Uptime information unavailable: " << e.what()
                      << "\n";
        }

        // Get memory usage
        try {
            memoryUsage = getMemoryUsage();
            memoryInfoAvailable = true;
        } catch (const std::exception& e) {
            std::cerr << "⚠️  Memory information unavailable: " << e.what()
                      << "\n";
        }

        // Display system overview
        std::cout << "System Overview:\n";
        if (osInfoAvailable) {
            std::cout << "  System:            " << osInfo.osName << " "
                      << osInfo.osVersion << "\n";
            std::cout << "  Architecture:      " << osInfo.architecture << "\n";
            std::cout << "  Computer Name:     " << osInfo.computerName << "\n";
            std::cout << "  Kernel:            " << osInfo.kernelVersion
                      << "\n";
        } else {
            std::cout << "  System:            ✗ Information unavailable\n";
        }

        if (uptimeAvailable) {
            auto uptimeHours = uptime.count() / 3600;
            std::cout << "  Uptime:            " << formatUptime(uptime.count())
                      << "\n";
            std::cout << "  Uptime (hours):    " << uptimeHours << " hours\n";
        } else {
            std::cout << "  Uptime:            ✗ Information unavailable\n";
        }

        if (memoryInfoAvailable) {
            std::cout << "  Memory Usage:      " << std::fixed
                      << std::setprecision(1) << memoryUsage << "%\n";
        } else {
            std::cout << "  Memory Usage:      ✗ Information unavailable\n";
        }

        // Comprehensive system health assessment
        std::cout << "\nSystem Health Assessment:\n";
        int healthScore = 100;
        std::vector<std::string> issues;
        std::vector<std::string> recommendations;

        // Memory health check
        if (memoryInfoAvailable) {
            if (memoryUsage > 95.0) {
                std::cout << "  Memory Health:     🔴 CRITICAL (" << std::fixed
                          << std::setprecision(1) << memoryUsage << "%)\n";
                healthScore -= 30;
                issues.push_back("Critical memory usage");
                recommendations.push_back(
                    "Immediately close applications or restart system");
            } else if (memoryUsage > 85.0) {
                std::cout << "  Memory Health:     🟠 WARNING (" << std::fixed
                          << std::setprecision(1) << memoryUsage << "%)\n";
                healthScore -= 15;
                issues.push_back("High memory usage");
                recommendations.push_back("Close unnecessary applications");
            } else if (memoryUsage > 70.0) {
                std::cout << "  Memory Health:     🟡 CAUTION (" << std::fixed
                          << std::setprecision(1) << memoryUsage << "%)\n";
                healthScore -= 5;
            } else {
                std::cout << "  Memory Health:     🟢 GOOD (" << std::fixed
                          << std::setprecision(1) << memoryUsage << "%)\n";
            }
        } else {
            std::cout << "  Memory Health:     ❓ UNKNOWN\n";
            healthScore -= 10;
        }

        // Uptime stability check
        if (uptimeAvailable) {
            auto uptimeDays = uptime.count() / SECONDS_PER_DAY;
            if (uptimeDays > 60) {
                std::cout << "  Uptime Health:     🟠 LONG (" << uptimeDays
                          << " days)\n";
                healthScore -= 10;
                issues.push_back("Very long uptime without restart");
                recommendations.push_back(
                    "Consider restarting for updates and stability");
            } else if (uptimeDays > 30) {
                std::cout << "  Uptime Health:     🟡 EXTENDED (" << uptimeDays
                          << " days)\n";
                healthScore -= 5;
                recommendations.push_back(
                    "Consider restarting soon for updates");
            } else if (uptimeDays > 1) {
                std::cout << "  Uptime Health:     🟢 STABLE (" << uptimeDays
                          << " days)\n";
            } else {
                std::cout << "  Uptime Health:     🔵 RECENT ("
                          << (uptime.count() / 3600) << " hours)\n";
            }
        } else {
            std::cout << "  Uptime Health:     ❓ UNKNOWN\n";
            healthScore -= 10;
        }

        // Overall system health score
        std::cout << "\nOverall System Health:\n";
        std::cout << "  Health Score:      " << healthScore << "/100\n";

        if (healthScore >= 90) {
            std::cout << "  Overall Status:    🟢 EXCELLENT - System running "
                         "optimally\n";
        } else if (healthScore >= 75) {
            std::cout
                << "  Overall Status:    🟡 GOOD - Minor issues detected\n";
        } else if (healthScore >= 60) {
            std::cout
                << "  Overall Status:    🟠 FAIR - Some attention needed\n";
        } else {
            std::cout << "  Overall Status:    🔴 POOR - Immediate attention "
                         "required\n";
        }

        // Display issues and recommendations
        if (!issues.empty()) {
            std::cout << "\nIdentified Issues:\n";
            for (size_t i = 0; i < issues.size(); ++i) {
                std::cout << "  " << (i + 1) << ". " << issues[i] << "\n";
            }
        }

        if (!recommendations.empty()) {
            std::cout << "\nRecommendations:\n";
            for (size_t i = 0; i < recommendations.size(); ++i) {
                std::cout << "  " << (i + 1) << ". " << recommendations[i]
                          << "\n";
            }
        }

        // System capabilities summary
        std::cout << "\nSystem Capabilities Verified:\n";
        std::cout << "  ✓ OS Information:  "
                  << (osInfoAvailable ? "Available" : "Limited") << "\n";
        std::cout << "  ✓ Uptime Tracking: "
                  << (uptimeAvailable ? "Available" : "Limited") << "\n";
        std::cout << "  ✓ Memory Monitoring:"
                  << (memoryInfoAvailable ? "Available" : "Limited") << "\n";

        std::cout << "\n✓ System summary and health assessment completed\n";

    } catch (const std::exception& e) {
        std::cerr << "\n✗ Critical error in system summary generation: "
                  << e.what() << "\n";
        std::cerr << "This may indicate a system compatibility issue or "
                     "insufficient permissions.\n";
    }
}

/**
 * @brief Main function demonstrating enhanced sysinfo capabilities
 *
 * This main function orchestrates the execution of all enhanced system
 * information examples, providing a comprehensive demonstration of the
 * atom::system APIs. It includes proper error handling, performance timing, and
 * user-friendly output.
 */
int main() {
    std::cout << std::string(80, '=') << "\n";
    std::cout
        << "=== Atom System Information Module - Enhanced Basic Example ===\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "This example demonstrates comprehensive system information "
                 "gathering\n";
    std::cout << "with enhanced error handling, detailed analysis, and health "
                 "monitoring.\n";
    std::cout << std::string(80, '=') << "\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        std::cout << "\nStarting enhanced system information gathering...\n";

        // Run all enhanced examples with timing
        auto osStartTime = std::chrono::high_resolution_clock::now();
        enhancedOsInfoExample();
        auto osEndTime = std::chrono::high_resolution_clock::now();
        auto osDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            osEndTime - osStartTime);

        auto memStartTime = std::chrono::high_resolution_clock::now();
        enhancedMemoryInfoExample();
        auto memEndTime = std::chrono::high_resolution_clock::now();
        auto memDuration =
            std::chrono::duration_cast<std::chrono::milliseconds>(memEndTime -
                                                                  memStartTime);

        auto summaryStartTime = std::chrono::high_resolution_clock::now();
        enhancedSystemSummaryExample();
        auto summaryEndTime = std::chrono::high_resolution_clock::now();
        auto summaryDuration =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                summaryEndTime - summaryStartTime);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto totalDuration =
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                                  startTime);

        // Display completion summary with performance metrics
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout
            << "=== Enhanced System Information Completed Successfully ===\n";
        std::cout << std::string(80, '=') << "\n";

        std::cout << "\nPerformance Metrics:\n";
        std::cout << "  OS Information:    " << osDuration.count() << " ms\n";
        std::cout << "  Memory Analysis:   " << memDuration.count() << " ms\n";
        std::cout << "  System Summary:    " << summaryDuration.count()
                  << " ms\n";
        std::cout << "  Total Execution:   " << totalDuration.count()
                  << " ms\n";

        std::cout << "\nThe enhanced sysinfo module provides:\n";
        std::cout << "  ✓ Comprehensive operating system details and version "
                     "information\n";
        std::cout
            << "  ✓ Advanced system uptime analysis and boot time tracking\n";
        std::cout
            << "  ✓ Detailed memory usage monitoring with health assessment\n";
        std::cout << "  ✓ System language, encoding, and locale detection\n";
        std::cout << "  ✓ Cross-platform compatibility with graceful error "
                     "handling\n";
        std::cout
            << "  ✓ Intelligent system health monitoring and recommendations\n";
        std::cout << "  ✓ Performance metrics and timing analysis\n";
        std::cout << "  ✓ User-friendly formatting and visual indicators\n";
        std::cout << "  ✓ WSL (Windows Subsystem for Linux) detection\n";
        std::cout << "  ✓ System update information and history\n";

        std::cout << "\nNext Steps:\n";
        std::cout << "  • Explore the comprehensive system_info_example for "
                     "advanced features\n";
        std::cout << "  • Check the README.md for complete API documentation\n";
        std::cout << "  • Review platform-specific considerations for your "
                     "environment\n";

        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "Example completed successfully! Thank you for using Atom "
                     "Sysinfo.\n";
        std::cout << std::string(80, '=') << "\n";

    } catch (const std::runtime_error& e) {
        std::cerr << "\n" << std::string(80, '=') << "\n";
        std::cerr << "RUNTIME ERROR: " << e.what() << "\n";
        std::cerr << "This may indicate a platform compatibility issue or "
                     "missing dependencies.\n";
        std::cerr << std::string(80, '=') << "\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(80, '=') << "\n";
        std::cerr << "UNHANDLED EXCEPTION: " << e.what() << "\n";
        std::cerr << "Please report this issue with your platform details.\n";
        std::cerr << std::string(80, '=') << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(80, '=') << "\n";
        std::cerr << "UNKNOWN ERROR: An unexpected error occurred.\n";
        std::cerr << "Please report this issue with your platform details.\n";
        std::cerr << std::string(80, '=') << "\n";
        return 3;
    }

    return 0;
}
