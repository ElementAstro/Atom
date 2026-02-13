/**
 * @file backward_compatibility.cpp
 * @brief Backward compatibility example for the atom_sysinfo_sn library
 *
 * This example demonstrates how the original HardwareInfo API continues
 * to work while providing access to enhanced features.
 */

#include "../sn.hpp"  // Original header for backward compatibility
#include <iostream>
#include <iomanip>

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void demonstrateOriginalAPI() {
    printSeparator("Original API (Backward Compatible)");

    // Create HardwareInfo instance using original API
    HardwareInfo hwInfo;

    std::cout << "Using original HardwareInfo API:\n\n";

    // Get hardware serial numbers using original methods
    std::string biosSerial = hwInfo.getBiosSerialNumber();
    std::string mbSerial = hwInfo.getMotherboardSerialNumber();
    std::string cpuSerial = hwInfo.getCpuSerialNumber();
    auto diskSerials = hwInfo.getDiskSerialNumbers();

    // Display results
    std::cout << "BIOS Serial:        "
              << (biosSerial.empty() ? "Not available" : biosSerial) << "\n";
    std::cout << "Motherboard Serial: "
              << (mbSerial.empty() ? "Not available" : mbSerial) << "\n";
    std::cout << "CPU Serial:         "
              << (cpuSerial.empty() ? "Not available" : cpuSerial) << "\n";

    std::cout << "Disk Serials:       ";
    if (diskSerials.empty()) {
        std::cout << "Not available";
    } else {
        for (size_t i = 0; i < diskSerials.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << diskSerials[i];
        }
    }
    std::cout << "\n";
}

void demonstrateEnhancedFeatures() {
    printSeparator("Enhanced Features (New)");

    HardwareInfo hwInfo;

    // Check if enhanced mode is available
    bool enhancedAvailable = hwInfo.isEnhancedModeAvailable();
    std::cout << "Enhanced Mode Available: " << (enhancedAvailable ? "Yes" : "No") << "\n";

    if (enhancedAvailable) {
        // Get system fingerprint (new feature)
        std::string fingerprint = hwInfo.getSystemFingerprint();
        std::cout << "System Fingerprint: " << fingerprint << "\n";

        // Access enhanced system info
        auto* enhancedSysInfo = hwInfo.getEnhancedSystemInfo();
        if (enhancedSysInfo) {
            std::cout << "\nAccessing enhanced system information:\n";
            std::cout << "Platform: " << enhancedSysInfo->getPlatformName() << "\n";
            std::cout << "Supported: " << (enhancedSysInfo->isSupported() ? "Yes" : "No") << "\n";

            // Get comprehensive information
            auto result = enhancedSysInfo->getComprehensiveInfo();
            if (result.success) {
                std::cout << "System UUID: "
                          << (result.data.systemId.systemUuid.empty() ?
                              "Not available" : result.data.systemId.systemUuid) << "\n";
                std::cout << "Machine ID: "
                          << (result.data.systemId.machineId.empty() ?
                              "Not available" : result.data.systemId.machineId) << "\n";
                std::cout << "Hostname: "
                          << (result.data.systemId.hostname.empty() ?
                              "Not available" : result.data.systemId.hostname) << "\n";

                // Show memory modules if available
                if (!result.data.memoryModules.empty()) {
                    std::cout << "\nMemory Modules:\n";
                    for (size_t i = 0; i < result.data.memoryModules.size(); ++i) {
                        const auto& module = result.data.memoryModules[i];
                        std::cout << "  Module " << (i + 1) << ": ";
                        if (module.sizeBytes > 0) {
                            double sizeGB = static_cast<double>(module.sizeBytes) / (1024.0 * 1024.0 * 1024.0);
                            std::cout << std::fixed << std::setprecision(1) << sizeGB << " GB";
                        }
                        if (!module.manufacturer.empty()) {
                            std::cout << " (" << module.manufacturer << ")";
                        }
                        std::cout << "\n";
                    }
                }

                // Show network interfaces if available
                if (!result.data.networkInterfaces.empty()) {
                    std::cout << "\nNetwork Interfaces:\n";
                    for (const auto& interface : result.data.networkInterfaces) {
                        std::cout << "  " << interface.name << ": " << interface.macAddress;
                        if (!interface.type.empty()) {
                            std::cout << " (" << interface.type << ")";
                        }
                        std::cout << "\n";
                    }
                }
            }
        }
    } else {
        std::cout << "Enhanced features are not available on this system.\n";
        std::cout << "Only basic hardware serial numbers are accessible.\n";
    }
}

