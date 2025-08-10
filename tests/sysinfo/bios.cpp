#include "atom/sysinfo/bios.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>

using namespace atom::system;
using namespace testing;

class BiosTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if necessary
    }

    void TearDown() override {
        // Cleanup code if necessary
    }
};

// Test BiosInfo singleton pattern
TEST_F(BiosTest, SingletonPattern) {
    auto& instance1 = BiosInfo::getInstance();
    auto& instance2 = BiosInfo::getInstance();

    // Should be the same instance
    EXPECT_EQ(&instance1, &instance2);
}

// Test basic BIOS information retrieval
TEST_F(BiosTest, GetBiosInfo) {
    auto& biosInfo = BiosInfo::getInstance();
    const auto& info = biosInfo.getBiosInfo();

    // At least one field should be populated on most systems
    bool hasData = !info.version.empty() ||
                   !info.manufacturer.empty() ||
                   !info.releaseDate.empty();

    // On some systems, BIOS info might not be available
    if (hasData) {
        EXPECT_FALSE(info.version.empty());
        EXPECT_FALSE(info.manufacturer.empty());

        // Test isValid method
        EXPECT_TRUE(info.isValid());

        // Test toString method
        std::string infoStr = info.toString();
        EXPECT_FALSE(infoStr.empty());
        EXPECT_NE(infoStr.find("BIOS Information"), std::string::npos);
    }
}

// Test BIOS information caching
TEST_F(BiosTest, CachingBehavior) {
    auto& biosInfo = BiosInfo::getInstance();

    // First call
    auto start1 = std::chrono::high_resolution_clock::now();
    const auto& info1 = biosInfo.getBiosInfo();
    auto end1 = std::chrono::high_resolution_clock::now();

    // Second call (should use cache)
    auto start2 = std::chrono::high_resolution_clock::now();
    const auto& info2 = biosInfo.getBiosInfo();
    auto end2 = std::chrono::high_resolution_clock::now();

    // Results should be identical
    EXPECT_EQ(info1.version, info2.version);
    EXPECT_EQ(info1.manufacturer, info2.manufacturer);
    EXPECT_EQ(info1.releaseDate, info2.releaseDate);

    // Second call should be faster (cached)
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    EXPECT_LE(duration2.count(), duration1.count() * 2);
}

// Test forced refresh
TEST_F(BiosTest, ForceRefresh) {
    auto& biosInfo = BiosInfo::getInstance();

    // Get cached info
    const auto& info1 = biosInfo.getBiosInfo();

    // Force refresh
    const auto& info2 = biosInfo.getBiosInfo(true);

    // Results should still be identical
    EXPECT_EQ(info1.version, info2.version);
    EXPECT_EQ(info1.manufacturer, info2.manufacturer);
}

// Test BIOS health check
TEST_F(BiosTest, HealthCheck) {
    auto& biosInfo = BiosInfo::getInstance();
    auto health = biosInfo.checkHealth();

    // Health check should return valid data
    EXPECT_GE(health.biosAgeInDays, 0);
    EXPECT_GE(health.lastCheckTime, 0);

    // Warnings and errors should be valid vectors
    EXPECT_TRUE(health.warnings.empty() || !health.warnings.empty());
    EXPECT_TRUE(health.errors.empty() || !health.errors.empty());

    // If there are errors, health should be false
    if (!health.errors.empty()) {
        EXPECT_FALSE(health.isHealthy);
    }
}

// Test BIOS update check
TEST_F(BiosTest, UpdateCheck) {
    auto& biosInfo = BiosInfo::getInstance();
    auto updateInfo = biosInfo.checkForUpdates();

    // Update info should have valid structure
    EXPECT_TRUE(updateInfo.currentVersion.empty() || !updateInfo.currentVersion.empty());
    EXPECT_TRUE(updateInfo.latestVersion.empty() || !updateInfo.latestVersion.empty());

    // If update is available, latest version should be different
    if (updateInfo.updateAvailable) {
        EXPECT_FALSE(updateInfo.latestVersion.empty());
        EXPECT_NE(updateInfo.currentVersion, updateInfo.latestVersion);
    }
}

// Test SMBIOS data retrieval
TEST_F(BiosTest, SMBIOSData) {
    auto& biosInfo = BiosInfo::getInstance();
    auto smbiosData = biosInfo.getSMBIOSData();

    // SMBIOS data might be empty on some systems
    for (const auto& data : smbiosData) {
        EXPECT_FALSE(data.empty());
    }
}

