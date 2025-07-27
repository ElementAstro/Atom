/**
 * @file advanced_features.cpp
 * @brief Advanced features example for the atom_sysinfo_sn library
 *
 * This example demonstrates advanced features like caching, anonymization,
 * license key generation, and system change detection.
 */

#include "sn.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>

using namespace atom::system;

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void demonstrateCaching() {
    printSeparator("Caching Demonstration");

    // Configure caching
    SystemInfoConfig config;
    config.cacheResults = true;
    config.cacheTimeout = std::chrono::seconds(5);
    config.enableLogging = true;

    auto sysInfo = createSystemInfo(config);

    std::cout << "Cache configuration:\n";
    std::cout << "  Caching enabled: " << (config.cacheResults ? "Yes" : "No") << "\n";
    std::cout << "  Cache timeout: " << config.cacheTimeout.count() << " seconds\n\n";

    // First call - should populate cache
    std::cout << "First call (populating cache):\n";
    auto start = std::chrono::high_resolution_clock::now();
    auto result1 = sysInfo->getHardwareSerials();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "  Duration: " << duration1.count() << " ms\n";
    std::cout << "  Cache valid: " << (sysInfo->isCacheValid() ? "Yes" : "No") << "\n";
    std::cout << "  Cache age: " << sysInfo->getCacheAge().count() << " seconds\n";

    // Second call - should use cache
    std::cout << "\nSecond call (using cache):\n";
    start = std::chrono::high_resolution_clock::now();
    auto result2 = sysInfo->getHardwareSerials();
    end = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "  Duration: " << duration2.count() << " ms\n";
    std::cout << "  Cache valid: " << (sysInfo->isCacheValid() ? "Yes" : "No") << "\n";
    std::cout << "  Cache age: " << sysInfo->getCacheAge().count() << " seconds\n";

    // Verify results are identical
    if (result1.success && result2.success) {
        bool identical = (result1.data.biosSerial == result2.data.biosSerial &&
                         result1.data.motherboardSerial == result2.data.motherboardSerial &&
                         result1.data.cpuSerial == result2.data.cpuSerial);
        std::cout << "  Results identical: " << (identical ? "Yes" : "No") << "\n";
    }

    // Wait for cache to expire
    std::cout << "\nWaiting for cache to expire...\n";
    std::this_thread::sleep_for(std::chrono::seconds(6));

    std::cout << "After cache expiration:\n";
    std::cout << "  Cache valid: " << (sysInfo->isCacheValid() ? "Yes" : "No") << "\n";
    std::cout << "  Cache age: " << sysInfo->getCacheAge().count() << " seconds\n";

    // Third call - should refresh cache
    std::cout << "\nThird call (refreshing cache):\n";
    start = std::chrono::high_resolution_clock::now();
    auto result3 = sysInfo->getHardwareSerials();
    end = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "  Duration: " << duration3.count() << " ms\n";
    std::cout << "  Cache valid: " << (sysInfo->isCacheValid() ? "Yes" : "No") << "\n";
    std::cout << "  Cache age: " << sysInfo->getCacheAge().count() << " seconds\n";
}

void demonstrateAnonymization() {
    printSeparator("Data Anonymization");

    auto sysInfo = createSystemInfo();
    auto result = sysInfo->getComprehensiveInfo();

    if (result.success) {
        std::cout << "Original system information:\n";
        std::cout << "  BIOS Serial: " << result.data.hardwareSerials.biosSerial << "\n";
        std::cout << "  Motherboard Serial: " << result.data.hardwareSerials.motherboardSerial << "\n";
        std::cout << "  CPU Serial: " << result.data.hardwareSerials.cpuSerial << "\n";
        std::cout << "  System UUID: " << result.data.systemId.systemUuid << "\n";
        std::cout << "  Hostname: " << result.data.systemId.hostname << "\n";

        if (!result.data.systemId.macAddresses.empty()) {
            std::cout << "  MAC Address: " << result.data.systemId.macAddresses[0] << "\n";
        }

        // Anonymize the data
        auto anonymized = SystemInfoHelpers::anonymizeSystemInfo(result.data);

        std::cout << "\nAnonymized system information:\n";
        std::cout << "  BIOS Serial: " << anonymized.hardwareSerials.biosSerial << "\n";
        std::cout << "  Motherboard Serial: " << anonymized.hardwareSerials.motherboardSerial << "\n";
        std::cout << "  CPU Serial: " << anonymized.hardwareSerials.cpuSerial << "\n";
        std::cout << "  System UUID: " << anonymized.systemId.systemUuid << "\n";
        std::cout << "  Hostname: " << anonymized.systemId.hostname << "\n";

        if (!anonymized.systemId.macAddresses.empty()) {
            std::cout << "  MAC Address: " << anonymized.systemId.macAddresses[0] << "\n";
        }

        std::cout << "\nAnonymization preserves structure while protecting privacy.\n";
    } else {
        std::cout << "Failed to get system information for anonymization demo.\n";
    }
}