void demonstrateUtilityFunctions() {
    printSeparator("Utility Functions");

    // Test utility functions
    std::cout << "Quick Fingerprint: " << HardwareInfoUtils::getQuickFingerprint() << "\n";
    std::cout << "System Available: " << (HardwareInfoUtils::isAvailable() ? "Yes" : "No") << "\n";

    std::cout << "\nFormatted Serial Numbers:\n";
    std::cout << HardwareInfoUtils::getFormattedSerials();
}

void demonstrateTypeAliases() {
    printSeparator("Type Aliases");

    // Demonstrate type aliases for backward compatibility
    std::cout << "Testing type aliases:\n\n";

    // SystemHardwareInfo is an alias for HardwareInfo
    SystemHardwareInfo sysHwInfo;
    std::cout << "SystemHardwareInfo BIOS Serial: " << sysHwInfo.getBiosSerialNumber() << "\n";

    // HwInfo is an alias for HardwareInfo
    HwInfo hwInfoAlias;
    std::cout << "HwInfo BIOS Serial: " << hwInfoAlias.getBiosSerialNumber() << "\n";

    // All should return the same values
    HardwareInfo originalHwInfo;
    std::string original = originalHwInfo.getBiosSerialNumber();
    std::string alias1 = sysHwInfo.getBiosSerialNumber();
    std::string alias2 = hwInfoAlias.getBiosSerialNumber();

    std::cout << "\nConsistency check:\n";
    std::cout << "Original == SystemHardwareInfo: " << (original == alias1 ? "Yes" : "No") << "\n";
    std::cout << "Original == HwInfo: " << (original == alias2 ? "Yes" : "No") << "\n";
    std::cout << "SystemHardwareInfo == HwInfo: " << (alias1 == alias2 ? "Yes" : "No") << "\n";
}

void demonstrateCopyAndMove() {
    printSeparator("Copy and Move Operations");

    // Create original instance
    HardwareInfo original;
    std::string originalBios = original.getBiosSerialNumber();

    std::cout << "Original BIOS Serial: " << originalBios << "\n";

    // Test copy constructor
    HardwareInfo copied(original);
    std::string copiedBios = copied.getBiosSerialNumber();
    std::cout << "Copied BIOS Serial: " << copiedBios << "\n";
    std::cout << "Copy successful: " << (originalBios == copiedBios ? "Yes" : "No") << "\n";

    // Test copy assignment
    HardwareInfo assigned;
    assigned = original;
    std::string assignedBios = assigned.getBiosSerialNumber();
    std::cout << "Assigned BIOS Serial: " << assignedBios << "\n";
    std::cout << "Assignment successful: " << (originalBios == assignedBios ? "Yes" : "No") << "\n";

    // Test move constructor
    HardwareInfo moved(std::move(original));
    std::string movedBios = moved.getBiosSerialNumber();
    std::cout << "Moved BIOS Serial: " << movedBios << "\n";
    std::cout << "Move successful: " << (originalBios == movedBios ? "Yes" : "No") << "\n";
}

int main() {
    std::cout << "Atom System Information - Backward Compatibility Example\n";
    std::cout << "========================================================\n";

    try {
        // Demonstrate original API still works
        demonstrateOriginalAPI();

        // Show enhanced features accessible through original API
        demonstrateEnhancedFeatures();

        // Test utility functions
        demonstrateUtilityFunctions();

        // Test type aliases
        demonstrateTypeAliases();

        // Test copy and move operations
        demonstrateCopyAndMove();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "Backward compatibility example completed successfully!\n";
        std::cout << "The original API continues to work while providing\n";
        std::cout << "access to enhanced features when available.\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred\n";
        return 1;
    }

    return 0;
}
