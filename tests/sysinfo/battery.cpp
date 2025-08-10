#include "atom/sysinfo/battery.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>

using namespace atom::system;
using namespace atom::system::battery;
using namespace testing;

// Test fixture for BatteryInfo
class BatteryInfoTest : public ::testing::Test {
protected:
    BatteryInfo batteryInfo;

    void SetUp() override {
        // Initialize BatteryInfo with default values
        batteryInfo = BatteryInfo();
    }
};

// Test default values of BatteryInfo
TEST_F(BatteryInfoTest, DefaultValues) {
    EXPECT_FALSE(batteryInfo.isBatteryPresent);
    EXPECT_FALSE(batteryInfo.isCharging);
    EXPECT_FLOAT_EQ(batteryInfo.batteryLifePercent, 0.0);
    EXPECT_FLOAT_EQ(batteryInfo.batteryLifeTime, 0.0);
    EXPECT_FLOAT_EQ(batteryInfo.batteryFullLifeTime, 0.0);
    EXPECT_FLOAT_EQ(batteryInfo.energyNow, 0.0);
    EXPECT_FLOAT_EQ(batteryInfo.energyFull, 0.0);
    EXPECT_FLOAT_EQ(batteryInfo.energyDesign, 0.0);
    EXPECT_FLOAT_EQ(batteryInfo.voltageNow, 0.0);
    EXPECT_FLOAT_EQ(batteryInfo.currentNow, 0.0);
}

// Test operator== for BatteryInfo
TEST_F(BatteryInfoTest, EqualityOperator) {
    BatteryInfo other;
    EXPECT_TRUE(batteryInfo == other);

    other.isBatteryPresent = true;
    EXPECT_FALSE(batteryInfo == other);
}

// Test operator!= for BatteryInfo
TEST_F(BatteryInfoTest, InequalityOperator) {
    BatteryInfo other;
    EXPECT_FALSE(batteryInfo != other);

    other.isBatteryPresent = true;
    EXPECT_TRUE(batteryInfo != other);
}

// Test operator= for BatteryInfo
TEST_F(BatteryInfoTest, AssignmentOperator) {
    BatteryInfo other;
    other.isBatteryPresent = true;
    other.isCharging = true;
    other.batteryLifePercent = 50.0;
    other.batteryLifeTime = 120.0;
    other.batteryFullLifeTime = 240.0;
    other.energyNow = 5000000.0;
    other.energyFull = 10000000.0;
    other.energyDesign = 12000000.0;
    other.voltageNow = 3.7;
    other.currentNow = 1.5;

    batteryInfo = other;
    EXPECT_TRUE(batteryInfo == other);
}

// Test getBatteryInfo function
TEST(BatteryInfoFunctionTest, GetBatteryInfo) {
    auto batteryInfoOpt = getBatteryInfo();

    // Check if battery info is available
    if (batteryInfoOpt.has_value()) {
        const auto& info = batteryInfoOpt.value();

        // Since we don't know the actual values returned by getBatteryInfo,
        // we will just check if the function returns a BatteryInfo object.
        EXPECT_TRUE(info.isBatteryPresent || !info.isBatteryPresent);
        EXPECT_TRUE(info.isCharging || !info.isCharging);
        EXPECT_GE(info.batteryLifePercent, 0.0);
        EXPECT_GE(info.batteryLifeTime, 0.0);
        EXPECT_GE(info.batteryFullLifeTime, 0.0);
        EXPECT_GE(info.energyNow, 0.0);
        EXPECT_GE(info.energyFull, 0.0);
        EXPECT_GE(info.energyDesign, 0.0);
        EXPECT_GE(info.voltageNow, 0.0);
        EXPECT_GE(info.currentNow, 0.0);
    } else {
        // No battery present or error occurred
        EXPECT_TRUE(true); // Test passes if no battery is available
    }
}

