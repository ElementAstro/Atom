#include <gtest/gtest.h>
#include "sn.hpp"
#include "common.hpp"
#include <chrono>
#include <thread>

using namespace atom::system;

class SystemInfoComprehensiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create system info with full configuration
        SystemInfoConfig config;
        config.includeMemoryModules = true;
        config.includeNetworkInterfaces = true;
        config.includeDiskSerials = true;
        config.includeSystemUuid = true;
        config.includeMacAddresses = true;
        config.cacheResults = true;
        config.cacheTimeout = std::chrono::seconds(30);
        config.enableLogging = true;
        
        sysInfo = createSystemInfo(config);
        ASSERT_NE(sysInfo, nullptr);
    }

    void TearDown() override {
        sysInfo.reset();
    }

    std::unique_ptr<SystemInfo> sysInfo;
};

TEST_F(SystemInfoComprehensiveTest, GetComprehensiveInfo) {
    auto result = sysInfo->getComprehensiveInfo();
    
    if (result.success) {
        EXPECT_TRUE(result.data.isValid());
        
        // Check hardware serials
        const auto& hwSerials = result.data.hardwareSerials;
        EXPECT_TRUE(hwSerials.isValid() || hwSerials.biosSerial.empty());
        
        // Check system identification
        const auto& sysId = result.data.systemId;
        EXPECT_TRUE(sysId.isValid() || sysId.systemUuid.empty());
        
        // Check timestamp
        auto now = std::chrono::system_clock::now();
        auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(now - result.data.lastUpdate);
        EXPECT_LT(timeDiff.count(), 60); // Should be recent
        
        // Test string representation
        std::string infoStr = result.data.toString();
        EXPECT_FALSE(infoStr.empty());
        EXPECT_NE(infoStr.find("System Information"), std::string::npos);
        
        // Test fingerprint generation
        std::string fingerprint = result.data.getSystemFingerprint();
        EXPECT_FALSE(fingerprint.empty());
        EXPECT_GE(fingerprint.length(), 8);
    } else {
        // If comprehensive info fails, should have meaningful error
        EXPECT_FALSE(result.errorMessage.empty());
        EXPECT_NE(result.error, SystemInfoError::SUCCESS);
    }
}

TEST_F(SystemInfoComprehensiveTest, GetMemoryModules) {
    auto result = sysInfo->getMemoryModules();
    
    if (result.success) {
        EXPECT_FALSE(result.data.empty());
        
        for (const auto& module : result.data) {
            EXPECT_TRUE(module.isValid());
            
            // Check size is reasonable
            if (module.sizeBytes > 0) {
                EXPECT_GE(module.sizeBytes, 1024 * 1024); // At least 1MB
                EXPECT_LE(module.sizeBytes, 1024ULL * 1024 * 1024 * 1024); // At most 1TB
            }
            
            // Check speed is reasonable
            if (module.speedMHz > 0) {
                EXPECT_GE(module.speedMHz, 100); // At least 100MHz
                EXPECT_LE(module.speedMHz, 10000); // At most 10GHz
            }
            
            // Test string representation
            std::string moduleStr = module.toString();
            EXPECT_FALSE(moduleStr.empty());
            EXPECT_NE(moduleStr.find("Memory Module"), std::string::npos);
        }
    } else {
        // Memory module info might not be available on all systems
        EXPECT_TRUE(result.error == SystemInfoError::HARDWARE_NOT_FOUND ||
                   result.error == SystemInfoError::ACCESS_DENIED ||
                   result.error == SystemInfoError::NOT_SUPPORTED);
    }
}

TEST_F(SystemInfoComprehensiveTest, GetNetworkInterfaces) {
    auto result = sysInfo->getNetworkInterfaces();
    
    if (result.success) {
        EXPECT_FALSE(result.data.empty());
        
        for (const auto& interface : result.data) {
            EXPECT_TRUE(interface.isValid());
            EXPECT_FALSE(interface.name.empty());
            EXPECT_FALSE(interface.macAddress.empty());
            
            // Validate MAC address format
            EXPECT_TRUE(SystemInfoUtils::isValidMacAddress(interface.macAddress));
            
            // Test string representation
            std::string interfaceStr = interface.toString();
            EXPECT_FALSE(interfaceStr.empty());
            EXPECT_NE(interfaceStr.find("Network Interface"), std::string::npos);
        }
    } else {
        // Network interfaces should be available on most systems
        EXPECT_FALSE(result.errorMessage.empty());
    }
}

