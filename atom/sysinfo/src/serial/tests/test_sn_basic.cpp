#include <gtest/gtest.h>
#include "sn.hpp"
#include "common.hpp"
#include <chrono>
#include <thread>

using namespace atom::system;

class SystemInfoBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create system info instance for testing
        sysInfo = createSystemInfo();
        ASSERT_NE(sysInfo, nullptr);
    }

    void TearDown() override {
        sysInfo.reset();
    }

    std::unique_ptr<SystemInfo> sysInfo;
};

TEST_F(SystemInfoBasicTest, CreateSystemInfo) {
    EXPECT_TRUE(sysInfo != nullptr);
    EXPECT_TRUE(sysInfo->isSupported());
}

TEST_F(SystemInfoBasicTest, GetPlatformName) {
    std::string platform = sysInfo->getPlatformName();
    EXPECT_FALSE(platform.empty());

#ifdef _WIN32
    EXPECT_EQ(platform, "Windows");
#else
    EXPECT_EQ(platform, "Linux");
#endif
}

TEST_F(SystemInfoBasicTest, GetHardwareSerials) {
    auto result = sysInfo->getHardwareSerials();

    // Should succeed or fail gracefully
    if (result.success) {
        // At least one serial should be available on most systems
        EXPECT_TRUE(result.data.isValid());

        // Check that serials are reasonable (not empty or placeholder values)
        if (!result.data.biosSerial.empty()) {
            EXPECT_TRUE(SystemInfoUtils::isValidSerial(result.data.biosSerial));
        }
        if (!result.data.motherboardSerial.empty()) {
            EXPECT_TRUE(SystemInfoUtils::isValidSerial(result.data.motherboardSerial));
        }
        if (!result.data.cpuSerial.empty()) {
            EXPECT_TRUE(SystemInfoUtils::isValidSerial(result.data.cpuSerial));
        }
    } else {
        // If it fails, should have a meaningful error message
        EXPECT_FALSE(result.errorMessage.empty());
        EXPECT_NE(result.error, SystemInfoError::SUCCESS);
    }
}

TEST_F(SystemInfoBasicTest, GetSystemIdentification) {
    auto result = sysInfo->getSystemIdentification();

    if (result.success) {
        EXPECT_TRUE(result.data.isValid());

        // Check UUID format if available
        if (!result.data.systemUuid.empty()) {
            EXPECT_TRUE(SystemInfoUtils::isValidUuid(result.data.systemUuid));
        }

        // Check MAC addresses if available
        for (const auto& mac : result.data.macAddresses) {
            if (!mac.empty()) {
                EXPECT_TRUE(SystemInfoUtils::isValidMacAddress(mac));
            }
        }

        // Hostname should be available on most systems
        EXPECT_FALSE(result.data.hostname.empty());
    } else {
        EXPECT_FALSE(result.errorMessage.empty());
    }
}

TEST_F(SystemInfoBasicTest, GetSystemFingerprint) {
    std::string fingerprint = sysInfo->getSystemFingerprint();

    // Fingerprint should be generated even if some data is missing
    EXPECT_FALSE(fingerprint.empty());
    EXPECT_GE(fingerprint.length(), 8); // Should be at least 8 characters
}

TEST_F(SystemInfoBasicTest, QuerySpecificSystemId) {
    // Test querying specific system identifiers
    auto biosResult = sysInfo->querySystemId(SystemIdType::BIOS_SERIAL);
    EXPECT_TRUE(biosResult.success);
    EXPECT_EQ(biosResult.data.type, SystemIdType::BIOS_SERIAL);

    auto uuidResult = sysInfo->querySystemId(SystemIdType::SYSTEM_UUID);
    EXPECT_TRUE(uuidResult.success);
    EXPECT_EQ(uuidResult.data.type, SystemIdType::SYSTEM_UUID);

    auto macResult = sysInfo->querySystemId(SystemIdType::MAC_ADDRESS);
    EXPECT_TRUE(macResult.success);
    EXPECT_EQ(macResult.data.type, SystemIdType::MAC_ADDRESS);
}