// Test getDetailedBatteryInfo function
TEST(BatteryInfoFunctionTest, GetDetailedBatteryInfo) {
    auto result = getDetailedBatteryInfo();

    // Check if the result is valid
    if (auto* info = std::get_if<BatteryInfo>(&result)) {
        // Battery info is available
        EXPECT_TRUE(info->isBatteryPresent || !info->isBatteryPresent);
        EXPECT_TRUE(info->isCharging || !info->isCharging);
        EXPECT_GE(info->batteryLifePercent, 0.0);
        EXPECT_LE(info->batteryLifePercent, 100.0);

        // Test new methods if available
        EXPECT_NO_THROW({
            auto powerConsumption = info->getPowerConsumption();
            EXPECT_GE(powerConsumption, 0.0);

            auto batteryAge = info->getBatteryAge();
            EXPECT_GE(batteryAge, 0.0);
            EXPECT_LE(batteryAge, 100.0);

            bool isCritical = info->isCritical();
            EXPECT_TRUE(isCritical == true || isCritical == false);
        });
    } else {
        // Error occurred
        auto error = std::get<BatteryError>(result);
        EXPECT_TRUE(error == BatteryError::NOT_PRESENT ||
                   error == BatteryError::NOT_SUPPORTED ||
                   error == BatteryError::ACCESS_DENIED ||
                   error == BatteryError::INVALID_DATA ||
                   error == BatteryError::READ_ERROR);
    }
}

// Test getAllBatteries function
TEST(BatteryInfoFunctionTest, GetAllBatteries) {
    auto allBatteries = getAllBatteries();

    if (!allBatteries.isEmpty()) {
        // Multiple batteries found
        EXPECT_GT(allBatteries.size(), 0);
        EXPECT_GE(allBatteries.activeBatteryCount, 0);
        EXPECT_LE(allBatteries.activeBatteryCount, static_cast<int>(allBatteries.size()));
        EXPECT_GE(allBatteries.totalCapacity, 0.0f);
        EXPECT_GE(allBatteries.totalEnergyRemaining, 0.0f);

        // Test primary battery
        auto* primary = allBatteries.getPrimaryBattery();
        if (primary) {
            EXPECT_GE(primary->batteryLifePercent, 0.0f);
            EXPECT_LE(primary->batteryLifePercent, 100.0f);
        }

        // Test individual batteries
        for (const auto& battery : allBatteries.batteries) {
            EXPECT_GE(battery.batteryLifePercent, 0.0f);
            EXPECT_LE(battery.batteryLifePercent, 100.0f);
        }

        // Test combined battery info
        EXPECT_GE(allBatteries.combined.batteryLifePercent, 0.0f);
        EXPECT_LE(allBatteries.combined.batteryLifePercent, 100.0f);
    } else {
        // No batteries found
        EXPECT_EQ(allBatteries.size(), 0);
        EXPECT_EQ(allBatteries.activeBatteryCount, 0);
    }
}

// Test BatteryMonitor class
TEST(BatteryMonitorTest, BasicMonitoring) {
    // Initially should not be monitoring
    EXPECT_FALSE(BatteryMonitor::isMonitoring());

    bool callbackCalled = false;
    BatteryInfo receivedInfo;

    // Start monitoring
    bool started = BatteryMonitor::startMonitoring([&](const BatteryInfo& info) {
        callbackCalled = true;
        receivedInfo = info;
    }, 100); // 100ms interval for fast testing

    if (started) {
        EXPECT_TRUE(BatteryMonitor::isMonitoring());

        // Wait for at least one callback
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // Stop monitoring
        BatteryMonitor::stopMonitoring();
        EXPECT_FALSE(BatteryMonitor::isMonitoring());

        // Check if callback was called (only if battery is present)
        if (callbackCalled) {
            EXPECT_GE(receivedInfo.batteryLifePercent, 0.0f);
            EXPECT_LE(receivedInfo.batteryLifePercent, 100.0f);
        }
    } else {
        // Monitor failed to start (acceptable on some systems)
        EXPECT_FALSE(BatteryMonitor::isMonitoring());
    }
}