void demonstrateLicenseKeyGeneration() {
    printSeparator("Hardware License Key Generation");

    auto sysInfo = createSystemInfo();
    auto result = sysInfo->getComprehensiveInfo();

    if (result.success) {
        // Generate license keys with different salts
        std::string licenseKey1 = SystemInfoHelpers::generateHardwareLicenseKey(result.data);
        std::string licenseKey2 = SystemInfoHelpers::generateHardwareLicenseKey(result.data, "salt123");
        std::string licenseKey3 = SystemInfoHelpers::generateHardwareLicenseKey(result.data, "different_salt");

        std::cout << "Generated license keys:\n";
        std::cout << "  No salt:         " << licenseKey1 << "\n";
        std::cout << "  With 'salt123':  " << licenseKey2 << "\n";
        std::cout << "  With 'different_salt': " << licenseKey3 << "\n";

        // Validate license keys
        std::cout << "\nLicense key validation:\n";
        bool valid1 = SystemInfoHelpers::validateHardwareLicenseKey(licenseKey1, result.data);
        bool valid2 = SystemInfoHelpers::validateHardwareLicenseKey(licenseKey2, result.data, "salt123");
        bool valid3 = SystemInfoHelpers::validateHardwareLicenseKey(licenseKey2, result.data, "wrong_salt");
        bool valid4 = SystemInfoHelpers::validateHardwareLicenseKey("invalid_key", result.data);

        std::cout << "  Key 1 (no salt): " << (valid1 ? "Valid" : "Invalid") << "\n";
        std::cout << "  Key 2 (correct salt): " << (valid2 ? "Valid" : "Invalid") << "\n";
        std::cout << "  Key 2 (wrong salt): " << (valid3 ? "Valid" : "Invalid") << "\n";
        std::cout << "  Invalid key: " << (valid4 ? "Valid" : "Invalid") << "\n";

        std::cout << "\nLicense keys are tied to specific hardware configurations.\n";
        std::cout << "They can be used for software licensing and activation.\n";
    } else {
        std::cout << "Failed to get system information for license key demo.\n";
    }
}

void demonstrateSystemChangeDetection() {
    printSeparator("System Change Detection");

    auto sysInfo = createSystemInfo();
    auto result = sysInfo->getComprehensiveInfo();

    if (result.success) {
        // Get initial system hash
        std::string initialHash = SystemInfoHelpers::getSystemChangeHash(result.data);
        std::cout << "Initial system hash: " << initialHash << "\n";

        // Simulate system changes by modifying the data
        auto modifiedData = result.data;

        // Simulate adding a memory module
        MemoryModuleInfo newModule;
        newModule.serialNumber = "NEW_MODULE_123";
        newModule.sizeBytes = 8ULL * 1024 * 1024 * 1024; // 8 GB
        modifiedData.memoryModules.push_back(newModule);

        std::string modifiedHash = SystemInfoHelpers::getSystemChangeHash(modifiedData);
        std::cout << "Modified system hash: " << modifiedHash << "\n";

        // Detect changes
        bool changesDetected = SystemInfoHelpers::detectSystemChanges(initialHash, modifiedHash);
        std::cout << "Changes detected: " << (changesDetected ? "Yes" : "No") << "\n";

        // Simulate no changes
        std::string unchangedHash = SystemInfoHelpers::getSystemChangeHash(result.data);
        bool noChanges = SystemInfoHelpers::detectSystemChanges(initialHash, unchangedHash);
        std::cout << "No changes detected: " << (noChanges ? "No" : "Yes") << "\n";

        std::cout << "\nSystem change detection can be used to:\n";
        std::cout << "  - Monitor hardware modifications\n";
        std::cout << "  - Detect system tampering\n";
        std::cout << "  - Trigger license revalidation\n";
        std::cout << "  - Update system inventories\n";
    } else {
        std::cout << "Failed to get system information for change detection demo.\n";
    }
}

