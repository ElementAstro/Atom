#include <gtest/gtest.h>
#include "sn.hpp"
#include "common.hpp"
#include <chrono>
#include <thread>

using namespace atom::system;

class SystemInfoHelpersTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test data
        testInfo1.hardwareSerials.biosSerial = "BIOS123456";
        testInfo1.hardwareSerials.motherboardSerial = "MB123456";
        testInfo1.hardwareSerials.cpuSerial = "CPU123456";
        testInfo1.systemId.systemUuid = "550e8400-e29b-41d4-a716-446655440000";
        testInfo1.systemId.machineId = "machine123";
        testInfo1.systemId.macAddresses = {"00:11:22:33:44:55"};
        testInfo1.hardwareSerials.diskSerials = {"DISK123456"};
        
        // Create similar but slightly different test data
        testInfo2.hardwareSerials.biosSerial = "BIOS123456";
        testInfo2.hardwareSerials.motherboardSerial = "MB123456";
        testInfo2.hardwareSerials.cpuSerial = "CPU654321"; // Different
        testInfo2.systemId.systemUuid = "550e8400-e29b-41d4-a716-446655440000";
        testInfo2.systemId.machineId = "machine123";
        testInfo2.systemId.macAddresses = {"00:11:22:33:44:55"};
        testInfo2.hardwareSerials.diskSerials = {"DISK654321"}; // Different
    }

    ComprehensiveSystemInfo testInfo1;
    ComprehensiveSystemInfo testInfo2;
};

TEST_F(SystemInfoHelpersTest, CompareFingerprints) {
    // Generate fingerprints
    std::string fp1 = testInfo1.getSystemFingerprint();
    std::string fp2 = testInfo2.getSystemFingerprint();
    
    // Should be different due to different CPU and disk serials
    EXPECT_NE(fp1, fp2);
    
    // Compare fingerprints
    double similarity = SystemInfoHelpers::compareFingerprints(fp1, fp2);
    
    // Should be somewhat similar but not identical
    EXPECT_GT(similarity, 0.0);
    EXPECT_LT(similarity, 1.0);
    
    // Same fingerprint should have perfect similarity
    EXPECT_EQ(SystemInfoHelpers::compareFingerprints(fp1, fp1), 1.0);
    
    // Empty fingerprints should have zero similarity
    EXPECT_EQ(SystemInfoHelpers::compareFingerprints("", ""), 0.0);
    EXPECT_EQ(SystemInfoHelpers::compareFingerprints(fp1, ""), 0.0);
    EXPECT_EQ(SystemInfoHelpers::compareFingerprints("", fp2), 0.0);
}

TEST_F(SystemInfoHelpersTest, AnonymizeSystemInfo) {
    // Anonymize system info
    auto anonymized = SystemInfoHelpers::anonymizeSystemInfo(testInfo1);
    
    // Should not be empty
    EXPECT_TRUE(anonymized.isValid());
    
    // Should be different from original
    EXPECT_NE(anonymized.hardwareSerials.biosSerial, testInfo1.hardwareSerials.biosSerial);
    EXPECT_NE(anonymized.hardwareSerials.motherboardSerial, testInfo1.hardwareSerials.motherboardSerial);
    EXPECT_NE(anonymized.hardwareSerials.cpuSerial, testInfo1.hardwareSerials.cpuSerial);
    
    // Should contain anonymized prefixes
    EXPECT_NE(anonymized.hardwareSerials.biosSerial.find("BIOS_"), std::string::npos);
    EXPECT_NE(anonymized.hardwareSerials.motherboardSerial.find("MB_"), std::string::npos);
    EXPECT_NE(anonymized.hardwareSerials.cpuSerial.find("CPU_"), std::string::npos);
    
    // MAC addresses should be anonymized
    if (!anonymized.systemId.macAddresses.empty()) {
        EXPECT_NE(anonymized.systemId.macAddresses[0], testInfo1.systemId.macAddresses[0]);
        EXPECT_NE(anonymized.systemId.macAddresses[0].find("MAC_"), std::string::npos);
    }
    
    // Hostname should be anonymized
    if (!anonymized.systemId.hostname.empty()) {
        EXPECT_NE(anonymized.systemId.hostname, testInfo1.systemId.hostname);
        EXPECT_NE(anonymized.systemId.hostname.find("HOST_"), std::string::npos);
    }
}

