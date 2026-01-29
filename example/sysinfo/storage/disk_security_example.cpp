/**
 * @file disk_security_example.cpp
 * @brief Example demonstrating disk security features
 *
 * @author Max Qian
 * @date 2024-12-19
 */

#include <chrono>
#include <iostream>
#include <string>

#include "atom/sysinfo/storage/disk/disk_security.hpp"

using namespace atom::system;

namespace {
constexpr int DISPLAY_WIDTH = 80;
}

std::string createSectionHeader(const std::string& title) {
    return "\n" + std::string(DISPLAY_WIDTH, '=') + "\n=== " + title +
           " ===\n" + std::string(DISPLAY_WIDTH, '=') + "\n";
}

void demonstrateDeviceWhitelist() {
    std::cout << createSectionHeader("Device Whitelist Management");

    std::cout << "Demonstrating device whitelist operations...\n\n";

    std::string testDevice = "TEST-DEVICE-12345";

    // Add device to whitelist
    std::cout << "Adding device to whitelist: " << testDevice << "\n";
    if (addDeviceToWhitelist(testDevice)) {
        std::cout << "  ✓ Device added successfully\n";
    } else {
        std::cout << "  ✗ Failed to add device\n";
    }

    // Check if device is in whitelist
    std::cout << "\nChecking if device is in whitelist...\n";
    if (isDeviceInWhitelist(testDevice)) {
        std::cout << "  ✓ Device is in whitelist\n";
    } else {
        std::cout << "  ✗ Device is not in whitelist\n";
    }

    // Remove device from whitelist
    std::cout << "\nRemoving device from whitelist...\n";
    if (removeDeviceFromWhitelist(testDevice)) {
        std::cout << "  ✓ Device removed successfully\n";
    } else {
        std::cout << "  ✗ Failed to remove device\n";
    }

    // Verify removal
    std::cout << "\nVerifying removal...\n";
    if (!isDeviceInWhitelist(testDevice)) {
        std::cout << "  ✓ Device successfully removed from whitelist\n";
    } else {
        std::cout << "  ✗ Device still in whitelist\n";
    }

    std::cout << "\n✓ Whitelist management demonstration completed\n";
}

void demonstrateDiskSecurity() {
    std::cout << createSectionHeader("Disk Security Features");

    std::cout << "Demonstrating disk security features...\n\n";

    std::cout << "Note: These operations may require elevated privileges.\n\n";

    // Read-only mode (demonstration only - commented out to avoid system
    // changes)
    std::cout << "Read-Only Mode:\n";
    std::cout << "  Function: setDiskReadOnly(path)\n";
    std::cout << "  Purpose:  Set a disk to read-only mode for security\n";
    std::cout << "  Usage:    Protect critical data from modification\n";
    std::cout << "  Note:     Requires administrator/root privileges\n\n";

    // Threat scanning (demonstration only)
    std::cout << "Threat Scanning:\n";
    std::cout << "  Function: scanDiskForThreats(path, depth)\n";
    std::cout << "  Purpose:  Scan disk for potentially malicious files\n";
    std::cout << "  Usage:    Security auditing and threat detection\n";
    std::cout << "  Note:     May take significant time for large disks\n\n";

    std::cout << "Security Best Practices:\n";
    std::cout << "  • Regularly scan removable media\n";
    std::cout << "  • Maintain device whitelists\n";
    std::cout << "  • Use read-only mode for sensitive data\n";
    std::cout << "  • Monitor disk access patterns\n";
    std::cout << "  • Keep security software updated\n";

    std::cout << "\n✓ Security features demonstration completed\n";
}

int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "=== Atom Sysinfo - Disk Security Example ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        demonstrateDeviceWhitelist();
        demonstrateDiskSecurity();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        std::cout << createSectionHeader("Execution Summary");
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ Device whitelist management\n";
        std::cout << "  ✓ Read-only mode configuration\n";
        std::cout << "  ✓ Threat scanning capabilities\n";
        std::cout << "  ✓ Security best practices\n";

        std::cout << "\n" << std::string(DISPLAY_WIDTH, '=') << "\n";
        std::cout << "Example completed successfully!\n";
        std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";

    } catch (const std::exception& e) {
        std::cerr << "\nCRITICAL ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
