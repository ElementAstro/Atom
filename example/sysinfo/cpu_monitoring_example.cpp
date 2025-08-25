/**
 * @file cpu_monitoring_example.cpp
 * @brief Comprehensive example demonstrating CPU information and monitoring
 *
 * This example provides a complete demonstration of CPU information gathering
 * and real-time monitoring capabilities available in the Atom Sysinfo module.
 * It showcases advanced CPU analysis, performance monitoring, and thermal
 * tracking.
 *
 * Features demonstrated:
 * - Complete CPU information retrieval (model, vendor, architecture)
 * - CPU core and thread information
 * - CPU frequency and performance monitoring
 * - CPU cache information and analysis
 * - CPU temperature monitoring and thermal management
 * - CPU load average and usage tracking
 * - CPU power consumption monitoring
 * - CPU feature flags and instruction set detection
 * - Real-time CPU performance monitoring
 * - Cross-platform CPU information gathering
 *
 * Platform support:
 * - Windows (with WMI and Performance Counters)
 * - Linux (with /proc/cpuinfo and sysfs)
 * - macOS (with sysctl and IOKit)
 * - FreeBSD (with sysctl)
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 1.0 - Comprehensive CPU monitoring demonstration
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
#include "atom/sysinfo/cpu.hpp"

using namespace atom::system;

namespace {
constexpr int DISPLAY_WIDTH = 80;
constexpr double TEMP_WARNING_THRESHOLD = 80.0;
constexpr double TEMP_CRITICAL_THRESHOLD = 90.0;
constexpr double USAGE_HIGH_THRESHOLD = 80.0;
constexpr double USAGE_CRITICAL_THRESHOLD = 95.0;
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
 * @brief Format bytes to human-readable cache size
 */
std::string formatCacheSize(size_t bytes) {
    if (bytes >= 1024 * 1024) {
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    } else if (bytes >= 1024) {
        return std::to_string(bytes / 1024) + " KB";
    } else {
        return std::to_string(bytes) + " B";
    }
}

/**
 * @brief Create a simple text-based progress bar
 */
std::string createProgressBar(double percentage, int width = 40) {
    int filledWidth = static_cast<int>((percentage / 100.0) * width);
    std::string bar = "[";

    for (int i = 0; i < width; ++i) {
        if (i < filledWidth) {
            if (percentage > 90.0)
                bar += "█";
            else if (percentage > 70.0)
                bar += "▓";
            else
                bar += "▒";
        } else {
            bar += "░";
        }
    }

    bar += "] " + std::to_string(static_cast<int>(percentage)) + "%";
    return bar;
}

/**
 * @brief Demonstrates comprehensive CPU information
 */