TEST_F(SystemInfoHelpersTest, GenerateHardwareLicenseKey) {
    // Generate license key
    std::string licenseKey = SystemInfoHelpers::generateHardwareLicenseKey(testInfo1);
    EXPECT_FALSE(licenseKey.empty());
    
    // Generate with salt
    std::string licenseKeyWithSalt = SystemInfoHelpers::generateHardwareLicenseKey(testInfo1, "salt123");
    EXPECT_FALSE(licenseKeyWithSalt.empty());
    
    // Should be different with different salt
    EXPECT_NE(licenseKey, licenseKeyWithSalt);
    
    // Different system info should generate different key
    std::string licenseKey2 = SystemInfoHelpers::generateHardwareLicenseKey(testInfo2);
    EXPECT_NE(licenseKey, licenseKey2);
}

TEST_F(SystemInfoHelpersTest, ValidateHardwareLicenseKey) {
    // Generate license key
    std::string licenseKey = SystemInfoHelpers::generateHardwareLicenseKey(testInfo1, "salt123");
    
    // Should validate with same system info and salt
    EXPECT_TRUE(SystemInfoHelpers::validateHardwareLicenseKey(licenseKey, testInfo1, "salt123"));
    
    // Should fail with different system info
    EXPECT_FALSE(SystemInfoHelpers::validateHardwareLicenseKey(licenseKey, testInfo2, "salt123"));
    
    // Should fail with different salt
    EXPECT_FALSE(SystemInfoHelpers::validateHardwareLicenseKey(licenseKey, testInfo1, "different_salt"));
    
    // Should fail with invalid license key
    EXPECT_FALSE(SystemInfoHelpers::validateHardwareLicenseKey("invalid_key", testInfo1, "salt123"));
}

TEST_F(SystemInfoHelpersTest, SystemChangeDetection) {
    // Get system change hash
    std::string hash1 = SystemInfoHelpers::getSystemChangeHash(testInfo1);
    std::string hash2 = SystemInfoHelpers::getSystemChangeHash(testInfo2);
    
    // Should be different due to different CPU and disk serials
    EXPECT_NE(hash1, hash2);
    
    // Detect changes
    EXPECT_TRUE(SystemInfoHelpers::detectSystemChanges(hash1, hash2));
    EXPECT_FALSE(SystemInfoHelpers::detectSystemChanges(hash1, hash1));
    
    // Create a copy with minimal changes
    ComprehensiveSystemInfo testInfo3 = testInfo1;
    testInfo3.systemId.hostname = "different_hostname";
    
    // Get hash for minimal changes
    std::string hash3 = SystemInfoHelpers::getSystemChangeHash(testInfo3);
    
    // Should be same as original (hostname not included in change detection)
    EXPECT_EQ(hash1, hash3);
    EXPECT_FALSE(SystemInfoHelpers::detectSystemChanges(hash1, hash3));
}

// Test data structure implementations
class DataStructuresTest : public ::testing::Test {};

TEST_F(DataStructuresTest, HardwareSerialData) {
    HardwareSerialData data;
    
    // Empty data should not be valid
    EXPECT_FALSE(data.isValid());
    
    // Add some data
    data.biosSerial = "BIOS123456";
    EXPECT_TRUE(data.isValid());
    
    // Test toString
    std::string str = data.toString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("Hardware Serial Information"), std::string::npos);
    EXPECT_NE(str.find("BIOS Serial: BIOS123456"), std::string::npos);
    
    // Reset and test with different data
    data = HardwareSerialData();
    data.diskSerials = {"DISK1", "DISK2"};
    EXPECT_TRUE(data.isValid());
    
    str = data.toString();
    EXPECT_NE(str.find("Disk Serials: DISK1, DISK2"), std::string::npos);
}

TEST_F(DataStructuresTest, SystemIdentificationData) {
    SystemIdentificationData data;
    
    // Empty data should not be valid
    EXPECT_FALSE(data.isValid());
    
    // Add some data
    data.systemUuid = "550e8400-e29b-41d4-a716-446655440000";
    EXPECT_TRUE(data.isValid());
    
    // Test toString
    std::string str = data.toString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("System Identification Information"), std::string::npos);
    EXPECT_NE(str.find("System UUID: 550e8400-e29b-41d4-a716-446655440000"), std::string::npos);
    
    // Reset and test with different data
    data = SystemIdentificationData();
    data.macAddresses = {"00:11:22:33:44:55", "AA:BB:CC:DD:EE:FF"};
    EXPECT_TRUE(data.isValid());
    
    str = data.toString();
    EXPECT_NE(str.find("MAC Addresses: 00:11:22:33:44:55, AA:BB:CC:DD:EE:FF"), std::string::npos);
}

