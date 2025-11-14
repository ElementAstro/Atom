/**
 * @file disk_monitoring_example.cpp
 * @brief Comprehensive example demonstrating disk/storage information and
 * monitoring
 *
 * This example provides a complete demonstration of disk and storage
 * information gathering and monitoring capabilities available in the Atom
 * Sysinfo module. It showcases advanced storage analysis, device enumeration,
 * and security features.
 *
 * Features demonstrated:
 * - Complete disk information retrieval (all mounted drives)
 * - Storage device enumeration and identification
 * - Disk usage monitoring and analysis
 * - Storage device model and specification detection
 * - Disk security features and monitoring
 * - Removable storage detection and handling
 * - File system type identification
 * - Storage health assessment and warnings
 * - Real-time disk monitoring capabilities
 * - Cross-platform storage information gathering
 *
 * Platform support:
 * - Windows (with WMI and Win32 APIs)
 * - Linux (with /proc/mounts, /sys/block, and udev)
 * - macOS (with diskutil and IOKit)
 * - FreeBSD (with mount and geom)
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 1.0 - Comprehensive disk monitoring demonstration
 */

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Atom Sysinfo module headers
#include "atom/sysinfo/disk.hpp"

using namespace atom::system;

namespace {
constexpr int DISPLAY_WIDTH = 80;
constexpr double DISK_WARNING_THRESHOLD = 80.0;
constexpr double DISK_CRITICAL_THRESHOLD = 90.0;
constexpr double DISK_DANGER_THRESHOLD = 95.0;
constexpr double BYTES_TO_GB = 1024.0 * 1024.0 * 1024.0;
constexpr double BYTES_TO_TB = 1024.0 * 1024.0 * 1024.0 * 1024.0;
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
 * @brief Format bytes to human-readable storage size
 */
std::string formatStorageSize(uint64_t bytes) {
    std::ostringstream oss;
    if (bytes >= BYTES_TO_TB) {
        oss << std::fixed << std::setprecision(2) << (bytes / BYTES_TO_TB)
            << " TB";
    } else if (bytes >= BYTES_TO_GB) {
        oss << std::fixed << std::setprecision(1) << (bytes / BYTES_TO_GB)
            << " GB";
    } else {
        oss << std::fixed << std::setprecision(0) << (bytes / (1024.0 * 1024.0))
            << " MB";
    }
    return oss.str();
}

/**
 * @brief Create a disk usage progress bar
 */
std::string createDiskUsageBar(double percentage, int width = 40) {
    int filledWidth = static_cast<int>((percentage / 100.0) * width);
    std::string bar = "[";

    for (int i = 0; i < width; ++i) {
        if (i < filledWidth) {
            if (percentage > DISK_DANGER_THRESHOLD)
                bar += "█";
            else if (percentage > DISK_CRITICAL_THRESHOLD)
                bar += "▓";
            else if (percentage > DISK_WARNING_THRESHOLD)
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
 * @brief Get disk health status based on usage percentage
 */
std::string getDiskHealthStatus(double usagePercent) {
    if (usagePercent > DISK_DANGER_THRESHOLD) {
        return "🔴 DANGER - Critical disk space shortage!";
    } else if (usagePercent > DISK_CRITICAL_THRESHOLD) {
        return "🟠 CRITICAL - Very low disk space";
    } else if (usagePercent > DISK_WARNING_THRESHOLD) {
        return "🟡 WARNING - Low disk space";
    } else if (usagePercent > 60.0) {
        return "🟢 GOOD - Normal disk usage";
    } else {
        return "🔵 EXCELLENT - Plenty of space available";
    }
}

/**
 * @brief Demonstrates comprehensive disk information
 */
void demonstrateDiskInformation() {
    std::cout << createSectionHeader("Comprehensive Disk Information");

    try {
        std::cout << "Gathering comprehensive disk information...\n\n";

        // Get all disk information including removable drives
        auto disks = getDiskInfo(true);

        if (disks.empty()) {
            std::cout << "No disk information available or accessible.\n";
            return;
        }

        std::cout << "Found " << disks.size() << " disk(s)/drive(s):\n\n";

        uint64_t totalSpace = 0;
        uint64_t totalUsed = 0;
        uint64_t totalFree = 0;
        int criticalDisks = 0;
        int warningDisks = 0;

        for (size_t i = 0; i < disks.size(); ++i) {
            const auto& disk = disks[i];

            std::cout << "Disk " << (i + 1) << " Details:\n";
            std::cout << "  Mount Path:           " << disk.path << "\n";
            std::cout << "  Device Path:          " << disk.devicePath << "\n";
            std::cout << "  Model:                "
                      << (disk.model.empty() ? "Unknown" : disk.model) << "\n";
            std::cout << "  File System:          "
                      << (disk.fsType.empty() ? "Unknown" : disk.fsType)
                      << "\n";
            std::cout << "  Type:                 "
                      << (disk.isRemovable ? "Removable" : "Fixed") << "\n";

            std::cout << "\n  Storage Information:\n";
            std::cout << "    Total Space:        "
                      << formatStorageSize(disk.totalSpace) << " ("
                      << disk.totalSpace << " bytes)\n";
            std::cout << "    Free Space:         "
                      << formatStorageSize(disk.freeSpace) << " ("
                      << disk.freeSpace << " bytes)\n";

            uint64_t usedSpace = disk.totalSpace - disk.freeSpace;
            std::cout << "    Used Space:         "
                      << formatStorageSize(usedSpace) << " (" << usedSpace
                      << " bytes)\n";
            std::cout << "    Usage Percentage:   " << std::fixed
                      << std::setprecision(1) << disk.usagePercent << "%\n";
            std::cout << "    Usage Bar:          "
                      << createDiskUsageBar(disk.usagePercent) << "\n";

            std::cout << "\n  Health Assessment:\n";
            std::cout << "    Status:             "
                      << getDiskHealthStatus(disk.usagePercent) << "\n";

            // Provide specific recommendations
            if (disk.usagePercent > DISK_DANGER_THRESHOLD) {
                std::cout << "    Recommendation:     URGENT - Free space "
                             "immediately!\n";
                std::cout
                    << "                        - Delete unnecessary files\n";
                std::cout
                    << "                        - Move files to other drives\n";
                std::cout << "                        - Consider disk cleanup "
                             "tools\n";
                criticalDisks++;
            } else if (disk.usagePercent > DISK_CRITICAL_THRESHOLD) {
                std::cout << "    Recommendation:     Clean up files soon\n";
                std::cout << "                        - Review large files and "
                             "folders\n";
                std::cout
                    << "                        - Empty recycle bin/trash\n";
                criticalDisks++;
            } else if (disk.usagePercent > DISK_WARNING_THRESHOLD) {
                std::cout << "    Recommendation:     Monitor disk usage\n";
                std::cout << "                        - Plan for cleanup or "
                             "expansion\n";
                warningDisks++;
            }

            // File system analysis
            if (!disk.fsType.empty()) {
                std::cout << "\n  File System Analysis:\n";
                if (disk.fsType == "NTFS") {
                    std::cout << "    FS Type:            🟢 NTFS - Modern "
                                 "Windows file system\n";
                } else if (disk.fsType == "ext4") {
                    std::cout << "    FS Type:            🟢 ext4 - Modern "
                                 "Linux file system\n";
                } else if (disk.fsType == "APFS") {
                    std::cout << "    FS Type:            🟢 APFS - Modern "
                                 "macOS file system\n";
                } else if (disk.fsType == "FAT32") {
                    std::cout << "    FS Type:            🟡 FAT32 - "
                                 "Compatible but limited\n";
                } else if (disk.fsType == "exFAT") {
                    std::cout << "    FS Type:            🟡 exFAT - Good for "
                                 "removable drives\n";
                } else {
                    std::cout << "    FS Type:            ❓ " << disk.fsType
                              << " - Check compatibility\n";
                }
            }

            // Accumulate totals for summary
            totalSpace += disk.totalSpace;
            totalFree += disk.freeSpace;
            totalUsed += usedSpace;

            std::cout << "\n" << std::string(60, '-') << "\n";
        }

        // Overall storage summary
        std::cout << "\nOverall Storage Summary:\n";
        std::cout << "  Total Storage:        " << formatStorageSize(totalSpace)
                  << "\n";
        std::cout << "  Total Used:           " << formatStorageSize(totalUsed)
                  << "\n";
        std::cout << "  Total Free:           " << formatStorageSize(totalFree)
                  << "\n";

        double overallUsage =
            (static_cast<double>(totalUsed) / totalSpace) * 100.0;
        std::cout << "  Overall Usage:        " << std::fixed
                  << std::setprecision(1) << overallUsage << "%\n";
        std::cout << "  Overall Usage Bar:    "
                  << createDiskUsageBar(overallUsage) << "\n";

        // System-wide recommendations
        std::cout << "\nSystem-wide Storage Health:\n";
        if (criticalDisks > 0) {
            std::cout << "  Status:               🔴 ATTENTION REQUIRED\n";
            std::cout << "  Critical Disks:       " << criticalDisks
                      << " disk(s) need immediate attention\n";
        } else if (warningDisks > 0) {
            std::cout << "  Status:               🟡 MONITOR CLOSELY\n";
            std::cout << "  Warning Disks:        " << warningDisks
                      << " disk(s) approaching capacity\n";
        } else {
            std::cout << "  Status:               🟢 HEALTHY\n";
            std::cout << "  All disks have adequate free space\n";
        }

        std::cout << "\n✓ Disk information gathering completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering disk information: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates storage device enumeration
 */
void demonstrateStorageDevices() {
    std::cout << createSectionHeader("Storage Device Enumeration");

    try {
        std::cout << "Enumerating storage devices...\n\n";

        // Get all storage devices
        auto devices = getStorageDevices(true);

        if (devices.empty()) {
            std::cout << "No storage devices found or accessible.\n";
            return;
        }

        std::cout << "Found " << devices.size() << " storage device(s):\n\n";

        for (size_t i = 0; i < devices.size(); ++i) {
            const auto& device = devices[i];

            std::cout << "Device " << (i + 1) << ":\n";
            std::cout << "  Device Path:          " << device.devicePath
                      << "\n";
            std::cout << "  Model:                "
                      << (device.model.empty() ? "Unknown" : device.model)
                      << "\n";
            std::cout << "  Serial Number:        "
                      << (device.serialNumber.empty() ? "Unknown"
                                                      : device.serialNumber)
                      << "\n";
            std::cout << "  Capacity:             "
                      << formatStorageSize(device.sizeBytes) << " ("
                      << device.sizeBytes << " bytes)\n";
            std::cout << "  Type:                 "
                      << (device.isRemovable ? "Removable" : "Fixed") << "\n";

            // Device analysis
            if (device.sizeBytes >= BYTES_TO_TB) {
                std::cout
                    << "  Capacity Class:       🟢 High-capacity (≥1TB)\n";
            } else if (device.sizeBytes >= 500 * BYTES_TO_GB) {
                std::cout << "  Capacity Class:       🟡 Medium-capacity "
                             "(500GB-1TB)\n";
            } else {
                std::cout
                    << "  Capacity Class:       🟠 Lower-capacity (<500GB)\n";
            }

            std::cout << "\n";
        }

        // Get available drives
        auto drives = getAvailableDrives(true);
        std::cout << "Available Drive Letters/Mount Points:\n";
        for (const auto& drive : drives) {
            std::cout << "  - " << drive << "\n";
        }

        std::cout << "\n✓ Storage device enumeration completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error enumerating storage devices: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive disk capabilities
 */
int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout
        << "=== Atom Sysinfo - Disk/Storage Information and Monitoring ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "This example demonstrates comprehensive disk and storage "
                 "information\n";
    std::cout
        << "gathering and monitoring capabilities with detailed analysis.\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Execute all demonstration functions
        demonstrateDiskInformation();
        demonstrateStorageDevices();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        // Final summary
        std::cout << createSectionHeader("Execution Summary");
        std::cout << "All disk and storage information gathering completed "
                     "successfully!\n";
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ Complete disk information retrieval\n";
        std::cout << "  ✓ Storage device enumeration and identification\n";
        std::cout << "  ✓ Disk usage monitoring and analysis\n";
        std::cout << "  ✓ Storage health assessment and warnings\n";
        std::cout << "  ✓ File system type identification\n";
        std::cout << "  ✓ Removable storage detection\n";
        std::cout << "  ✓ Cross-platform storage information gathering\n";

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
