/**
 * @file sn_example.cpp
 * @brief Comprehensive example demonstrating hardware serial number retrieval
 *
 * This example provides a complete demonstration of hardware serial number
 * gathering capabilities available in the Atom Sysinfo module.
 *
 * Features demonstrated:
 * - BIOS serial number retrieval
 * - Motherboard serial number retrieval
 * - CPU serial number retrieval
 * - Disk serial numbers retrieval
 * - Cross-platform hardware identification
 *
 * Platform support:
 * - Windows (with WMI)
 * - Linux (with /sys/class/dmi and filesystem)
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 1.0 - Comprehensive hardware serial number demonstration
 */

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

// Atom Sysinfo module headers
#include "atom/sysinfo/info/sn.hpp"

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
 * @brief Mask serial number for privacy (show only first and last few chars)
 */
std::string maskSerialNumber(const std::string& serial) {
    if (serial.empty() || serial == "N/A" || serial == "Unknown") {
        return serial;
    }

    if (serial.length() <= 8) {
        return serial.substr(0, 2) + "****" +
               serial.substr(serial.length() - 2);
    }

    return serial.substr(0, 4) + "********" +
           serial.substr(serial.length() - 4);
}

/**
 * @brief Demonstrates hardware serial number information
 */
void demonstrateSerialNumbers() {
    std::cout << createSectionHeader("Hardware Serial Numbers");

    try {
        std::cout << "Gathering hardware serial numbers...\n";
        std::cout << "Note: Serial numbers are masked for privacy in this "
                     "example.\n\n";

        HardwareInfo hwInfo;

        // BIOS Serial Number
        std::cout << "BIOS Information:\n";
        try {
            auto biosSerial = hwInfo.getBiosSerialNumber();
            std::cout << "  BIOS Serial:          ";
            if (!biosSerial.empty()) {
                std::cout << maskSerialNumber(biosSerial) << "\n";
            } else {
                std::cout << "Not available\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  BIOS Serial:          ✗ Error: " << e.what()
                      << "\n";
        }

        // Motherboard Serial Number
        std::cout << "\nMotherboard Information:\n";
        try {
            auto mbSerial = hwInfo.getMotherboardSerialNumber();
            std::cout << "  Motherboard Serial:   ";
            if (!mbSerial.empty()) {
                std::cout << maskSerialNumber(mbSerial) << "\n";
            } else {
                std::cout << "Not available\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  Motherboard Serial:   ✗ Error: " << e.what()
                      << "\n";
        }

        // CPU Serial Number
        std::cout << "\nCPU Information:\n";
        try {
            auto cpuSerial = hwInfo.getCpuSerialNumber();
            std::cout << "  CPU Serial:           ";
            if (!cpuSerial.empty()) {
                std::cout << maskSerialNumber(cpuSerial) << "\n";
            } else {
                std::cout << "Not available\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  CPU Serial:           ✗ Error: " << e.what()
                      << "\n";
        }

        // Disk Serial Numbers
        std::cout << "\nDisk Information:\n";
        try {
            auto diskSerials = hwInfo.getDiskSerialNumbers();
            if (!diskSerials.empty()) {
                std::cout << "  Total Disks:          " << diskSerials.size()
                          << "\n";
                for (size_t i = 0; i < diskSerials.size(); ++i) {
                    std::cout << "  Disk " << (i + 1) << " Serial:       "
                              << maskSerialNumber(diskSerials[i]) << "\n";
                }
            } else {
                std::cout << "  Disk Serials:         Not available\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  Disk Serials:         ✗ Error: " << e.what()
                      << "\n";
        }

        // Usage Information
        std::cout << "\nSerial Number Usage:\n";
        std::cout << "  Serial numbers can be used for:\n";
        std::cout << "    • Hardware identification and tracking\n";
        std::cout << "    • License management and validation\n";
        std::cout << "    • Asset management in organizations\n";
        std::cout << "    • Warranty and support verification\n";
        std::cout << "    • Security and authentication purposes\n";

        std::cout << "\n✓ Hardware serial number gathering completed "
                     "successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering serial numbers: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive serial number capabilities
 */
int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "=== Atom Sysinfo - Hardware Serial Number Example ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "This example demonstrates comprehensive hardware serial "
                 "number\n";
    std::cout << "retrieval capabilities.\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Execute demonstration
        demonstrateSerialNumbers();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        // Final summary
        std::cout << createSectionHeader("Execution Summary");
        std::cout << "Serial number gathering completed!\n";
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ BIOS serial number retrieval\n";
        std::cout << "  ✓ Motherboard serial number retrieval\n";
        std::cout << "  ✓ CPU serial number retrieval\n";
        std::cout << "  ✓ Disk serial numbers retrieval\n";
        std::cout << "  ✓ Cross-platform hardware identification\n";

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
