/**
 * @file disk_device_example.cpp
 * @brief Comprehensive example demonstrating disk device information
 *
 * This example provides a complete demonstration of disk device enumeration
 * and information gathering capabilities available in the Atom Sysinfo module.
 *
 * Features demonstrated:
 * - Storage device enumeration
 * - Device model information retrieval
 * - Available drives listing
 * - Device serial number retrieval
 * - Disk health monitoring
 * - Cross-platform disk device information gathering
 *
 * Platform support:
 * - Windows (with WMI and Win32 APIs)
 * - Linux (with /sys/block and udev)
 * - macOS (with IOKit and DiskArbitration)
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 1.0 - Comprehensive disk device information demonstration
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

// Atom Sysinfo module headers
#include "atom/sysinfo/storage/disk/disk_device.hpp"

using namespace atom::system;

namespace {
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
 * @brief Demonstrates storage device enumeration
 */
void demonstrateStorageDevices() {
    std::cout << createSectionHeader("Storage Device Enumeration");

    try {
        std::cout << "Enumerating storage devices...\n\n";

        auto devices = getStorageDevices(true);

        if (devices.empty()) {
            std::cout << "No storage devices detected.\n";
            return;
        }

        std::cout << "Total Storage Devices: " << devices.size() << "\n\n";

        for (size_t i = 0; i < devices.size(); ++i) {
            const auto& device = devices[i];

            std::cout << "Device " << (i + 1) << ":\n";
            std::cout << "  Path:                 " << device.devicePath
                      << "\n";
            std::cout << "  Model:                " << device.model << "\n";
            std::cout << "  Type:                 "
                      << (device.isRemovable ? "Removable" : "Fixed") << "\n";

            // Get serial number if available
            try {
                auto serial = getDeviceSerialNumber(device.devicePath);
                if (serial.has_value()) {
                    std::cout << "  Serial Number:        " << serial.value()
                              << "\n";
                } else {
                    std::cout << "  Serial Number:        Not available\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "  Serial Number:        ✗ Error: " << e.what()
                          << "\n";
            }

            // Get disk health if available
            try {
                auto health = getDiskHealth(device.devicePath);
                if (std::holds_alternative<int>(health)) {
                    int healthPercent = std::get<int>(health);
                    std::cout << "  Health:               " << healthPercent
                              << "%\n";

                    if (healthPercent >= 90) {
                        std::cout << "  Health Status:        🟢 EXCELLENT\n";
                    } else if (healthPercent >= 70) {
                        std::cout << "  Health Status:        🟡 GOOD\n";
                    } else if (healthPercent >= 50) {
                        std::cout << "  Health Status:        🟠 WARNING - "
                                     "Monitor closely\n";
                    } else {
                        std::cout << "  Health Status:        🔴 CRITICAL - "
                                     "Backup data immediately\n";
                    }
                } else {
                    std::string error = std::get<std::string>(health);
                    std::cout << "  Health:               " << error << "\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "  Health:               ✗ Error: " << e.what()
                          << "\n";
            }

            if (i < devices.size() - 1) {
                std::cout << "\n";
            }
        }

        std::cout << "\n✓ Storage device enumeration completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error enumerating storage devices: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates available drives listing
 */
void demonstrateAvailableDrives() {
    std::cout << createSectionHeader("Available Drives");

    try {
        std::cout << "Listing available drives...\n\n";

        auto drives = getAvailableDrives(true);

        if (drives.empty()) {
            std::cout << "No drives detected.\n";
            return;
        }

        std::cout << "Total Available Drives: " << drives.size() << "\n\n";

        for (size_t i = 0; i < drives.size(); ++i) {
            std::cout << "  " << (i + 1) << ". " << drives[i] << "\n";
        }

        std::cout << "\n✓ Drive listing completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error listing drives: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive disk device capabilities
 */
int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "=== Atom Sysinfo - Disk Device Information Example ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "This example demonstrates comprehensive disk device "
                 "enumeration\n";
    std::cout << "and information gathering capabilities.\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Execute all demonstration functions
        demonstrateStorageDevices();
        demonstrateAvailableDrives();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        // Final summary
        std::cout << createSectionHeader("Execution Summary");
        std::cout << "Disk device information gathering completed!\n";
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ Storage device enumeration\n";
        std::cout << "  ✓ Device model information retrieval\n";
        std::cout << "  ✓ Available drives listing\n";
        std::cout << "  ✓ Device serial number retrieval\n";
        std::cout << "  ✓ Disk health monitoring\n";
        std::cout << "  ✓ Cross-platform disk device information gathering\n";

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