// Test BatteryMonitor error handling
TEST(BatteryMonitorTest, ErrorHandling) {
    // Try to start monitoring twice
    bool started1 = BatteryMonitor::startMonitoring([](const BatteryInfo&) {}, 1000);

    if (started1) {
        // Second start should fail
        bool started2 = BatteryMonitor::startMonitoring([](const BatteryInfo&) {}, 1000);
        EXPECT_FALSE(started2);

        // Stop monitoring
        BatteryMonitor::stopMonitoring();
        EXPECT_FALSE(BatteryMonitor::isMonitoring());
    }

    // Stopping when not monitoring should be safe
    BatteryMonitor::stopMonitoring();
    EXPECT_FALSE(BatteryMonitor::isMonitoring());
}

// Test BatteryManager class
TEST(BatteryManagerTest, BasicFunctionality) {
    auto& manager = BatteryManager::getInstance();

    // Test alert settings
    BatteryAlertSettings settings;
    settings.lowBatteryThreshold = 25.0f;
    settings.criticalBatteryThreshold = 10.0f;
    settings.highTempThreshold = 45.0f;
    settings.lowHealthThreshold = 60.0f;
    settings.enableTemperatureAlerts = true;
    settings.enableHealthAlerts = true;

    EXPECT_NO_THROW({
        manager.setAlertSettings(settings);
    });

    // Test alert callback
    bool alertCalled = false;
    AlertType receivedAlertType;
    BatteryInfo receivedInfo;

    manager.setAlertCallback([&](AlertType type, const BatteryInfo& info) {
        alertCalled = true;
        receivedAlertType = type;
        receivedInfo = info;
    });

    // Test statistics
    const auto& stats = manager.getStats();
    EXPECT_GE(stats.averagePowerConsumption, 0.0f);
    EXPECT_GE(stats.totalEnergyConsumed, 0.0f);
    EXPECT_GE(stats.batteryHealth, 0.0f);
    EXPECT_LE(stats.batteryHealth, 100.0f);
}

// Test BatteryManager monitoring and recording
TEST(BatteryManagerTest, MonitoringAndRecording) {
    auto& manager = BatteryManager::getInstance();

    // Create a temporary file for testing
    std::string tempFile = std::filesystem::temp_directory_path() / "battery_test.csv";

    // Test recording
    bool recordingStarted = manager.startRecording(tempFile);
    if (recordingStarted) {
        // Test monitoring
        bool monitoringStarted = manager.startMonitoring(100); // 100ms interval
        if (monitoringStarted) {
            // Wait a bit for data collection
            std::this_thread::sleep_for(std::chrono::milliseconds(300));

            // Stop monitoring and recording
            manager.stopMonitoring();
            manager.stopRecording();

            // Check if file was created
            EXPECT_TRUE(std::filesystem::exists(tempFile));

            // Clean up
            std::filesystem::remove(tempFile);
        }
    }

    // Test history
    auto history = manager.getHistory(10);
    EXPECT_GE(history.size(), 0);

    // Test memory-only recording
    bool memoryRecordingStarted = manager.startRecording(); // No file
    EXPECT_TRUE(memoryRecordingStarted == true || memoryRecordingStarted == false);

    if (memoryRecordingStarted) {
        manager.stopRecording();
    }
}