TEST_F(SystemInfoComprehensiveTest, SystemIdQueries) {
    // Test all system ID types
    std::vector<SystemIdType> idTypes = {
        SystemIdType::BIOS_SERIAL,
        SystemIdType::MOTHERBOARD_SERIAL,
        SystemIdType::CPU_SERIAL,
        SystemIdType::SYSTEM_UUID,
        SystemIdType::MACHINE_ID,
        SystemIdType::MAC_ADDRESS,
        SystemIdType::DISK_SERIAL,
        SystemIdType::MEMORY_SERIAL
    };
    
    for (auto idType : idTypes) {
        auto result = sysInfo->querySystemId(idType);
        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.data.type, idType);
        
        // Check timestamp is recent
        auto now = std::chrono::system_clock::now();
        auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(now - result.data.timestamp);
        EXPECT_LT(timeDiff.count(), 60);
        
        // If value is available, it should be valid
        if (result.data.isAvailable) {
            EXPECT_FALSE(result.data.value.empty());
            
            // Type-specific validation
            switch (idType) {
                case SystemIdType::BIOS_SERIAL:
                case SystemIdType::MOTHERBOARD_SERIAL:
                case SystemIdType::CPU_SERIAL:
                case SystemIdType::DISK_SERIAL:
                case SystemIdType::MEMORY_SERIAL:
                    EXPECT_TRUE(SystemInfoUtils::isValidSerial(result.data.value));
                    break;
                case SystemIdType::SYSTEM_UUID:
                    if (!result.data.value.empty()) {
                        EXPECT_TRUE(SystemInfoUtils::isValidUuid(result.data.value));
                    }
                    break;
                case SystemIdType::MAC_ADDRESS:
                    if (!result.data.value.empty()) {
                        EXPECT_TRUE(SystemInfoUtils::isValidMacAddress(result.data.value));
                    }
                    break;
                case SystemIdType::MACHINE_ID:
                    // Machine ID format varies by platform
                    EXPECT_FALSE(result.data.value.empty());
                    break;
            }
        }
    }
}

TEST_F(SystemInfoComprehensiveTest, CachingBehavior) {
    // Test caching with different data types
    
    // First calls should populate cache
    auto hw1 = sysInfo->getHardwareSerials();
    auto sys1 = sysInfo->getSystemIdentification();
    auto mem1 = sysInfo->getMemoryModules();
    auto net1 = sysInfo->getNetworkInterfaces();
    
    EXPECT_TRUE(sysInfo->isCacheValid());
    
    // Second calls should use cache (should be faster)
    auto start = std::chrono::high_resolution_clock::now();
    auto hw2 = sysInfo->getHardwareSerials();
    auto sys2 = sysInfo->getSystemIdentification();
    auto mem2 = sysInfo->getMemoryModules();
    auto net2 = sysInfo->getNetworkInterfaces();
    auto end = std::chrono::high_resolution_clock::now();
    
    // Cached calls should be very fast
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 100); // Should take less than 100ms
    
    // Results should be identical
    if (hw1.success && hw2.success) {
        EXPECT_EQ(hw1.data.biosSerial, hw2.data.biosSerial);
        EXPECT_EQ(hw1.data.motherboardSerial, hw2.data.motherboardSerial);
    }
    
    if (sys1.success && sys2.success) {
        EXPECT_EQ(sys1.data.systemUuid, sys2.data.systemUuid);
        EXPECT_EQ(sys1.data.machineId, sys2.data.machineId);
    }
}

TEST_F(SystemInfoComprehensiveTest, RefreshAllData) {
    // Get initial data
    auto initial = sysInfo->getComprehensiveInfo();
    
    // Refresh all data
    bool refreshSuccess = sysInfo->refreshAll();
    EXPECT_TRUE(refreshSuccess);
    
    // Get data after refresh
    auto refreshed = sysInfo->getComprehensiveInfo();
    
    // Both should be successful (or both fail)
    EXPECT_EQ(initial.success, refreshed.success);
    
    if (initial.success && refreshed.success) {
        // Data should be consistent
        EXPECT_EQ(initial.data.hardwareSerials.biosSerial, 
                 refreshed.data.hardwareSerials.biosSerial);
        EXPECT_EQ(initial.data.systemId.systemUuid, 
                 refreshed.data.systemId.systemUuid);
    }
}

TEST_F(SystemInfoComprehensiveTest, ExportFormats) {
    auto result = sysInfo->getComprehensiveInfo();
    
    if (result.success) {
        // Test JSON export with full data
        std::string jsonFull = sysInfo->exportToJson(true);
        EXPECT_FALSE(jsonFull.empty());
        EXPECT_NE(jsonFull.find("hardware_serials"), std::string::npos);
        EXPECT_NE(jsonFull.find("system_id"), std::string::npos);
        EXPECT_NE(jsonFull.find("platform"), std::string::npos);
        
        // Test JSON export with minimal data
        std::string jsonMinimal = sysInfo->exportToJson(false);
        EXPECT_FALSE(jsonMinimal.empty());
        EXPECT_NE(jsonMinimal.find("platform"), std::string::npos);
        EXPECT_NE(jsonMinimal.find("fingerprint"), std::string::npos);
        
        // Test XML export with full data
        std::string xmlFull = sysInfo->exportToXml(true);
        EXPECT_FALSE(xmlFull.empty());
        EXPECT_NE(xmlFull.find("<?xml"), std::string::npos);
        EXPECT_NE(xmlFull.find("<hardware_serials>"), std::string::npos);
        EXPECT_NE(xmlFull.find("<system_id>"), std::string::npos);
        
        // Test XML export with minimal data
        std::string xmlMinimal = sysInfo->exportToXml(false);
        EXPECT_FALSE(xmlMinimal.empty());
        EXPECT_NE(xmlMinimal.find("<?xml"), std::string::npos);
        EXPECT_NE(xmlMinimal.find("<platform>"), std::string::npos);
    }
}