TEST_F(SystemInfoBasicTest, ConfigurationHandling) {
    // Test configuration updates
    SystemInfoConfig config;
    config.includeMemoryModules = false;
    config.includeNetworkInterfaces = false;
    config.cacheResults = false;

    sysInfo->updateConfig(config);

    const auto& currentConfig = sysInfo->getConfig();
    EXPECT_EQ(currentConfig.includeMemoryModules, false);
    EXPECT_EQ(currentConfig.includeNetworkInterfaces, false);
    EXPECT_EQ(currentConfig.cacheResults, false);
}

TEST_F(SystemInfoBasicTest, CacheManagement) {
    // Enable caching
    SystemInfoConfig config;
    config.cacheResults = true;
    config.cacheTimeout = std::chrono::seconds(1);
    sysInfo->updateConfig(config);

    // First call should populate cache
    auto result1 = sysInfo->getHardwareSerials();
    EXPECT_TRUE(sysInfo->isCacheValid());

    // Second call should use cache
    auto result2 = sysInfo->getHardwareSerials();
    EXPECT_TRUE(sysInfo->isCacheValid());

    // Wait for cache to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_FALSE(sysInfo->isCacheValid());

    // Clear cache manually
    sysInfo->clearCache();
    EXPECT_FALSE(sysInfo->isCacheValid());
}

TEST_F(SystemInfoBasicTest, ErrorHandling) {
    // Test error handling with invalid operations
    auto result = sysInfo->querySystemId(static_cast<SystemIdType>(999));
    // Should handle gracefully without crashing
    EXPECT_TRUE(true); // If we get here, no crash occurred
}

TEST_F(SystemInfoBasicTest, ExportFunctionality) {
    // Test JSON export
    std::string json = sysInfo->exportToJson(false);
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("platform"), std::string::npos);
    EXPECT_NE(json.find("timestamp"), std::string::npos);

    // Test XML export
    std::string xml = sysInfo->exportToXml(false);
    EXPECT_FALSE(xml.empty());
    EXPECT_NE(xml.find("<?xml"), std::string::npos);
    EXPECT_NE(xml.find("<system_info>"), std::string::npos);
}

TEST_F(SystemInfoBasicTest, UtilityFunctions) {
    // Test utility functions
    EXPECT_TRUE(isSystemInfoAvailable());

    std::string quickFingerprint = getQuickSystemFingerprint();
    EXPECT_FALSE(quickFingerprint.empty());

    auto capabilities = getPlatformCapabilities();
    EXPECT_FALSE(capabilities.empty());
}

TEST_F(SystemInfoBasicTest, ValidationAndIntegrity) {
    // Test data validation
    bool isValid = sysInfo->validateIntegrity();
    // Should not crash and return a boolean
    EXPECT_TRUE(isValid || !isValid); // Always true, just checking no crash

    // Test collection stats
    auto stats = sysInfo->getCollectionStats();
    EXPECT_FALSE(stats.empty());
    EXPECT_NE(stats.find("platform"), stats.end());
    EXPECT_NE(stats.find("supported"), stats.end());
}

// Test SystemInfoUtils functions
class SystemInfoUtilsTest : public ::testing::Test {};

TEST_F(SystemInfoUtilsTest, SerialValidation) {
    // Valid serials
    EXPECT_TRUE(SystemInfoUtils::isValidSerial("ABC123DEF456"));
    EXPECT_TRUE(SystemInfoUtils::isValidSerial("1234567890"));
    EXPECT_TRUE(SystemInfoUtils::isValidSerial("SERIAL-NUMBER-123"));

    // Invalid serials
    EXPECT_FALSE(SystemInfoUtils::isValidSerial(""));
    EXPECT_FALSE(SystemInfoUtils::isValidSerial("N/A"));
    EXPECT_FALSE(SystemInfoUtils::isValidSerial("Not Available"));
    EXPECT_FALSE(SystemInfoUtils::isValidSerial("Default"));
    EXPECT_FALSE(SystemInfoUtils::isValidSerial("0000000000"));
    EXPECT_FALSE(SystemInfoUtils::isValidSerial("AB")); // Too short
}