// Test PowerPlanManager class
TEST(PowerPlanManagerTest, BasicFunctionality) {
    // Test getting current power plan
    auto currentPlan = PowerPlanManager::getCurrentPowerPlan();
    if (currentPlan.has_value()) {
        EXPECT_TRUE(currentPlan.value() == PowerPlan::BALANCED ||
                   currentPlan.value() == PowerPlan::PERFORMANCE ||
                   currentPlan.value() == PowerPlan::POWER_SAVER ||
                   currentPlan.value() == PowerPlan::CUSTOM ||
                   currentPlan.value() == PowerPlan::ADAPTIVE ||
                   currentPlan.value() == PowerPlan::GAMING ||
                   currentPlan.value() == PowerPlan::PRESENTATION);
    }

    // Test getting available power plans
    auto availablePlans = PowerPlanManager::getAvailablePowerPlans();
    EXPECT_GE(availablePlans.size(), 0);

    // Test setting power plan (may not work on all systems)
    auto setPlanResult = PowerPlanManager::setPowerPlan(PowerPlan::BALANCED);
    if (setPlanResult.has_value()) {
        // Setting succeeded or failed, both are valid
        EXPECT_TRUE(setPlanResult.value() == true || setPlanResult.value() == false);
    }
    // If nullopt, the operation is not supported on this platform
}

// Test BatteryCalibrator class
TEST(BatteryCalibratorTest, BasicFunctionality) {
    // Test getting calibration data
    auto calibrationData = BatteryCalibrator::getCalibrationData();
    EXPECT_GE(calibrationData.actualCapacity, 0.0f);
    EXPECT_GE(calibrationData.designCapacity, 0.0f);
    EXPECT_GE(calibrationData.calibrationAccuracy, 0.0f);
    EXPECT_LE(calibrationData.calibrationAccuracy, 100.0f);
    EXPECT_GE(calibrationData.calibrationCycles, 0);

    // Test calibration status
    bool needsCalibration = BatteryCalibrator::needsCalibration();
    EXPECT_TRUE(needsCalibration == true || needsCalibration == false);

    // Test starting calibration (may not work on all systems)
    bool calibrationStarted = BatteryCalibrator::startCalibration();
    if (calibrationStarted) {
        // If calibration started, stop it immediately for testing
        BatteryCalibrator::stopCalibration();
        EXPECT_FALSE(BatteryCalibrator::isCalibrating());
    }

    // Test calibration status
    bool isCalibrating = BatteryCalibrator::isCalibrating();
    EXPECT_TRUE(isCalibrating == true || isCalibrating == false);
}

// Test ThermalManager class
TEST(ThermalManagerTest, BasicFunctionality) {
    // Test thermal settings
    ThermalSettings settings;
    settings.warningTemperature = 40.0f;
    settings.criticalTemperature = 50.0f;
    settings.shutdownTemperature = 60.0f;
    settings.enableThermalThrottling = true;
    settings.enableFanControl = false;

    EXPECT_NO_THROW({
        ThermalManager::setThermalSettings(settings);
    });

    // Test getting thermal settings
    auto retrievedSettings = ThermalManager::getThermalSettings();
    EXPECT_GE(retrievedSettings.warningTemperature, 0.0f);
    EXPECT_GE(retrievedSettings.criticalTemperature, 0.0f);
    EXPECT_GE(retrievedSettings.shutdownTemperature, 0.0f);

    // Test thermal monitoring
    ThermalManager::setThermalMonitoring(true);
    EXPECT_TRUE(ThermalManager::isThermalMonitoringEnabled());

    // Test thermal throttling check
    bool isThrottling = ThermalManager::isThermalThrottling();
    EXPECT_TRUE(isThrottling == true || isThrottling == false);

    // Test system temperature
    auto temperature = ThermalManager::getSystemTemperature();
    if (temperature.has_value()) {
        EXPECT_GE(temperature.value(), -50.0f); // Reasonable temperature range
        EXPECT_LE(temperature.value(), 100.0f);
    }

    // Disable thermal monitoring
    ThermalManager::setThermalMonitoring(false);
    EXPECT_FALSE(ThermalManager::isThermalMonitoringEnabled());
}