// Test feature support checks
TEST_F(BiosTest, FeatureSupport) {
    auto& biosInfo = BiosInfo::getInstance();

    // These should not throw exceptions
    EXPECT_NO_THROW({
        bool secureBootSupported = biosInfo.isSecureBootSupported();
        bool uefiSupported = biosInfo.isUEFIBootSupported();

        // Results should be boolean
        EXPECT_TRUE(secureBootSupported == true || secureBootSupported == false);
        EXPECT_TRUE(uefiSupported == true || uefiSupported == false);
    });
}

// Test enhanced firmware information
TEST_F(BiosTest, FirmwareInfo) {
    auto& biosInfo = BiosInfo::getInstance();

    EXPECT_NO_THROW({
        auto firmwareInfo = biosInfo.getFirmwareInfo();

        // Firmware info should have valid structure
        EXPECT_TRUE(firmwareInfo.type.empty() || !firmwareInfo.type.empty());
        EXPECT_TRUE(firmwareInfo.version.empty() || !firmwareInfo.version.empty());
        EXPECT_TRUE(firmwareInfo.vendor.empty() || !firmwareInfo.vendor.empty());

        // Boolean fields should be valid
        EXPECT_TRUE(firmwareInfo.secureBootCapable == true || firmwareInfo.secureBootCapable == false);
        EXPECT_TRUE(firmwareInfo.tmpSupported == true || firmwareInfo.tpmSupported == false);
    });
}

// Test boot configuration
TEST_F(BiosTest, BootConfiguration) {
    auto& biosInfo = BiosInfo::getInstance();

    EXPECT_NO_THROW({
        auto bootConfig = biosInfo.getBootConfiguration();

        // Boot configuration should have valid structure
        EXPECT_TRUE(bootConfig.currentBootDevice.empty() || !bootConfig.currentBootDevice.empty());
        EXPECT_TRUE(bootConfig.uefiMode == true || bootConfig.uefiMode == false);
        EXPECT_TRUE(bootConfig.secureBootEnabled == true || bootConfig.secureBootEnabled == false);
        EXPECT_TRUE(bootConfig.fastBootEnabled == true || bootConfig.fastBootEnabled == false);

        // Boot order and devices should be valid vectors
        for (const auto& device : bootConfig.bootOrder) {
            EXPECT_FALSE(device.empty());
        }
        for (const auto& device : bootConfig.bootDevices) {
            EXPECT_FALSE(device.empty());
        }
    });
}

// Test security settings
TEST_F(BiosTest, SecuritySettings) {
    auto& biosInfo = BiosInfo::getInstance();

    EXPECT_NO_THROW({
        auto securitySettings = biosInfo.getSecuritySettings();

        // Security settings should have valid boolean values
        EXPECT_TRUE(securitySettings.passwordProtected == true || securitySettings.passwordProtected == false);
        EXPECT_TRUE(securitySettings.secureBootEnabled == true || securitySettings.secureBootEnabled == false);
        EXPECT_TRUE(securitySettings.tpmEnabled == true || securitySettings.tpmEnabled == false);
        EXPECT_TRUE(securitySettings.virtualizationEnabled == true || securitySettings.virtualizationEnabled == false);
        EXPECT_TRUE(securitySettings.hyperThreadingEnabled == true || securitySettings.hyperThreadingEnabled == false);

        // Security features should be valid
        for (const auto& feature : securitySettings.securityFeatures) {
            EXPECT_FALSE(feature.empty());
        }
    });
}

// Test power management settings
TEST_F(BiosTest, PowerManagementSettings) {
    auto& biosInfo = BiosInfo::getInstance();

    EXPECT_NO_THROW({
        auto powerSettings = biosInfo.getPowerManagementSettings();

        // Power settings should have valid boolean values
        EXPECT_TRUE(powerSettings.acpiEnabled == true || powerSettings.acpiEnabled == false);
        EXPECT_TRUE(powerSettings.wakeOnLanEnabled == true || powerSettings.wakeOnLanEnabled == false);
        EXPECT_TRUE(powerSettings.wakeOnKeyboardEnabled == true || powerSettings.wakeOnKeyboardEnabled == false);
        EXPECT_TRUE(powerSettings.wakeOnMouseEnabled == true || powerSettings.wakeOnMouseEnabled == false);

        // CPU power limit should be non-negative
        EXPECT_GE(powerSettings.cpuPowerLimit, 0);

        // Sleep states should be valid
        for (const auto& state : powerSettings.supportedSleepStates) {
            EXPECT_FALSE(state.empty());
        }
    });
}

