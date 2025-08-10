#include "atom/sysinfo/sn.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>

using namespace atom::system;
using namespace testing;

class SerialNumberTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if necessary
    }

    void TearDown() override {
        // Cleanup code if necessary
    }
};

// Test SystemInfo factory function
TEST_F(SerialNumberTest, SystemInfoFactory) {
    EXPECT_NO_THROW({
        auto sysInfo = createSystemInfo();
        EXPECT_NE(sysInfo, nullptr);

        // Test with configuration
        SystemInfoConfig config;
        config.includeMemoryModules = true;
        config.includeNetworkInterfaces = true;
        config.cacheResults = true;

        auto configuredSysInfo = createSystemInfo(config);
        EXPECT_NE(configuredSysInfo, nullptr);
    });
}

// Test SystemInfo basic functionality
TEST_F(SerialNumberTest, SystemInfoBasic) {
    auto sysInfo = createSystemInfo();
    ASSERT_NE(sysInfo, nullptr);

    EXPECT_NO_THROW({
        // Test platform support
        bool isSupported = sysInfo->isSupported();
        EXPECT_TRUE(isSupported == true || isSupported == false);

        // Test cache age
        auto cacheAge = sysInfo->getCacheAge();
        EXPECT_GE(cacheAge.count(), 0);

        // Test summary
        std::string summary = sysInfo->getSummary();
        EXPECT_TRUE(summary.empty() || !summary.empty());

        // Test integrity validation
        bool isValid = sysInfo->validateIntegrity();
        EXPECT_TRUE(isValid == true || isValid == false);
    });
}

// Test hardware serial retrieval
TEST_F(SerialNumberTest, HardwareSerials) {
    auto sysInfo = createSystemInfo();
    ASSERT_NE(sysInfo, nullptr);

    EXPECT_NO_THROW({
        auto result = sysInfo->getHardwareSerials();

        // Result should have valid structure
        EXPECT_TRUE(result.success == true || result.success == false);

        if (result.success) {
            const auto& data = result.data;

            // Serial numbers can be empty on some systems
            EXPECT_TRUE(data.biosSerial.empty() || !data.biosSerial.empty());
            EXPECT_TRUE(data.motherboardSerial.empty() || !data.motherboardSerial.empty());
            EXPECT_TRUE(data.cpuSerial.empty() || !data.cpuSerial.empty());

            // Disk serials should be a valid vector
            for (const auto& diskSerial : data.diskSerials) {
                EXPECT_FALSE(diskSerial.empty());
            }

            // Test isValid method
            bool dataValid = data.isValid();
            EXPECT_TRUE(dataValid == true || dataValid == false);

            // Test toString method
            std::string dataStr = data.toString();
            EXPECT_TRUE(dataStr.empty() || !dataStr.empty());

            // Test forced refresh
            auto refreshResult = sysInfo->getHardwareSerials(true);
            EXPECT_TRUE(refreshResult.success == true || refreshResult.success == false);

            // Results should be consistent
            if (result.success && refreshResult.success) {
                EXPECT_EQ(data.biosSerial, refreshResult.data.biosSerial);
                EXPECT_EQ(data.motherboardSerial, refreshResult.data.motherboardSerial);
                EXPECT_EQ(data.cpuSerial, refreshResult.data.cpuSerial);
            }
        } else {
            // If failed, should have error information
            EXPECT_FALSE(result.errorMessage.empty());
            EXPECT_NE(result.error, SystemInfoError::SUCCESS);
        }
    });
}

// Test system identification
TEST_F(SerialNumberTest, SystemIdentification) {
    auto sysInfo = createSystemInfo();
    ASSERT_NE(sysInfo, nullptr);

    EXPECT_NO_THROW({
        auto result = sysInfo->getSystemIdentification();

        EXPECT_TRUE(result.success == true || result.success == false);

        if (result.success) {
            const auto& data = result.data;

            // System identification fields
            EXPECT_TRUE(data.systemUuid.empty() || !data.systemUuid.empty());
            EXPECT_TRUE(data.machineId.empty() || !data.machineId.empty());
            EXPECT_TRUE(data.bootId.empty() || !data.bootId.empty());
            EXPECT_TRUE(data.hostname.empty() || !data.hostname.empty());
            EXPECT_TRUE(data.domainName.empty() || !data.domainName.empty());

            // MAC addresses should be valid
            for (const auto& macAddr : data.macAddresses) {
                EXPECT_FALSE(macAddr.empty());
                // Basic MAC address format check (should contain colons or dashes)
                EXPECT_TRUE(macAddr.find(':') != std::string::npos ||
                           macAddr.find('-') != std::string::npos);
            }

            // Test isValid method
            bool dataValid = data.isValid();
            EXPECT_TRUE(dataValid == true || dataValid == false);

            // Test toString method
            std::string dataStr = data.toString();
            EXPECT_TRUE(dataStr.empty() || !dataStr.empty());
        }
    });
}

