/**
 * @file bios_example.cpp
 * @brief Comprehensive example demonstrating BIOS information gathering
 *
 * This example provides a complete demonstration of BIOS information retrieval
 * and health monitoring capabilities available in the Atom Sysinfo module.
 *
 * Features demonstrated:
 * - BIOS version and manufacturer information
 * - BIOS release date and serial number
 * - BIOS characteristics and capabilities
 * - BIOS health status checking
 * - BIOS age calculation
 * - SMBIOS data retrieval
 * - Cross-platform BIOS information gathering
 *
 * Platform support:
 * - Windows (with WMI)
 * - Linux (with /sys/class/dmi)
 * - macOS (with IOKit)
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 1.0 - Comprehensive BIOS information demonstration
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

// Atom Sysinfo module headers
#include "atom/sysinfo/hardware/bios.hpp"

using namespace atom::system;

namespace {
constexpr int DISPLAY_WIDTH = 80;
constexpr int BIOS_AGE_WARNING_DAYS = 730;    // 2 years
constexpr int BIOS_AGE_CRITICAL_DAYS = 1825;  // 5 years
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
 * @brief Demonstrates comprehensive BIOS information
 */
void demonstrateBIOSInformation() {
    std::cout << createSectionHeader("Comprehensive BIOS Information");

    try {
        std::cout << "Gathering comprehensive BIOS information...\n\n";

        auto& biosInfo = BiosInfo::getInstance();
        const auto& biosData = biosInfo.getBiosInfo();

        if (!biosData.isValid()) {
            std::cerr << "✗ Error: Unable to retrieve valid BIOS information\n";
            return;
        }

        // Basic BIOS Information
        std::cout << "Basic BIOS Information:\n";
        std::cout << "  BIOS Version:         " << biosData.version << "\n";
        std::cout << "  Manufacturer:         " << biosData.manufacturer
                  << "\n";
        std::cout << "  Release Date:         " << biosData.releaseDate << "\n";
        std::cout << "  Serial Number:        " << biosData.serialNumber
                  << "\n";
        std::cout << "  Upgradeable:          "
                  << (biosData.isUpgradeable ? "Yes" : "No") << "\n";

        // BIOS Characteristics
        if (!biosData.characteristics.empty()) {
            std::cout << "\nBIOS Characteristics:\n";
            std::cout << "  " << biosData.characteristics << "\n";
        }

        // BIOS Health Status
        std::cout << "\nBIOS Health Status:\n";
        auto healthStatus = biosInfo.checkHealth();

        std::cout << "  Overall Health:       "
                  << (healthStatus.isHealthy ? "🟢 Healthy"
                                             : "🔴 Issues Detected")
                  << "\n";
        std::cout << "  BIOS Age:             " << healthStatus.biosAgeInDays
                  << " days\n";

        if (healthStatus.biosAgeInDays > BIOS_AGE_CRITICAL_DAYS) {
            std::cout << "  Age Status:           🔴 CRITICAL - BIOS is very "
                         "old (>5 years)\n";
            std::cout
                << "                        Consider checking for updates\n";
        } else if (healthStatus.biosAgeInDays > BIOS_AGE_WARNING_DAYS) {
            std::cout << "  Age Status:           🟠 WARNING - BIOS is aging "
                         "(>2 years)\n";
        } else {
            std::cout << "  Age Status:           🟢 CURRENT - BIOS is "
                         "relatively recent\n";
        }

        // Display warnings
        if (!healthStatus.warnings.empty()) {
            std::cout << "\nWarnings:\n";
            for (const auto& warning : healthStatus.warnings) {
                std::cout << "  ⚠ " << warning << "\n";
            }
        }

        // Display errors
        if (!healthStatus.errors.empty()) {
            std::cout << "\nErrors:\n";
            for (const auto& error : healthStatus.errors) {
                std::cout << "  ✗ " << error << "\n";
            }
        }

        // SMBIOS Data
        std::cout << "\nSMBIOS Data:\n";
        try {
            auto smbiosData = biosInfo.getSMBIOSData();
            if (!smbiosData.empty()) {
                std::cout << "  SMBIOS Entries:       " << smbiosData.size()
                          << "\n";
                std::cout << "  Sample Entries:\n";
                size_t displayCount = std::min(smbiosData.size(), size_t(5));
                for (size_t i = 0; i < displayCount; ++i) {
                    std::cout << "    " << (i + 1) << ". " << smbiosData[i]
                              << "\n";
                }
                if (smbiosData.size() > 5) {
                    std::cout << "    ... and " << (smbiosData.size() - 5)
                              << " more entries\n";
                }
            } else {
                std::cout << "  No SMBIOS data available\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  SMBIOS Data:          ✗ Error: " << e.what()
                      << "\n";
        }

        std::cout << "\n✓ BIOS information gathering completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering BIOS information: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive BIOS capabilities
 */
int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "=== Atom Sysinfo - BIOS Information Example ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "This example demonstrates comprehensive BIOS information "
                 "gathering\n";
    std::cout << "and health monitoring capabilities.\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Execute demonstration
        demonstrateBIOSInformation();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        // Final summary
        std::cout << createSectionHeader("Execution Summary");
        std::cout << "BIOS information gathering completed!\n";
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ BIOS version and manufacturer information\n";
        std::cout << "  ✓ BIOS release date and characteristics\n";
        std::cout << "  ✓ BIOS health status checking\n";
        std::cout << "  ✓ BIOS age calculation and warnings\n";
        std::cout << "  ✓ SMBIOS data retrieval\n";
        std::cout << "  ✓ Cross-platform BIOS information gathering\n";

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