void demonstrateConfigurationManagement() {
    printSeparator("Configuration Management");

    // Create system info with custom configuration
    SystemInfoConfig config;
    config.includeMemoryModules = false;
    config.includeNetworkInterfaces = true;
    config.includeDiskSerials = false;
    config.cacheResults = false;
    config.enableLogging = false;

    auto sysInfo = createSystemInfo(config);

    std::cout << "Initial configuration:\n";
    const auto& currentConfig = sysInfo->getConfig();
    std::cout << "  Memory modules: " << (currentConfig.includeMemoryModules ? "Included" : "Excluded") << "\n";
    std::cout << "  Network interfaces: " << (currentConfig.includeNetworkInterfaces ? "Included" : "Excluded") << "\n";
    std::cout << "  Disk serials: " << (currentConfig.includeDiskSerials ? "Included" : "Excluded") << "\n";
    std::cout << "  Caching: " << (currentConfig.cacheResults ? "Enabled" : "Disabled") << "\n";
    std::cout << "  Logging: " << (currentConfig.enableLogging ? "Enabled" : "Disabled") << "\n";

    // Update configuration
    SystemInfoConfig newConfig;
    newConfig.includeMemoryModules = true;
    newConfig.includeNetworkInterfaces = false;
    newConfig.includeDiskSerials = true;
    newConfig.cacheResults = true;
    newConfig.cacheTimeout = std::chrono::minutes(10);
    newConfig.enableLogging = true;

    sysInfo->updateConfig(newConfig);

    std::cout << "\nUpdated configuration:\n";
    const auto& updatedConfig = sysInfo->getConfig();
    std::cout << "  Memory modules: " << (updatedConfig.includeMemoryModules ? "Included" : "Excluded") << "\n";
    std::cout << "  Network interfaces: " << (updatedConfig.includeNetworkInterfaces ? "Included" : "Excluded") << "\n";
    std::cout << "  Disk serials: " << (updatedConfig.includeDiskSerials ? "Included" : "Excluded") << "\n";
    std::cout << "  Caching: " << (updatedConfig.cacheResults ? "Enabled" : "Disabled") << "\n";
    std::cout << "  Cache timeout: " << updatedConfig.cacheTimeout.count() << " seconds\n";
    std::cout << "  Logging: " << (updatedConfig.enableLogging ? "Enabled" : "Disabled") << "\n";

    // Get collection statistics
    auto stats = sysInfo->getCollectionStats();
    std::cout << "\nCollection statistics:\n";
    for (const auto& [key, value] : stats) {
        std::cout << "  " << std::setw(20) << std::left << key << ": " << value << "\n";
    }
}

int main() {
    std::cout << "Atom System Information - Advanced Features Example\n";
    std::cout << "===================================================\n";

    try {
        // Demonstrate caching functionality
        demonstrateCaching();

        // Demonstrate data anonymization
        demonstrateAnonymization();

        // Demonstrate license key generation
        demonstrateLicenseKeyGeneration();

        // Demonstrate system change detection
        demonstrateSystemChangeDetection();

        // Demonstrate configuration management
        demonstrateConfigurationManagement();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "Advanced features example completed successfully!\n";
        std::cout << "These features enable sophisticated system monitoring,\n";
        std::cout << "security, and licensing applications.\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred\n";
        return 1;
    }

    return 0;
}