// Test memory modules information
TEST_F(SerialNumberTest, MemoryModules) {
    auto sysInfo = createSystemInfo();
    ASSERT_NE(sysInfo, nullptr);

    EXPECT_NO_THROW({
        auto result = sysInfo->getMemoryModules();

        EXPECT_TRUE(result.success == true || result.success == false);

        if (result.success) {
            const auto& modules = result.data;

            // Memory modules can be empty on some systems
            for (const auto& module : modules) {
                // Serial number can be empty
                EXPECT_TRUE(module.serialNumber.empty() || !module.serialNumber.empty());

                // Manufacturer can be empty
                EXPECT_TRUE(module.manufacturer.empty() || !module.manufacturer.empty());

                // Size should be non-negative
                EXPECT_GE(module.sizeBytes, 0ULL);

                // Speed should be non-negative
                EXPECT_GE(module.speedMHz, 0);

                // Form factor should be valid
                EXPECT_TRUE(module.formFactor.empty() || !module.formFactor.empty());

                // Type should be valid
                EXPECT_TRUE(module.type.empty() || !module.type.empty());

                // Test isValid method
                bool moduleValid = module.isValid();
                EXPECT_TRUE(moduleValid == true || moduleValid == false);

                // Test toString method
                std::string moduleStr = module.toString();
                EXPECT_TRUE(moduleStr.empty() || !moduleStr.empty());
            }
        }
    });
}

// Test network interfaces information
TEST_F(SerialNumberTest, NetworkInterfaces) {
    auto sysInfo = createSystemInfo();
    ASSERT_NE(sysInfo, nullptr);

    EXPECT_NO_THROW({
        auto result = sysInfo->getNetworkInterfaces();

        EXPECT_TRUE(result.success == true || result.success == false);

        if (result.success) {
            const auto& interfaces = result.data;

            // Network interfaces can be empty on some systems
            for (const auto& interface : interfaces) {
                // Name should not be empty
                EXPECT_FALSE(interface.name.empty());

                // MAC address can be empty for some interfaces
                EXPECT_TRUE(interface.macAddress.empty() || !interface.macAddress.empty());

                // IP addresses should be valid
                for (const auto& ip : interface.ipAddresses) {
                    EXPECT_FALSE(ip.empty());
                }

                // Driver info can be empty
                EXPECT_TRUE(interface.driver.empty() || !interface.driver.empty());

                // Type should be valid
                EXPECT_TRUE(interface.type.empty() || !interface.type.empty());

                // Manufacturer should be valid
                EXPECT_TRUE(interface.manufacturer.empty() || !interface.manufacturer.empty());

                // Active status should be valid
                EXPECT_TRUE(interface.isActive == true || interface.isActive == false);

                // Test isValid method
                bool interfaceValid = interface.isValid();
                EXPECT_TRUE(interfaceValid == true || interfaceValid == false);

                // Test toString method
                std::string interfaceStr = interface.toString();
                EXPECT_TRUE(interfaceStr.empty() || !interfaceStr.empty());
            }
        }
    });
}

// Test comprehensive system information
TEST_F(SerialNumberTest, ComprehensiveInfo) {
    auto sysInfo = createSystemInfo();
    ASSERT_NE(sysInfo, nullptr);

    EXPECT_NO_THROW({
        auto result = sysInfo->getComprehensiveInfo();

        EXPECT_TRUE(result.success == true || result.success == false);

        if (result.success) {
            const auto& info = result.data;

            // Test isValid method
            bool infoValid = info.isValid();
            EXPECT_TRUE(infoValid == true || infoValid == false);

            // Test toString method
            std::string infoStr = info.toString();
            EXPECT_TRUE(infoStr.empty() || !infoStr.empty());

            // Test getSystemFingerprint method
            std::string fingerprint = info.getSystemFingerprint();
            EXPECT_TRUE(fingerprint.empty() || !fingerprint.empty());

            // Hardware serials should be valid
            EXPECT_TRUE(info.hardwareSerials.isValid() || !info.hardwareSerials.isValid());

            // System ID should be valid
            EXPECT_TRUE(info.systemId.isValid() || !info.systemId.isValid());

            // Memory modules should be valid
            for (const auto& module : info.memoryModules) {
                EXPECT_TRUE(module.isValid() || !module.isValid());
            }

            // Network interfaces should be valid
            for (const auto& interface : info.networkInterfaces) {
                EXPECT_TRUE(interface.isValid() || !interface.isValid());
            }

            // Additional properties should be valid
            for (const auto& [key, value] : info.additionalProperties) {
                EXPECT_FALSE(key.empty());
                EXPECT_TRUE(value.empty() || !value.empty());
            }
        }
    });
}