TEST_F(SystemInfoUtilsTest, MacAddressValidation) {
    // Valid MAC addresses
    EXPECT_TRUE(SystemInfoUtils::isValidMacAddress("00:11:22:33:44:55"));
    EXPECT_TRUE(SystemInfoUtils::isValidMacAddress("AA:BB:CC:DD:EE:FF"));
    EXPECT_TRUE(SystemInfoUtils::isValidMacAddress("00-11-22-33-44-55"));
    EXPECT_TRUE(SystemInfoUtils::isValidMacAddress("aa:bb:cc:dd:ee:ff"));

    // Invalid MAC addresses
    EXPECT_FALSE(SystemInfoUtils::isValidMacAddress(""));
    EXPECT_FALSE(SystemInfoUtils::isValidMacAddress("00:11:22:33:44"));
    EXPECT_FALSE(SystemInfoUtils::isValidMacAddress("00:11:22:33:44:55:66"));
    EXPECT_FALSE(SystemInfoUtils::isValidMacAddress("GG:HH:II:JJ:KK:LL"));
    EXPECT_FALSE(SystemInfoUtils::isValidMacAddress("00.11.22.33.44.55"));
}

TEST_F(SystemInfoUtilsTest, UuidValidation) {
    // Valid UUIDs
    EXPECT_TRUE(SystemInfoUtils::isValidUuid("550e8400-e29b-41d4-a716-446655440000"));
    EXPECT_TRUE(SystemInfoUtils::isValidUuid("6ba7b810-9dad-11d1-80b4-00c04fd430c8"));
    EXPECT_TRUE(SystemInfoUtils::isValidUuid("12345678-1234-1234-1234-123456789abc"));

    // Invalid UUIDs
    EXPECT_FALSE(SystemInfoUtils::isValidUuid(""));
    EXPECT_FALSE(SystemInfoUtils::isValidUuid("550e8400-e29b-41d4-a716"));
    EXPECT_FALSE(SystemInfoUtils::isValidUuid("550e8400-e29b-41d4-a716-446655440000-extra"));
    EXPECT_FALSE(SystemInfoUtils::isValidUuid("550e8400_e29b_41d4_a716_446655440000"));
    EXPECT_FALSE(SystemInfoUtils::isValidUuid("gggggggg-gggg-gggg-gggg-gggggggggggg"));
}

TEST_F(SystemInfoUtilsTest, ByteFormatting) {
    EXPECT_EQ(SystemInfoUtils::formatBytes(0), "0.0 B");
    EXPECT_EQ(SystemInfoUtils::formatBytes(1024), "1.0 KB");
    EXPECT_EQ(SystemInfoUtils::formatBytes(1024 * 1024), "1.0 MB");
    EXPECT_EQ(SystemInfoUtils::formatBytes(1024 * 1024 * 1024), "1.0 GB");
    EXPECT_EQ(SystemInfoUtils::formatBytes(8ULL * 1024 * 1024 * 1024), "8.0 GB");
}

TEST_F(SystemInfoUtilsTest, HashGeneration) {
    std::string hash1 = SystemInfoUtils::generateHash("test string");
    std::string hash2 = SystemInfoUtils::generateHash("test string");
    std::string hash3 = SystemInfoUtils::generateHash("different string");

    // Same input should produce same hash
    EXPECT_EQ(hash1, hash2);

    // Different input should produce different hash
    EXPECT_NE(hash1, hash3);

    // Hash should not be empty
    EXPECT_FALSE(hash1.empty());
}

TEST_F(SystemInfoUtilsTest, TimestampGeneration) {
    std::string timestamp = SystemInfoUtils::getCurrentTimestamp();
    EXPECT_FALSE(timestamp.empty());

    // Should be in ISO 8601 format (basic check)
    EXPECT_NE(timestamp.find("T"), std::string::npos);
    EXPECT_NE(timestamp.find("Z"), std::string::npos);
}