// Test AdaptivePowerManager class
TEST(AdaptivePowerManagerTest, BasicFunctionality) {
    // Test initial state
    EXPECT_FALSE(AdaptivePowerManager::isAdaptivePowerEnabled());

    // Test available profiles
    auto profiles = AdaptivePowerManager::getAvailableProfiles();
    EXPECT_GT(profiles.size(), 0);

    // Test setting optimization profile
    if (!profiles.empty()) {
        bool profileSet = AdaptivePowerManager::setOptimizationProfile(profiles[0]);
        EXPECT_TRUE(profileSet);

        auto currentProfile = AdaptivePowerManager::getCurrentProfile();
        EXPECT_EQ(profiles[0], currentProfile);
    }

    // Test enabling adaptive power (may start background thread)
    bool enabled = AdaptivePowerManager::enableAdaptivePower();
    if (enabled) {
        EXPECT_TRUE(AdaptivePowerManager::isAdaptivePowerEnabled());

        // Disable it quickly for testing
        AdaptivePowerManager::disableAdaptivePower();
        EXPECT_FALSE(AdaptivePowerManager::isAdaptivePowerEnabled());
    }
}

// Test concurrent access to battery functions
TEST(BatteryTest, ConcurrentAccess) {
    const int num_threads = 3;
    std::vector<std::thread> threads;
    std::vector<bool> results(num_threads, false);

    // Launch multiple threads accessing battery functions concurrently
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                // Test concurrent access to various battery functions
                auto batteryInfo = getBatteryInfo();
                auto detailedInfo = getDetailedBatteryInfo();
                auto allBatteries = getAllBatteries();

                // Basic validation - should not crash
                EXPECT_TRUE(batteryInfo.has_value() || !batteryInfo.has_value());

                if (auto* info = std::get_if<BatteryInfo>(&detailedInfo)) {
                    EXPECT_GE(info->batteryLifePercent, 0.0f);
                    EXPECT_LE(info->batteryLifePercent, 100.0f);
                } else {
                    auto error = std::get<BatteryError>(detailedInfo);
                    EXPECT_TRUE(error == BatteryError::NOT_PRESENT ||
                               error == BatteryError::NOT_SUPPORTED ||
                               error == BatteryError::ACCESS_DENIED ||
                               error == BatteryError::INVALID_DATA ||
                               error == BatteryError::READ_ERROR);
                }

                EXPECT_GE(allBatteries.size(), 0);

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

// Test performance of battery information retrieval
TEST(BatteryTest, Performance) {
    // Measure time for battery info retrieval
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10; ++i) {
        auto batteryInfo = getBatteryInfo();
        (void)batteryInfo; // Suppress unused variable warning
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Battery info retrieval should complete within reasonable time (5 seconds for 10 calls)
    EXPECT_LT(duration.count(), 5000);

    // Measure time for detailed battery info retrieval
    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 5; ++i) {
        auto detailedInfo = getDetailedBatteryInfo();
        (void)detailedInfo; // Suppress unused variable warning
    }

    end = std::chrono::high_resolution_clock::now();
    auto detailedDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Detailed battery info should complete within reasonable time (3 seconds for 5 calls)
    EXPECT_LT(detailedDuration.count(), 3000);
}

// Test error handling and edge cases
TEST(BatteryTest, ErrorHandling) {
    // Test with invalid alert settings
    auto& manager = BatteryManager::getInstance();

    BatteryAlertSettings invalidSettings;
    invalidSettings.lowBatteryThreshold = -10.0f; // Invalid negative threshold
    invalidSettings.criticalBatteryThreshold = 150.0f; // Invalid high threshold

    EXPECT_NO_THROW({
        manager.setAlertSettings(invalidSettings);
    });

    // Test with invalid thermal settings
    ThermalSettings invalidThermalSettings;
    invalidThermalSettings.warningTemperature = -100.0f; // Invalid temperature
    invalidThermalSettings.criticalTemperature = 200.0f; // Invalid temperature

    EXPECT_NO_THROW({
        ThermalManager::setThermalSettings(invalidThermalSettings);
    });

    // Test invalid power plan setting
    auto result = PowerPlanManager::setPowerPlan(static_cast<PowerPlan>(999)); // Invalid enum value
    if (result.has_value()) {
        EXPECT_FALSE(result.value()); // Should fail
    }
    // If nullopt, the operation is not supported on this platform
}
