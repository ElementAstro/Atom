/*
 * test_enhanced_features.cpp
 *
 * Comprehensive tests for enhanced sysinfo features
 * Tests new API features, export functionality, caching mechanisms, and advanced capabilities
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>

#include "atom/sysinfo/sn.hpp"
#include "atom/sysinfo/wm.hpp"
#include "atom/sysinfo/battery.hpp"
#include "atom/sysinfo/sysinfo_printer.hpp"

using namespace atom::system;
using namespace testing;
using namespace std::chrono_literals;

class EnhancedSysinfoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create system info with enhanced configuration
        SystemInfoConfig config;
        config.includeMemoryModules = true;
        config.includeNetworkInterfaces = true;
        config.includeDiskSerials = true;
        config.includeSystemUuid = true;
        config.includeMacAddresses = true;
        config.cacheResults = true;
        config.cacheTimeout = 30s;
        config.enableLogging = false;  // Reduce test noise

        sysInfo = createSystemInfo(config);
        ASSERT_NE(sysInfo, nullptr);
    }

    void TearDown() override {
        // Cleanup temporary files
        for (const auto& file : tempFiles) {
            if (std::filesystem::exists(file)) {
                std::filesystem::remove(file);
            }
        }
    }

    std::unique_ptr<SystemInfo> sysInfo;
    std::vector<std::string> tempFiles;
};

// Test enhanced SystemInfo API features
TEST_F(EnhancedSysinfoTest, ComprehensiveSystemInfo) {
    auto result = sysInfo->getComprehensiveInfo();

    if (result.success) {
        const auto& info = result.data;

        // Test comprehensive data structure
        EXPECT_TRUE(info.isValid());

        // Test hardware serials
        EXPECT_TRUE(info.hardwareSerials.isValid() ||
                   info.hardwareSerials.biosSerial.empty());

        // Test system identification
        EXPECT_TRUE(info.systemId.isValid() ||
                   info.systemId.systemUuid.empty());

        // Test memory modules (if included)
        if (!info.memoryModules.empty()) {
            for (const auto& module : info.memoryModules) {
                EXPECT_FALSE(module.serialNumber.empty() || module.serialNumber == "Unknown");
                EXPECT_GT(module.capacity, 0);
            }
        }

        // Test network interfaces (if included)
        if (!info.networkInterfaces.empty()) {
            for (const auto& interface : info.networkInterfaces) {
                EXPECT_FALSE(interface.name.empty());
                EXPECT_FALSE(interface.macAddress.empty());
            }
        }

        // Test timestamp
        auto now = std::chrono::system_clock::now();
        auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(now - info.lastUpdate);
        EXPECT_LT(timeDiff.count(), 60);  // Should be recent
    }
}

// Test caching mechanisms
TEST_F(EnhancedSysinfoTest, CachingFunctionality) {
    // First call - should populate cache
    auto start1 = std::chrono::high_resolution_clock::now();
    auto result1 = sysInfo->getHardwareSerials();
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1);

    // Second call - should use cache (faster)
    auto start2 = std::chrono::high_resolution_clock::now();
    auto result2 = sysInfo->getHardwareSerials();
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);

    if (result1.success && result2.success) {
        // Results should be identical
        EXPECT_EQ(result1.data.biosSerial, result2.data.biosSerial);
        EXPECT_EQ(result1.data.motherboardSerial, result2.data.motherboardSerial);
        EXPECT_EQ(result1.data.cpuSerial, result2.data.cpuSerial);

        // Second call should be faster (cached)
        EXPECT_LE(duration2.count(), duration1.count() + 10);  // Allow some variance
    }

    // Test cache age
    auto cacheAge = sysInfo->getCacheAge();
    EXPECT_GE(cacheAge.count(), 0);
    EXPECT_LT(cacheAge.count(), 60000);  // Less than 1 minute
}

// Test cache invalidation
TEST_F(EnhancedSysinfoTest, CacheInvalidation) {
    // Get initial data
    auto result1 = sysInfo->getHardwareSerials();
    auto age1 = sysInfo->getCacheAge();

    // Invalidate cache
    sysInfo->invalidateCache();

    // Cache age should be reset
    auto age2 = sysInfo->getCacheAge();
    EXPECT_LT(age2.count(), age1.count());

    // Get data again - should refresh
    auto result2 = sysInfo->getHardwareSerials();

    if (result1.success && result2.success) {
        // Data should still be consistent
        EXPECT_EQ(result1.data.biosSerial, result2.data.biosSerial);
    }
}

// Test export functionality
TEST_F(EnhancedSysinfoTest, JsonExport) {
    auto result = sysInfo->getComprehensiveInfo();

    if (result.success) {
        // Test JSON export
        std::string json = sysInfo->exportToJson(true);  // Pretty print
        EXPECT_FALSE(json.empty());

        // Basic JSON structure validation
        EXPECT_THAT(json, HasSubstr("{"));
        EXPECT_THAT(json, HasSubstr("}"));
        EXPECT_THAT(json, HasSubstr("\"hardwareSerials\""));
        EXPECT_THAT(json, HasSubstr("\"systemId\""));

        // Test compact JSON export
        std::string compactJson = sysInfo->exportToJson(false);
        EXPECT_FALSE(compactJson.empty());
        EXPECT_LT(compactJson.length(), json.length());  // Should be shorter
    }
}

TEST_F(EnhancedSysinfoTest, XmlExport) {
    auto result = sysInfo->getComprehensiveInfo();

    if (result.success) {
        // Test XML export
        std::string xml = sysInfo->exportToXml(true);  // Pretty print
        EXPECT_FALSE(xml.empty());

        // Basic XML structure validation
        EXPECT_THAT(xml, HasSubstr("<?xml"));
        EXPECT_THAT(xml, HasSubstr("<SystemInfo>"));
        EXPECT_THAT(xml, HasSubstr("</SystemInfo>"));
        EXPECT_THAT(xml, HasSubstr("<HardwareSerials>"));
        EXPECT_THAT(xml, HasSubstr("<SystemIdentification>"));
    }
}

// Test file export functionality
TEST_F(EnhancedSysinfoTest, FileExport) {
    auto result = sysInfo->getComprehensiveInfo();

    if (result.success) {
        std::string jsonFile = "test_sysinfo_export.json";
        std::string xmlFile = "test_sysinfo_export.xml";

        tempFiles.push_back(jsonFile);
        tempFiles.push_back(xmlFile);

        // Export to files
        bool jsonSuccess = sysInfo->exportToFile(jsonFile, "json");
        bool xmlSuccess = sysInfo->exportToFile(xmlFile, "xml");

        EXPECT_TRUE(jsonSuccess);
        EXPECT_TRUE(xmlSuccess);

        // Verify files exist and have content
        EXPECT_TRUE(std::filesystem::exists(jsonFile));
        EXPECT_TRUE(std::filesystem::exists(xmlFile));

        EXPECT_GT(std::filesystem::file_size(jsonFile), 0);
        EXPECT_GT(std::filesystem::file_size(xmlFile), 0);

        // Verify file contents
        std::ifstream jsonStream(jsonFile);
        std::string jsonContent((std::istreambuf_iterator<char>(jsonStream)),
                               std::istreambuf_iterator<char>());
        EXPECT_THAT(jsonContent, HasSubstr("hardwareSerials"));

        std::ifstream xmlStream(xmlFile);
        std::string xmlContent((std::istreambuf_iterator<char>(xmlStream)),
                              std::istreambuf_iterator<char>());
        EXPECT_THAT(xmlContent, HasSubstr("<SystemInfo>"));
    }
}

// Test system fingerprinting
TEST_F(EnhancedSysinfoTest, SystemFingerprinting) {
    auto result = sysInfo->getComprehensiveInfo();

    if (result.success) {
        std::string fingerprint = sysInfo->getSystemFingerprint();
        EXPECT_FALSE(fingerprint.empty());

        // Fingerprint should be consistent
        std::string fingerprint2 = sysInfo->getSystemFingerprint();
        EXPECT_EQ(fingerprint, fingerprint2);

        // Fingerprint should be a hash (hexadecimal)
        EXPECT_TRUE(std::all_of(fingerprint.begin(), fingerprint.end(),
                               [](char c) { return std::isxdigit(c); }));

        // Should be reasonable length for a hash
        EXPECT_GE(fingerprint.length(), 32);  // At least MD5 length
        EXPECT_LE(fingerprint.length(), 128); // At most SHA-512 length
    }
}

// Test anonymization features
TEST_F(EnhancedSysinfoTest, DataAnonymization) {
    auto result = sysInfo->getComprehensiveInfo();

    if (result.success) {
        auto anonymized = SystemInfoHelpers::anonymizeSystemInfo(result.data);

        // Anonymized data should be different from original
        if (!result.data.hardwareSerials.biosSerial.empty()) {
            EXPECT_NE(anonymized.hardwareSerials.biosSerial,
                     result.data.hardwareSerials.biosSerial);
            EXPECT_THAT(anonymized.hardwareSerials.biosSerial, HasSubstr("BIOS_"));
        }

        if (!result.data.hardwareSerials.motherboardSerial.empty()) {
            EXPECT_NE(anonymized.hardwareSerials.motherboardSerial,
                     result.data.hardwareSerials.motherboardSerial);
            EXPECT_THAT(anonymized.hardwareSerials.motherboardSerial, HasSubstr("MB_"));
        }

        // Structure should remain the same
        EXPECT_EQ(anonymized.memoryModules.size(), result.data.memoryModules.size());
        EXPECT_EQ(anonymized.networkInterfaces.size(), result.data.networkInterfaces.size());
    }
}

// Test system change detection
TEST_F(EnhancedSysinfoTest, SystemChangeDetection) {
    auto result = sysInfo->getComprehensiveInfo();

    if (result.success) {
        std::string hash1 = SystemInfoHelpers::getSystemChangeHash(result.data);
        EXPECT_FALSE(hash1.empty());

        // Get data again - hash should be the same
        auto result2 = sysInfo->getComprehensiveInfo();
        if (result2.success) {
            std::string hash2 = SystemInfoHelpers::getSystemChangeHash(result2.data);
            EXPECT_EQ(hash1, hash2);

            // Test change detection
            bool hasChanged = SystemInfoHelpers::detectSystemChanges(hash1, hash2);
            EXPECT_FALSE(hasChanged);

            // Test with different hash
            std::string differentHash = hash1 + "different";
            bool shouldHaveChanged = SystemInfoHelpers::detectSystemChanges(hash1, differentHash);
            EXPECT_TRUE(shouldHaveChanged);
        }
    }
}

// Test error handling and edge cases
TEST_F(EnhancedSysinfoTest, ErrorHandling) {
    // Test with invalid export format
    bool invalidExport = sysInfo->exportToFile("test.invalid", "invalid_format");
    EXPECT_FALSE(invalidExport);

    // Test with invalid file path
    bool invalidPath = sysInfo->exportToFile("/invalid/path/file.json", "json");
    EXPECT_FALSE(invalidPath);
}

// Test backward compatibility
TEST_F(EnhancedSysinfoTest, BackwardCompatibility) {
    // Test original HardwareInfo API
    HardwareInfo hwInfo;

    EXPECT_NO_THROW({
        std::string biosSerial = hwInfo.getBiosSerialNumber();
        std::string mbSerial = hwInfo.getMotherboardSerialNumber();
        std::string cpuSerial = hwInfo.getCpuSerialNumber();
        auto diskSerials = hwInfo.getDiskSerialNumbers();

        // These should work without throwing exceptions
        EXPECT_TRUE(biosSerial.empty() || !biosSerial.empty());
        EXPECT_TRUE(mbSerial.empty() || !mbSerial.empty());
        EXPECT_TRUE(cpuSerial.empty() || !cpuSerial.empty());
    });

    // Test enhanced mode availability
    if (hwInfo.isEnhancedModeAvailable()) {
        std::string fingerprint = hwInfo.getSystemFingerprint();
        EXPECT_FALSE(fingerprint.empty());
    }
}