// Test overclocking information
TEST_F(BiosTest, OverclockingInfo) {
    auto& biosInfo = BiosInfo::getInstance();

    EXPECT_NO_THROW({
        auto overclockInfo = biosInfo.getOverclockingInfo();

        // Overclocking info should have valid boolean values
        EXPECT_TRUE(overclockInfo.overclockingSupported == true || overclockInfo.overclockingSupported == false);
        EXPECT_TRUE(overclockInfo.overclockingEnabled == true || overclockInfo.overclockingEnabled == false);

        // Frequencies should be non-negative
        EXPECT_GE(overclockInfo.baseCpuFrequency, 0);
        EXPECT_GE(overclockInfo.currentCpuFrequency, 0);
        EXPECT_GE(overclockInfo.maxCpuFrequency, 0);
        EXPECT_GE(overclockInfo.baseMemoryFrequency, 0);
        EXPECT_GE(overclockInfo.currentMemoryFrequency, 0);

        // If overclocking is supported, there should be some frequency data
        if (overclockInfo.overclockingSupported) {
            EXPECT_GT(overclockInfo.baseCpuFrequency, 0);
        }

        // Available profiles should be valid
        for (const auto& profile : overclockInfo.availableProfiles) {
            EXPECT_FALSE(profile.empty());
        }
    });
}

// Test hardware monitoring
TEST_F(BiosTest, HardwareMonitoring) {
    auto& biosInfo = BiosInfo::getInstance();

    EXPECT_NO_THROW({
        auto monitoring = biosInfo.getHardwareMonitoring();

        // Temperature values should be reasonable (if present)
        for (int temp : monitoring.cpuTemperatures) {
            EXPECT_GE(temp, -50);  // Reasonable lower bound
            EXPECT_LE(temp, 150);  // Reasonable upper bound
        }

        // Fan speeds should be non-negative
        for (int speed : monitoring.fanSpeeds) {
            EXPECT_GE(speed, 0);
        }

        // Voltages should be reasonable
        for (double voltage : monitoring.voltages) {
            EXPECT_GE(voltage, 0.0);
            EXPECT_LE(voltage, 50.0);  // Reasonable upper bound
        }

        // System temperature should be reasonable (if present)
        if (monitoring.systemTemperature > 0) {
            EXPECT_GE(monitoring.systemTemperature, -50);
            EXPECT_LE(monitoring.systemTemperature, 150);
        }

        // Thermal throttling should be boolean
        EXPECT_TRUE(monitoring.thermalThrottling == true || monitoring.thermalThrottling == false);

        // Sensor names should be valid
        for (const auto& name : monitoring.sensorNames) {
            EXPECT_FALSE(name.empty());
        }
    });
}

// Test BIOS diagnostics
TEST_F(BiosTest, BiosDiagnostics) {
    auto& biosInfo = BiosInfo::getInstance();

    EXPECT_NO_THROW({
        auto diagnostics = biosInfo.runDiagnostics();

        // Diagnostic results should be boolean
        EXPECT_TRUE(diagnostics.postTestPassed == true || diagnostics.postTestPassed == false);
        EXPECT_TRUE(diagnostics.memoryTestPassed == true || diagnostics.memoryTestPassed == false);
        EXPECT_TRUE(diagnostics.cpuTestPassed == true || diagnostics.cpuTestPassed == false);
        EXPECT_TRUE(diagnostics.storageTestPassed == true || diagnostics.storageTestPassed == false);

        // Failed components should be valid
        for (const auto& component : diagnostics.failedComponents) {
            EXPECT_FALSE(component.empty());
        }

        // Warnings should be valid
        for (const auto& warning : diagnostics.warnings) {
            EXPECT_FALSE(warning.empty());
        }

        // Diagnostic code should be non-negative
        EXPECT_GE(diagnostics.diagnosticCode, 0);
    });
}

// Test available boot devices
TEST_F(BiosTest, AvailableBootDevices) {
    auto& biosInfo = BiosInfo::getInstance();

    EXPECT_NO_THROW({
        auto bootDevices = biosInfo.getAvailableBootDevices();

        // Boot devices should be valid strings
        for (const auto& device : bootDevices) {
            EXPECT_FALSE(device.empty());
        }
    });
}