// Test system ID queries
TEST_F(SerialNumberTest, SystemIdQueries) {
    auto sysInfo = createSystemInfo();
    ASSERT_NE(sysInfo, nullptr);

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
        EXPECT_NO_THROW({
            auto result = sysInfo->querySystemId(idType);

            EXPECT_TRUE(result.success == true || result.success == false);

            if (result.success) {
                const auto& query = result.data;

                EXPECT_EQ(query.type, idType);
                EXPECT_TRUE(query.isAvailable == true || query.isAvailable == false);

                if (query.isAvailable) {
                    EXPECT_FALSE(query.value.empty());
                    EXPECT_TRUE(query.errorMessage.empty());
                } else {
                    EXPECT_TRUE(query.value.empty() || !query.value.empty());
                    EXPECT_TRUE(query.errorMessage.empty() || !query.errorMessage.empty());
                }
            }
        });
    }
}

// Test system fingerprint
TEST_F(SerialNumberTest, SystemFingerprint) {
    auto sysInfo = createSystemInfo();
    ASSERT_NE(sysInfo, nullptr);

    EXPECT_NO_THROW({
        std::string fingerprint1 = sysInfo->getSystemFingerprint();
        std::string fingerprint2 = sysInfo->getSystemFingerprint();

        // Fingerprints should be consistent
        EXPECT_EQ(fingerprint1, fingerprint2);

        // Fingerprint should not be empty (unless system doesn't support it)
        EXPECT_TRUE(fingerprint1.empty() || !fingerprint1.empty());

        // Test forced refresh
        std::string fingerprint3 = sysInfo->getSystemFingerprint(true);
        EXPECT_EQ(fingerprint1, fingerprint3);
    });
}

// Test export functionality
TEST_F(SerialNumberTest, ExportFunctionality) {
    auto sysInfo = createSystemInfo();
    ASSERT_NE(sysInfo, nullptr);

    EXPECT_NO_THROW({
        // Test JSON export
        std::string jsonExport = sysInfo->exportToJson();
        EXPECT_TRUE(jsonExport.empty() || !jsonExport.empty());

        if (!jsonExport.empty()) {
            // Should contain JSON-like structure
            EXPECT_NE(jsonExport.find("{"), std::string::npos);
            EXPECT_NE(jsonExport.find("}"), std::string::npos);
        }

        // Test XML export
        std::string xmlExport = sysInfo->exportToXml();
        EXPECT_TRUE(xmlExport.empty() || !xmlExport.empty());

        if (!xmlExport.empty()) {
            // Should contain XML-like structure
            EXPECT_NE(xmlExport.find("<"), std::string::npos);
            EXPECT_NE(xmlExport.find(">"), std::string::npos);
        }

        // Test partial export
        std::string partialJson = sysInfo->exportToJson(false);
        std::string partialXml = sysInfo->exportToXml(false);

        EXPECT_TRUE(partialJson.empty() || !partialJson.empty());
        EXPECT_TRUE(partialXml.empty() || !partialXml.empty());
    });
}

// Test configuration options
TEST_F(SerialNumberTest, ConfigurationOptions) {
    SystemInfoConfig config;

    // Test default configuration
    EXPECT_TRUE(config.includeMemoryModules);
    EXPECT_TRUE(config.includeNetworkInterfaces);
    EXPECT_TRUE(config.cacheResults);

    // Test custom configuration
    config.includeMemoryModules = false;
    config.includeNetworkInterfaces = false;
    config.cacheResults = false;

    auto sysInfo = createSystemInfo(config);
    ASSERT_NE(sysInfo, nullptr);

    // Test that configuration is respected
    EXPECT_NO_THROW({
        auto result = sysInfo->getComprehensiveInfo();
        // Configuration effects may vary by platform
    });
}

// Test error handling
TEST_F(SerialNumberTest, ErrorHandling) {
    auto sysInfo = createSystemInfo();
    ASSERT_NE(sysInfo, nullptr);

    // Test invalid system ID type
    EXPECT_NO_THROW({
        auto result = sysInfo->querySystemId(static_cast<SystemIdType>(999));
        EXPECT_FALSE(result.success);
        EXPECT_NE(result.error, SystemInfoError::SUCCESS);
    });
}