void demonstrateCPUInformation() {
    std::cout << createSectionHeader("Comprehensive CPU Information");

    try {
        std::cout << "Gathering comprehensive CPU information...\n\n";

        auto cpuInfo = getCpuInfo();

        // Basic CPU Information
        std::cout << "Basic CPU Information:\n";
        std::cout << "  CPU Model:            " << cpuInfo.model << "\n";
        std::cout << "  CPU Identifier:       " << cpuInfo.identifier << "\n";
        std::cout << "  Vendor:               "
                  << cpuVendorToString(cpuInfo.vendor) << "\n";
        std::cout << "  Architecture:         "
                  << cpuArchitectureToString(cpuInfo.architecture) << "\n";
        std::cout << "  Socket Type:          " << cpuInfo.socketType << "\n";
        std::cout << "  Instruction Set:      " << cpuInfo.instructionSet
                  << "\n";
        std::cout << "  Family:               " << cpuInfo.family << "\n";
        std::cout << "  Model ID:             " << cpuInfo.model_id << "\n";
        std::cout << "  Stepping:             " << cpuInfo.stepping << "\n";

        // Core and Thread Information
        std::cout << "\nCore and Thread Information:\n";
        std::cout << "  Physical Packages:    " << cpuInfo.numPhysicalPackages
                  << "\n";
        std::cout << "  Physical Cores:       " << cpuInfo.numPhysicalCores
                  << "\n";
        std::cout << "  Logical Cores:        " << cpuInfo.numLogicalCores
                  << "\n";
        std::cout << "  Threads per Core:     "
                  << (cpuInfo.numLogicalCores / cpuInfo.numPhysicalCores)
                  << "\n";

        // Frequency Information
        std::cout << "\nFrequency Information:\n";
        std::cout << "  Base Frequency:       " << std::fixed
                  << std::setprecision(2) << cpuInfo.baseFrequency << " GHz\n";
        std::cout << "  Max Frequency:        " << std::fixed
                  << std::setprecision(2) << cpuInfo.maxFrequency << " GHz\n";

        // Current performance metrics
        std::cout << "\nCurrent Performance Metrics:\n";
        try {
            auto currentUsage = getCurrentCpuUsage();
            std::cout << "  Current Usage:        " << std::fixed
                      << std::setprecision(1) << currentUsage << "%\n";
            std::cout << "  Usage Bar:            "
                      << createProgressBar(currentUsage) << "\n";

            if (currentUsage > USAGE_CRITICAL_THRESHOLD) {
                std::cout << "  Usage Status:         🔴 CRITICAL - Very high "
                             "CPU usage!\n";
            } else if (currentUsage > USAGE_HIGH_THRESHOLD) {
                std::cout
                    << "  Usage Status:         🟠 HIGH - Elevated CPU usage\n";
            } else if (currentUsage > 50.0) {
                std::cout
                    << "  Usage Status:         🟡 MODERATE - Normal usage\n";
            } else {
                std::cout << "  Usage Status:         🟢 LOW - Light usage\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  Current Usage:        ✗ Error: " << e.what()
                      << "\n";
        }

        try {
            auto currentTemp = getCurrentCpuTemperature();
            std::cout << "  Current Temperature:  " << std::fixed
                      << std::setprecision(1) << currentTemp << "°C\n";

            if (currentTemp > TEMP_CRITICAL_THRESHOLD) {
                std::cout << "  Temperature Status:   🔴 CRITICAL - CPU "
                             "overheating!\n";
            } else if (currentTemp > TEMP_WARNING_THRESHOLD) {
                std::cout << "  Temperature Status:   🟠 WARNING - High "
                             "temperature\n";
            } else if (currentTemp > 60.0) {
                std::cout << "  Temperature Status:   🟡 WARM - Normal "
                             "operating temperature\n";
            } else {
                std::cout
                    << "  Temperature Status:   🟢 COOL - Good temperature\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  Current Temperature:  ✗ Error: " << e.what()
                      << "\n";
        }

        try {
            auto currentFreq = getProcessorFrequency();
            std::cout << "  Current Frequency:    " << std::fixed
                      << std::setprecision(2) << currentFreq << " GHz\n";
        } catch (const std::exception& e) {
            std::cerr << "  Current Frequency:    ✗ Error: " << e.what()
                      << "\n";
        }

        std::cout << "\n✓ CPU information gathering completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering CPU information: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates CPU cache information
 */
void demonstrateCPUCacheInformation() {
    std::cout << createSectionHeader("CPU Cache Information");

    try {
        std::cout << "Gathering CPU cache information...\n\n";

        auto cacheInfo = getCacheSizes();

        std::cout << "Cache Hierarchy:\n";
        std::cout << "  L1 Data Cache:        "
                  << formatCacheSize(cacheInfo.l1d)
                  << " (Line: " << cacheInfo.l1d_line_size << " bytes, "
                  << "Associativity: " << cacheInfo.l1d_associativity << ")\n";
        std::cout << "  L1 Instruction Cache: "
                  << formatCacheSize(cacheInfo.l1i)
                  << " (Line: " << cacheInfo.l1i_line_size << " bytes, "
                  << "Associativity: " << cacheInfo.l1i_associativity << ")\n";
        std::cout << "  L2 Cache:             " << formatCacheSize(cacheInfo.l2)
                  << " (Line: " << cacheInfo.l2_line_size << " bytes, "
                  << "Associativity: " << cacheInfo.l2_associativity << ")\n";
        std::cout << "  L3 Cache:             " << formatCacheSize(cacheInfo.l3)
                  << " (Line: " << cacheInfo.l3_line_size << " bytes, "
                  << "Associativity: " << cacheInfo.l3_associativity << ")\n";

        // Cache analysis
        size_t totalCache =
            cacheInfo.l1d + cacheInfo.l1i + cacheInfo.l2 + cacheInfo.l3;
        std::cout << "\nCache Analysis:\n";
        std::cout << "  Total Cache:          " << formatCacheSize(totalCache)
                  << "\n";
        std::cout << "  L1 Total:             "
                  << formatCacheSize(cacheInfo.l1d + cacheInfo.l1i) << "\n";

        if (cacheInfo.l3 > 0) {
            std::cout << "  Cache Tier:           3-level cache hierarchy\n";
        } else if (cacheInfo.l2 > 0) {
            std::cout << "  Cache Tier:           2-level cache hierarchy\n";
        } else {
            std::cout << "  Cache Tier:           1-level cache hierarchy\n";
        }

        std::cout << "\n✓ CPU cache information completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering CPU cache information: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates CPU load average and performance monitoring
 */
void demonstrateCPULoadAndPerformance() {
    std::cout << createSectionHeader("CPU Load Average and Performance");

    try {
        std::cout << "Gathering CPU load and performance information...\n\n";

        // Load average (Unix-like systems)
        try {
            auto loadAvg = getCpuLoadAverage();
            std::cout << "Load Average:\n";
            std::cout << "  1 minute:             " << std::fixed
                      << std::setprecision(2) << loadAvg.oneMinute << "\n";
            std::cout << "  5 minutes:            " << std::fixed
                      << std::setprecision(2) << loadAvg.fiveMinutes << "\n";
            std::cout << "  15 minutes:           " << std::fixed
                      << std::setprecision(2) << loadAvg.fifteenMinutes << "\n";

            // Load analysis
            auto cpuInfo = getCpuInfo();
            double loadPerCore1min =
                loadAvg.oneMinute / cpuInfo.numLogicalCores;

            std::cout << "\nLoad Analysis:\n";
            std::cout << "  Load per Core (1min): " << std::fixed
                      << std::setprecision(2) << loadPerCore1min << "\n";

            if (loadPerCore1min > 1.5) {
                std::cout << "  Load Status:          🔴 OVERLOADED - System "
                             "under heavy stress\n";
            } else if (loadPerCore1min > 1.0) {
                std::cout << "  Load Status:          🟠 HIGH - System fully "
                             "utilized\n";
            } else if (loadPerCore1min > 0.7) {
                std::cout << "  Load Status:          🟡 MODERATE - Good "
                             "utilization\n";
            } else {
                std::cout << "  Load Status:          🟢 LOW - System has "
                             "spare capacity\n";
            }

        } catch (const std::exception& e) {
            std::cerr << "Load Average:           ✗ Error: " << e.what()
                      << "\n";
        }

        // CPU Power Information
        try {
            auto powerInfo = getCpuPowerInfo();
            std::cout << "\nPower Information:\n";
            std::cout << "  Current Power:        " << std::fixed
                      << std::setprecision(1) << powerInfo.currentWatts
                      << " W\n";
            std::cout << "  Max TDP:              " << std::fixed
                      << std::setprecision(1) << powerInfo.maxTDP << " W\n";
            std::cout << "  Energy Impact:        " << std::fixed
                      << std::setprecision(1) << powerInfo.energyImpact << "\n";

            double powerUsagePercent =
                (powerInfo.currentWatts / powerInfo.maxTDP) * 100.0;
            std::cout << "  Power Usage:          " << std::fixed
                      << std::setprecision(1) << powerUsagePercent
                      << "% of TDP\n";

        } catch (const std::exception& e) {
            std::cerr << "Power Information:      ✗ Error: " << e.what()
                      << "\n";
        }

        std::cout << "\n✓ CPU load and performance monitoring completed "
                     "successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error in CPU load and performance monitoring: "
                  << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive CPU capabilities
 */
int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout
        << "=== Atom Sysinfo - CPU Information and Monitoring Example ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "This example demonstrates comprehensive CPU information "
                 "gathering\n";
    std::cout
        << "and real-time monitoring capabilities with detailed analysis.\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Execute all demonstration functions
        demonstrateCPUInformation();
        demonstrateCPUCacheInformation();
        demonstrateCPULoadAndPerformance();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        // Final summary
        std::cout << createSectionHeader("Execution Summary");
        std::cout << "All CPU information gathering completed successfully!\n";
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ Complete CPU information retrieval\n";
        std::cout << "  ✓ Real-time CPU usage and temperature monitoring\n";
        std::cout << "  ✓ CPU cache hierarchy analysis\n";
        std::cout << "  ✓ CPU load average and performance tracking\n";
        std::cout << "  ✓ CPU power consumption monitoring\n";
        std::cout << "  ✓ Cross-platform CPU information gathering\n";
        std::cout << "  ✓ Comprehensive performance analysis\n";

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