TEST_F(DataStructuresTest, MemoryModuleInfo) {
    MemoryModuleInfo data;
    
    // Empty data should not be valid
    EXPECT_FALSE(data.isValid());
    
    // Add some data
    data.serialNumber = "MEM123456";
    data.sizeBytes = 8ULL * 1024 * 1024 * 1024; // 8 GB
    EXPECT_TRUE(data.isValid());
    
    // Test toString
    std::string str = data.toString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("Memory Module"), std::string::npos);
    EXPECT_NE(str.find("Serial: MEM123456"), std::string::npos);
    EXPECT_NE(str.find("Size: 8.0 GB"), std::string::npos);
}

TEST_F(DataStructuresTest, NetworkInterfaceInfo) {
    NetworkInterfaceInfo data;
    
    // Empty data should not be valid
    EXPECT_FALSE(data.isValid());
    
    // Add some data
    data.name = "eth0";
    data.macAddress = "00:11:22:33:44:55";
    EXPECT_TRUE(data.isValid());
    
    // Test toString
    std::string str = data.toString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("Network Interface (eth0)"), std::string::npos);
    EXPECT_NE(str.find("MAC Address: 00:11:22:33:44:55"), std::string::npos);
}

TEST_F(DataStructuresTest, ComprehensiveSystemInfo) {
    ComprehensiveSystemInfo data;
    
    // Empty data should not be valid
    EXPECT_FALSE(data.isValid());
    
    // Add some data
    data.hardwareSerials.biosSerial = "BIOS123456";
    EXPECT_TRUE(data.isValid());
    
    // Test toString
    std::string str = data.toString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("Comprehensive System Information"), std::string::npos);
    
    // Add more data
    data.systemId.systemUuid = "550e8400-e29b-41d4-a716-446655440000";
    data.memoryModules.push_back(MemoryModuleInfo{
        .serialNumber = "MEM123456",
        .manufacturer = "Crucial",
        .sizeBytes = 8ULL * 1024 * 1024 * 1024
    });
    
    // Test toString with more data
    str = data.toString();
    EXPECT_NE(str.find("Memory Modules"), std::string::npos);
    
    // Test fingerprint
    std::string fingerprint = data.getSystemFingerprint();
    EXPECT_FALSE(fingerprint.empty());
}

TEST_F(DataStructuresTest, SystemInfoResult) {
    // Test with HardwareSerialData
    SystemInfoResult<HardwareSerialData> result;
    
    // Default should be success
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.error, SystemInfoError::SUCCESS);
    EXPECT_TRUE(result.errorMessage.empty());
    
    // Test error case
    result.success = false;
    result.error = SystemInfoError::ACCESS_DENIED;
    result.errorMessage = "Access denied";
    
    EXPECT_FALSE(result.isSuccess());
    EXPECT_FALSE(result.getErrorDescription().empty());
    EXPECT_NE(result.getErrorDescription().find("Access denied"), std::string::npos);
}

TEST_F(DataStructuresTest, SystemInfoConfig) {
    SystemInfoConfig config;
    
    // Check defaults
    EXPECT_TRUE(config.includeMemoryModules);
    EXPECT_TRUE(config.includeNetworkInterfaces);
    EXPECT_TRUE(config.includeDiskSerials);
    EXPECT_TRUE(config.includeSystemUuid);
    EXPECT_TRUE(config.includeMacAddresses);
    EXPECT_TRUE(config.cacheResults);
    EXPECT_EQ(config.cacheTimeout, std::chrono::seconds(300));
    EXPECT_TRUE(config.enableLogging);
    
    // Modify and check
    config.includeMemoryModules = false;
    config.cacheTimeout = std::chrono::seconds(60);
    
    EXPECT_FALSE(config.includeMemoryModules);
    EXPECT_EQ(config.cacheTimeout, std::chrono::seconds(60));
}