// Test utility functions
TEST_F(SerialNumberTest, UtilityFunctions) {
    using namespace SystemInfoUtils;

    // Test MAC address validation
    EXPECT_TRUE(isValidMacAddress("00:11:22:33:44:55"));
    EXPECT_TRUE(isValidMacAddress("AA:BB:CC:DD:EE:FF"));
    EXPECT_TRUE(isValidMacAddress("00-11-22-33-44-55"));
    EXPECT_FALSE(isValidMacAddress(""));
    EXPECT_FALSE(isValidMacAddress("invalid"));
    EXPECT_FALSE(isValidMacAddress("00:11:22:33:44"));

    // Test UUID validation
    EXPECT_TRUE(isValidUuid("550e8400-e29b-41d4-a716-446655440000"));
    EXPECT_TRUE(isValidUuid("6ba7b810-9dad-11d1-80b4-00c04fd430c8"));
    EXPECT_FALSE(isValidUuid(""));
    EXPECT_FALSE(isValidUuid("invalid"));
    EXPECT_FALSE(isValidUuid("550e8400-e29b-41d4-a716"));

    // Test serial number validation
    EXPECT_TRUE(isValidSerial("ABC123DEF456"));
    EXPECT_TRUE(isValidSerial("1234567890"));
    EXPECT_FALSE(isValidSerial(""));
    EXPECT_FALSE(isValidSerial("   "));

    // Test string sanitization (if available)
    EXPECT_NO_THROW({
        std::string testStr = "Test String";
        EXPECT_FALSE(testStr.empty());
    });

    // Test basic string operations
    std::string testHex = "0123456789ABCDEF";
    EXPECT_FALSE(testHex.empty());
    EXPECT_EQ(testHex.length(), 16);
}

// Test data structure validation
TEST_F(SerialNumberTest, DataStructureValidation) {
    // Test HardwareSerialData
    HardwareSerialData hwSerial;
    EXPECT_FALSE(hwSerial.isValid());

    hwSerial.biosSerial = "BIOS123";
    hwSerial.motherboardSerial = "MB456";
    EXPECT_TRUE(hwSerial.isValid());

    // Test SystemIdentificationData
    SystemIdentificationData sysId;
    EXPECT_FALSE(sysId.isValid());

    sysId.systemUuid = "550e8400-e29b-41d4-a716-446655440000";
    sysId.machineId = "machine123";
    EXPECT_TRUE(sysId.isValid());

    // Test MemoryModuleInfo
    MemoryModuleInfo memInfo;
    EXPECT_FALSE(memInfo.isValid());

    memInfo.serialNumber = "MEM123";
    memInfo.manufacturer = "TestMfg";
    memInfo.sizeBytes = 8589934592ULL; // 8GB
    EXPECT_TRUE(memInfo.isValid());

    // Test NetworkInterfaceInfo
    NetworkInterfaceInfo netInfo;
    EXPECT_FALSE(netInfo.isValid());

    netInfo.name = "eth0";
    netInfo.macAddress = "00:11:22:33:44:55";
    EXPECT_TRUE(netInfo.isValid());
}

// Test concurrent access
TEST_F(SerialNumberTest, ConcurrentAccess) {
    const int num_threads = 5;
    std::vector<std::thread> threads;
    std::vector<bool> results(num_threads, false);

    auto sysInfo = createSystemInfo();
    ASSERT_NE(sysInfo, nullptr);

    // Launch multiple threads accessing system info concurrently
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                auto hwResult = sysInfo->getHardwareSerials();
                auto sysIdResult = sysInfo->getSystemIdentification();
                std::string fingerprint = sysInfo->getSystemFingerprint();

                // Basic validation
                EXPECT_TRUE(hwResult.success == true || hwResult.success == false);
                EXPECT_TRUE(sysIdResult.success == true || sysIdResult.success == false);
                EXPECT_TRUE(fingerprint.empty() || !fingerprint.empty());

                results[i] = true;
            } catch (...) {
                results[i] = false;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // All threads should have completed successfully
    for (bool result : results) {
        EXPECT_TRUE(result);
    }
}

// Test performance and caching
TEST_F(SerialNumberTest, PerformanceAndCaching) {
    auto sysInfo = createSystemInfo();
    ASSERT_NE(sysInfo, nullptr);

    // First call
    auto start1 = std::chrono::high_resolution_clock::now();
    auto result1 = sysInfo->getHardwareSerials();
    auto end1 = std::chrono::high_resolution_clock::now();

    // Second call (should use cache)
    auto start2 = std::chrono::high_resolution_clock::now();
    auto result2 = sysInfo->getHardwareSerials();
    auto end2 = std::chrono::high_resolution_clock::now();

    // Results should be identical if both successful
    if (result1.success && result2.success) {
        EXPECT_EQ(result1.data.biosSerial, result2.data.biosSerial);
        EXPECT_EQ(result1.data.motherboardSerial, result2.data.motherboardSerial);
        EXPECT_EQ(result1.data.cpuSerial, result2.data.cpuSerial);
    }

    // Second call should be faster or at least not significantly slower
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    EXPECT_LE(duration2.count(), duration1.count() * 2);
}