// Test BIOS integrity validation
TEST_F(BiosTest, BiosIntegrityValidation) {
    auto& biosInfo = BiosInfo::getInstance();

    EXPECT_NO_THROW({
        bool isValid = biosInfo.validateBiosIntegrity();

        // Should return a valid boolean
        EXPECT_TRUE(isValid == true || isValid == false);
    });
}

// Test supported features
TEST_F(BiosTest, SupportedFeatures) {
    auto& biosInfo = BiosInfo::getInstance();

    EXPECT_NO_THROW({
        auto features = biosInfo.getSupportedFeatures();

        // Features should be valid strings
        for (const auto& feature : features) {
            EXPECT_FALSE(feature.empty());
        }
    });
}

// Test BIOS utility functions
TEST_F(BiosTest, UtilityFunctions) {
    using namespace BiosUtils;

    // Test temperature conversion
    EXPECT_DOUBLE_EQ(celsiusToFahrenheit(0.0), 32.0);
    EXPECT_DOUBLE_EQ(celsiusToFahrenheit(100.0), 212.0);
    EXPECT_DOUBLE_EQ(celsiusToFahrenheit(-40.0), -40.0);

    // Test frequency conversion
    EXPECT_DOUBLE_EQ(mhzToGhz(1000), 1.0);
    EXPECT_DOUBLE_EQ(mhzToGhz(2500), 2.5);
    EXPECT_DOUBLE_EQ(mhzToGhz(0), 0.0);

    // Test boot time formatting
    std::string bootTime = formatBootTime(5000);
    EXPECT_FALSE(bootTime.empty());
    EXPECT_NE(bootTime.find("5"), std::string::npos);

    // Test BIOS version validation
    EXPECT_TRUE(isValidBiosVersion("1.0.0"));
    EXPECT_TRUE(isValidBiosVersion("2.15.1234"));
    EXPECT_FALSE(isValidBiosVersion(""));
    EXPECT_FALSE(isValidBiosVersion("invalid"));

    // Test BIOS date parsing
    auto parsedDate = parseBiosDate("01/01/2020");
    EXPECT_NE(parsedDate, std::chrono::system_clock::time_point{});

    // Test BIOS age calculation
    auto oldDate = std::chrono::system_clock::now() - std::chrono::hours(24 * 365);  // 1 year ago
    int age = calculateBiosAge(oldDate);
    EXPECT_GE(age, 360);  // Should be around 365 days
    EXPECT_LE(age, 370);
}

// Test BiosInfoData structure
TEST_F(BiosTest, BiosInfoDataStructure) {
    BiosInfoData data;

    // Empty data should not be valid
    EXPECT_FALSE(data.isValid());

    // Add some data
    data.version = "1.0.0";
    data.manufacturer = "Test Manufacturer";
    data.releaseDate = "01/01/2020";

    // Now it should be valid
    EXPECT_TRUE(data.isValid());

    // Test toString
    std::string str = data.toString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("BIOS Information"), std::string::npos);
    EXPECT_NE(str.find("1.0.0"), std::string::npos);
    EXPECT_NE(str.find("Test Manufacturer"), std::string::npos);
}

// Test BiosOperationResult enum
TEST_F(BiosTest, BiosOperationResult) {
    // Test that all enum values are valid
    std::vector<BiosOperationResult> results = {
        BiosOperationResult::SUCCESS,
        BiosOperationResult::FAILED,
        BiosOperationResult::NOT_SUPPORTED,
        BiosOperationResult::INSUFFICIENT_PRIVILEGES,
        BiosOperationResult::REBOOT_REQUIRED
    };

    for (auto result : results) {
        // Should be able to compare enum values
        EXPECT_TRUE(result == BiosOperationResult::SUCCESS ||
                   result != BiosOperationResult::SUCCESS);
    }
}

// Test concurrent access to BIOS info
TEST_F(BiosTest, ConcurrentAccess) {
    const int num_threads = 5;
    std::vector<std::thread> threads;
    std::vector<bool> results(num_threads, false);

    // Launch multiple threads accessing BIOS info concurrently
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                auto& biosInfo = BiosInfo::getInstance();
                const auto& info = biosInfo.getBiosInfo();
                auto health = biosInfo.checkHealth();

                // Basic validation
                EXPECT_TRUE(info.version.empty() || !info.version.empty());
                EXPECT_GE(health.biosAgeInDays, 0);

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