TEST_F(SystemInfoComprehensiveTest, SystemSummary) {
    std::string summary = sysInfo->getSummary();
    EXPECT_FALSE(summary.empty());
    
    // Should contain key information
    EXPECT_NE(summary.find("System Information Summary"), std::string::npos);
    EXPECT_NE(summary.find("Platform:"), std::string::npos);
    EXPECT_NE(summary.find("Supported:"), std::string::npos);
    EXPECT_NE(summary.find("Cache"), std::string::npos);
}

TEST_F(SystemInfoComprehensiveTest, CollectionStatistics) {
    auto stats = sysInfo->getCollectionStats();
    EXPECT_FALSE(stats.empty());
    
    // Check required statistics
    EXPECT_NE(stats.find("platform"), stats.end());
    EXPECT_NE(stats.find("supported"), stats.end());
    EXPECT_NE(stats.find("cache_valid"), stats.end());
    EXPECT_NE(stats.find("cache_age_seconds"), stats.end());
    EXPECT_NE(stats.find("caching_enabled"), stats.end());
    EXPECT_NE(stats.find("logging_enabled"), stats.end());
    
    // Validate values
    EXPECT_TRUE(stats["supported"] == "true" || stats["supported"] == "false");
    EXPECT_TRUE(stats["cache_valid"] == "true" || stats["cache_valid"] == "false");
    EXPECT_TRUE(stats["caching_enabled"] == "true" || stats["caching_enabled"] == "false");
    EXPECT_TRUE(stats["logging_enabled"] == "true" || stats["logging_enabled"] == "false");
}

TEST_F(SystemInfoComprehensiveTest, ConfigurationPersistence) {
    // Test that configuration changes persist
    SystemInfoConfig newConfig;
    newConfig.includeMemoryModules = false;
    newConfig.includeNetworkInterfaces = false;
    newConfig.cacheResults = false;
    newConfig.enableLogging = false;
    
    sysInfo->updateConfig(newConfig);
    
    // Get comprehensive info with new config
    auto result = sysInfo->getComprehensiveInfo();
    
    // Should respect configuration
    if (result.success) {
        // Memory modules and network interfaces should be empty or minimal
        // (depending on implementation details)
        EXPECT_TRUE(result.data.memoryModules.empty() || 
                   result.data.memoryModules.size() <= 1);
        EXPECT_TRUE(result.data.networkInterfaces.empty() || 
                   result.data.networkInterfaces.size() <= 1);
    }
    
    // Verify configuration was applied
    const auto& currentConfig = sysInfo->getConfig();
    EXPECT_EQ(currentConfig.includeMemoryModules, false);
    EXPECT_EQ(currentConfig.includeNetworkInterfaces, false);
    EXPECT_EQ(currentConfig.cacheResults, false);
    EXPECT_EQ(currentConfig.enableLogging, false);
}

TEST_F(SystemInfoComprehensiveTest, ErrorRecovery) {
    // Test that the system can recover from errors
    
    // Force an error by clearing cache and trying invalid operations
    sysInfo->clearCache();
    
    // These should not crash the system
    auto result1 = sysInfo->querySystemId(static_cast<SystemIdType>(999));
    auto result2 = sysInfo->getHardwareSerials();
    auto result3 = sysInfo->getSystemIdentification();
    
    // System should still be functional
    EXPECT_TRUE(sysInfo->isSupported());
    std::string fingerprint = sysInfo->getSystemFingerprint();
    EXPECT_FALSE(fingerprint.empty());
}

TEST_F(SystemInfoComprehensiveTest, ThreadSafety) {
    // Basic thread safety test
    std::vector<std::thread> threads;
    std::vector<std::string> fingerprints(4);
    
    // Launch multiple threads to get fingerprints
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, &fingerprints, i]() {
            fingerprints[i] = sysInfo->getSystemFingerprint();
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All fingerprints should be identical
    for (int i = 1; i < 4; ++i) {
        EXPECT_EQ(fingerprints[0], fingerprints[i]);
    }
    
    // All should be non-empty
    for (const auto& fp : fingerprints) {
        EXPECT_FALSE(fp.empty());
    }
}
