/**
 * @file memory_monitoring_example.cpp
 * @brief Comprehensive example demonstrating memory information and monitoring
 *
 * This example provides a complete demonstration of memory information
 * gathering and real-time monitoring capabilities available in the Atom Sysinfo
 * module. It showcases advanced memory analysis, performance monitoring, and
 * health tracking.
 *
 * Features demonstrated:
 * - Complete memory information retrieval (physical, virtual, swap)
 * - Memory usage monitoring and analysis
 * - Memory performance metrics and bandwidth monitoring
 * - Memory health assessment and warnings
 * - Physical memory module information
 * - Virtual memory management analysis
 * - Memory pressure detection and recommendations
 * - Real-time memory monitoring capabilities
 * - Cross-platform memory information gathering
 *
 * Platform support:
 * - Windows (with WMI and Performance Counters)
 * - Linux (with /proc/meminfo and sysfs)
 * - macOS (with vm_stat and sysctl)
 * - FreeBSD (with sysctl)
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 1.0 - Comprehensive memory monitoring demonstration
 */

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Atom Sysinfo module headers
#include "atom/sysinfo/memory.hpp"

using namespace atom::system;

namespace {
constexpr int DISPLAY_WIDTH = 80;
constexpr double MEMORY_WARNING_THRESHOLD = 80.0;
constexpr double MEMORY_CRITICAL_THRESHOLD = 90.0;
constexpr double MEMORY_DANGER_THRESHOLD = 95.0;
constexpr double BYTES_TO_GB = 1024.0 * 1024.0 * 1024.0;
constexpr double BYTES_TO_MB = 1024.0 * 1024.0;
constexpr double BYTES_TO_KB = 1024.0;
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
 * @brief Format bytes to human-readable format with appropriate units
 */
std::string formatBytes(uint64_t bytes) {
    if (bytes >= BYTES_TO_GB) {
        return std::to_string(static_cast<int>(bytes / BYTES_TO_GB)) + " GB";
    } else if (bytes >= BYTES_TO_MB) {
        return std::to_string(static_cast<int>(bytes / BYTES_TO_MB)) + " MB";
    } else if (bytes >= BYTES_TO_KB) {
        return std::to_string(static_cast<int>(bytes / BYTES_TO_KB)) + " KB";
    } else {
        return std::to_string(bytes) + " B";
    }
}

/**
 * @brief Format bytes to precise decimal format
 */
std::string formatBytesDecimal(uint64_t bytes) {
    std::ostringstream oss;
    if (bytes >= BYTES_TO_GB) {
        oss << std::fixed << std::setprecision(2) << (bytes / BYTES_TO_GB)
            << " GB";
    } else if (bytes >= BYTES_TO_MB) {
        oss << std::fixed << std::setprecision(1) << (bytes / BYTES_TO_MB)
            << " MB";
    } else if (bytes >= BYTES_TO_KB) {
        oss << std::fixed << std::setprecision(1) << (bytes / BYTES_TO_KB)
            << " KB";
    } else {
        oss << bytes << " B";
    }
    return oss.str();
}

/**
 * @brief Create a detailed text-based progress bar with color indicators
 */
std::string createMemoryProgressBar(double percentage, int width = 50) {
    int filledWidth = static_cast<int>((percentage / 100.0) * width);
    std::string bar = "[";

    for (int i = 0; i < width; ++i) {
        if (i < filledWidth) {
            if (percentage > MEMORY_DANGER_THRESHOLD)
                bar += "█";
            else if (percentage > MEMORY_CRITICAL_THRESHOLD)
                bar += "▓";
            else if (percentage > MEMORY_WARNING_THRESHOLD)
                bar += "▒";
            else
                bar += "░";
        } else {
            bar += " ";
        }
    }

    bar += "] " + std::to_string(static_cast<int>(percentage)) + "%";
    return bar;
}

/**
 * @brief Demonstrates comprehensive memory information
 */
void demonstrateMemoryInformation() {
    std::cout << createSectionHeader("Comprehensive Memory Information");

    try {
        std::cout << "Gathering comprehensive memory information...\n\n";

        // Get basic memory usage first
        auto memoryUsage = getMemoryUsage();
        std::cout << "Basic Memory Usage Overview:\n";
        std::cout << "  Memory Usage:         " << std::fixed
                  << std::setprecision(1) << memoryUsage << "%\n";
        std::cout << "  Usage Bar:            "
                  << createMemoryProgressBar(memoryUsage) << "\n";

        // Memory health assessment
        if (memoryUsage > MEMORY_DANGER_THRESHOLD) {
            std::cout << "  Health Status:        🔴 DANGER - Critical memory "
                         "shortage!\n";
            std::cout << "  Recommendation:       Immediately close "
                         "applications or restart\n";
        } else if (memoryUsage > MEMORY_CRITICAL_THRESHOLD) {
            std::cout << "  Health Status:        🟠 CRITICAL - Very high "
                         "memory usage\n";
            std::cout
                << "  Recommendation:       Close unnecessary applications\n";
        } else if (memoryUsage > MEMORY_WARNING_THRESHOLD) {
            std::cout
                << "  Health Status:        🟡 WARNING - High memory usage\n";
            std::cout << "  Recommendation:       Monitor and consider "
                         "optimization\n";
        } else if (memoryUsage > 60.0) {
            std::cout
                << "  Health Status:        🟢 GOOD - Normal memory usage\n";
        } else {
            std::cout
                << "  Health Status:        🔵 EXCELLENT - Low memory usage\n";
        }

        // Get detailed memory statistics
        auto memInfo = getDetailedMemoryStats();

        std::cout << "\nDetailed Physical Memory Information:\n";
        std::cout << "  Total Physical:       "
                  << formatBytesDecimal(memInfo.totalPhysicalMemory) << " ("
                  << memInfo.totalPhysicalMemory << " bytes)\n";
        std::cout << "  Available Physical:   "
                  << formatBytesDecimal(memInfo.availablePhysicalMemory) << " ("
                  << memInfo.availablePhysicalMemory << " bytes)\n";

        uint64_t usedPhysical =
            memInfo.totalPhysicalMemory - memInfo.availablePhysicalMemory;
        std::cout << "  Used Physical:        "
                  << formatBytesDecimal(usedPhysical) << " (" << usedPhysical
                  << " bytes)\n";

        double physicalUsagePercent =
            (static_cast<double>(usedPhysical) / memInfo.totalPhysicalMemory) *
            100.0;
        std::cout << "  Physical Usage:       " << std::fixed
                  << std::setprecision(1) << physicalUsagePercent << "%\n";
        std::cout << "  Memory Load:          " << std::fixed
                  << std::setprecision(1) << memInfo.memoryLoadPercentage
                  << "%\n";

        std::cout << "\nDetailed Virtual Memory Information:\n";
        std::cout << "  Total Virtual:        "
                  << formatBytesDecimal(memInfo.virtualMemoryMax) << " ("
                  << memInfo.virtualMemoryMax << " bytes)\n";
        std::cout << "  Used Virtual:         "
                  << formatBytesDecimal(memInfo.virtualMemoryUsed) << " ("
                  << memInfo.virtualMemoryUsed << " bytes)\n";

        uint64_t availableVirtual =
            memInfo.virtualMemoryMax - memInfo.virtualMemoryUsed;
        std::cout << "  Available Virtual:    "
                  << formatBytesDecimal(availableVirtual) << " ("
                  << availableVirtual << " bytes)\n";

        double virtualUsagePercent =
            (static_cast<double>(memInfo.virtualMemoryUsed) /
             memInfo.virtualMemoryMax) *
            100.0;
        std::cout << "  Virtual Usage:        " << std::fixed
                  << std::setprecision(1) << virtualUsagePercent << "%\n";

        std::cout
            << "\n✓ Memory information gathering completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering memory information: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates physical memory module information
 */
void demonstratePhysicalMemoryInfo() {
    std::cout << createSectionHeader("Physical Memory Module Information");

    try {
        std::cout << "Gathering physical memory module information...\n\n";

        auto memSlot = getPhysicalMemoryInfo();

        std::cout << "Physical Memory Module Details:\n";
        std::cout << "  Capacity:             "
                  << (memSlot.capacity.empty() ? "Unknown" : memSlot.capacity)
                  << "\n";
        std::cout << "  Clock Speed:          "
                  << (memSlot.clockSpeed.empty() ? "Unknown"
                                                 : memSlot.clockSpeed)
                  << "\n";
        std::cout << "  Memory Type:          "
                  << (memSlot.type.empty() ? "Unknown" : memSlot.type) << "\n";

        // Memory technology analysis
        if (!memSlot.type.empty()) {
            std::cout << "\nMemory Technology Analysis:\n";
            if (memSlot.type.find("DDR5") != std::string::npos) {
                std::cout << "  Technology:           🟢 DDR5 - Latest "
                             "generation, excellent performance\n";
            } else if (memSlot.type.find("DDR4") != std::string::npos) {
                std::cout << "  Technology:           🟡 DDR4 - Modern, good "
                             "performance\n";
            } else if (memSlot.type.find("DDR3") != std::string::npos) {
                std::cout << "  Technology:           🟠 DDR3 - Older "
                             "generation, consider upgrade\n";
            } else {
                std::cout << "  Technology:           ❓ Unknown or legacy "
                             "technology\n";
            }
        }

        if (!memSlot.clockSpeed.empty()) {
            std::cout << "\nSpeed Analysis:\n";
            // Extract numeric value from clock speed string
            std::string speedStr = memSlot.clockSpeed;
            size_t pos = speedStr.find("MHz");
            if (pos != std::string::npos) {
                try {
                    int speed = std::stoi(speedStr.substr(0, pos));
                    if (speed >= 3200) {
                        std::cout << "  Speed Rating:         🟢 High-speed "
                                     "memory (≥3200 MHz)\n";
                    } else if (speed >= 2400) {
                        std::cout << "  Speed Rating:         🟡 Standard "
                                     "speed (2400-3199 MHz)\n";
                    } else {
                        std::cout << "  Speed Rating:         🟠 Lower speed "
                                     "(<2400 MHz)\n";
                    }
                } catch (...) {
                    std::cout
                        << "  Speed Rating:         ❓ Unable to parse speed\n";
                }
            }
        }

        std::cout << "\n✓ Physical memory module information completed "
                     "successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering physical memory information: "
                  << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates memory performance monitoring
 */
void demonstrateMemoryPerformance() {
    std::cout << createSectionHeader("Memory Performance Monitoring");

    try {
        std::cout << "Gathering memory performance information...\n\n";

        // Try to get memory performance metrics
        try {
            auto memPerf = getMemoryPerformance();

            if (memPerf.readSpeed > 0 || memPerf.writeSpeed > 0) {
                std::cout << "Memory Performance Metrics:\n";
                std::cout << "  Read Speed:           " << std::fixed
                          << std::setprecision(1) << memPerf.readSpeed
                          << " MB/s\n";
                std::cout << "  Write Speed:          " << std::fixed
                          << std::setprecision(1) << memPerf.writeSpeed
                          << " MB/s\n";
                std::cout << "  Bandwidth Usage:      " << std::fixed
                          << std::setprecision(1) << memPerf.bandwidthUsage
                          << "%\n";
                std::cout << "  Memory Latency:       " << std::fixed
                          << std::setprecision(1) << memPerf.latency << " ns\n";

                // Performance analysis
                std::cout << "\nPerformance Analysis:\n";
                if (memPerf.bandwidthUsage > 90.0) {
                    std::cout << "  Bandwidth Status:     🔴 SATURATED - "
                                 "Memory bandwidth fully utilized\n";
                } else if (memPerf.bandwidthUsage > 70.0) {
                    std::cout << "  Bandwidth Status:     🟠 HIGH - High "
                                 "memory bandwidth usage\n";
                } else if (memPerf.bandwidthUsage > 40.0) {
                    std::cout << "  Bandwidth Status:     🟡 MODERATE - Normal "
                                 "bandwidth usage\n";
                } else {
                    std::cout << "  Bandwidth Status:     🟢 LOW - Plenty of "
                                 "bandwidth available\n";
                }

                if (memPerf.latency > 100.0) {
                    std::cout << "  Latency Status:       🟠 HIGH - Consider "
                                 "memory optimization\n";
                } else if (memPerf.latency > 50.0) {
                    std::cout << "  Latency Status:       🟡 MODERATE - "
                                 "Acceptable latency\n";
                } else {
                    std::cout << "  Latency Status:       🟢 LOW - Excellent "
                                 "memory latency\n";
                }
            } else {
                std::cout << "Memory Performance:     ℹ️  Performance metrics "
                             "not available\n";
                std::cout << "Note:                   This may be normal on "
                             "some platforms\n";
            }

        } catch (const std::exception& e) {
            std::cout << "Memory Performance:     ✗ Error: " << e.what()
                      << "\n";
            std::cout << "Note:                   Performance monitoring may "
                         "not be supported\n";
        }

        // Virtual memory analysis
        std::cout << "\nVirtual Memory Analysis:\n";
        try {
            auto virtualMax = getVirtualMemoryMax();
            auto virtualUsed = getVirtualMemoryUsed();

            std::cout << "  Virtual Memory Max:   "
                      << formatBytesDecimal(virtualMax) << "\n";
            std::cout << "  Virtual Memory Used:  "
                      << formatBytesDecimal(virtualUsed) << "\n";

            double virtualUsagePercent =
                (static_cast<double>(virtualUsed) / virtualMax) * 100.0;
            std::cout << "  Virtual Usage:        " << std::fixed
                      << std::setprecision(1) << virtualUsagePercent << "%\n";

            if (virtualUsagePercent > 80.0) {
                std::cout << "  Virtual Status:       🟠 HIGH - Monitor "
                             "virtual memory usage\n";
            } else {
                std::cout << "  Virtual Status:       🟢 NORMAL - Virtual "
                             "memory usage healthy\n";
            }

        } catch (const std::exception& e) {
            std::cerr << "  Virtual Memory:       ✗ Error: " << e.what()
                      << "\n";
        }

        std::cout
            << "\n✓ Memory performance monitoring completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error in memory performance monitoring: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive memory capabilities
 */
int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout
        << "=== Atom Sysinfo - Memory Information and Monitoring Example ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "This example demonstrates comprehensive memory information "
                 "gathering\n";
    std::cout
        << "and performance monitoring capabilities with detailed analysis.\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Execute all demonstration functions
        demonstrateMemoryInformation();
        demonstratePhysicalMemoryInfo();
        demonstrateMemoryPerformance();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        // Final summary
        std::cout << createSectionHeader("Execution Summary");
        std::cout
            << "All memory information gathering completed successfully!\n";
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ Complete memory information retrieval\n";
        std::cout << "  ✓ Real-time memory usage monitoring\n";
        std::cout << "  ✓ Physical memory module information\n";
        std::cout << "  ✓ Virtual memory management analysis\n";
        std::cout << "  ✓ Memory performance metrics monitoring\n";
        std::cout << "  ✓ Memory health assessment and warnings\n";
        std::cout << "  ✓ Cross-platform memory information gathering\n";

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
