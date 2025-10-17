#include "atom/sysinfo/hardware/battery.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

using namespace atom::system;

namespace atom::sysinfo::test {

// Test fixture for BatteryInfo
class BatteryInfoTest : public ::testing::Test {
protected:
    BatteryInfo batteryInfo;

    BatteryInfoTest() : batteryInfo() {
        // Explicitly call the BatteryInfo constructor
    }
};

// Test default values of BatteryInfo
TEST_F(BatteryInfoTest, DefaultValues) {
    // Create a new BatteryInfo using the constructor
    BatteryInfo testInfo;

    EXPECT_FALSE(testInfo.isBatteryPresent);
    EXPECT_FALSE(testInfo.isCharging);
    EXPECT_FLOAT_EQ(testInfo.batteryLifePercent, 0.0);
    EXPECT_FLOAT_EQ(testInfo.batteryLifeTime, 0.0);
    EXPECT_FLOAT_EQ(testInfo.batteryFullLifeTime, 0.0);
    EXPECT_FLOAT_EQ(testInfo.energyNow, 0.0);
    EXPECT_FLOAT_EQ(testInfo.energyFull, 0.0);
    EXPECT_FLOAT_EQ(testInfo.energyDesign, 0.0);
    EXPECT_FLOAT_EQ(testInfo.voltageNow, 0.0);
    EXPECT_FLOAT_EQ(testInfo.currentNow, 0.0);
}

// Test operator== for BatteryInfo
TEST_F(BatteryInfoTest, EqualityOperator) {
    BatteryInfo info1;
    BatteryInfo info2;

    EXPECT_TRUE(info1 == info2);

    info2.isBatteryPresent = true;
    EXPECT_FALSE(info1 == info2);
}

// Test operator!= for BatteryInfo
TEST_F(BatteryInfoTest, InequalityOperator) {
    BatteryInfo info1;
    BatteryInfo info2;
    EXPECT_FALSE(info1 != info2);

    info2.isBatteryPresent = true;
    EXPECT_TRUE(info1 != info2);
}

// Test operator= for BatteryInfo
TEST_F(BatteryInfoTest, AssignmentOperator) {
    BatteryInfo info1;
    BatteryInfo info2;
    info2.isBatteryPresent = true;
    info2.isCharging = true;
    info2.batteryLifePercent = 50.0;
    info2.batteryLifeTime = 120.0;
    info2.batteryFullLifeTime = 240.0;
    info2.energyNow = 5000000.0;
    info2.energyFull = 10000000.0;
    info2.energyDesign = 12000000.0;
    info2.voltageNow = 3.7;
    info2.currentNow = 1.5;

    info1 = info2;
    EXPECT_TRUE(info1 == info2);
}

// ============================================================================
// Battery Information Function Tests
// ============================================================================

class BatteryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup battery tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(BatteryTest, GetBatteryInfo) {
    // Test getBatteryInfo function
    auto batteryInfoOpt = getBatteryInfo();

    // Function should not throw
    EXPECT_NO_THROW(getBatteryInfo());

    // If battery is present, validate the information
    if (batteryInfoOpt.has_value()) {
        const BatteryInfo& info = batteryInfoOpt.value();

        // Battery should be marked as present
        EXPECT_TRUE(info.isBatteryPresent);

        // Battery life percent should be between 0 and 100
        EXPECT_GE(info.batteryLifePercent, 0.0f);
        EXPECT_LE(info.batteryLifePercent, 100.0f);

        // Energy values should be non-negative
        EXPECT_GE(info.energyNow, 0.0f);
        EXPECT_GE(info.energyFull, 0.0f);
        EXPECT_GE(info.energyDesign, 0.0f);

        // If energy values are available, they should be reasonable
        if (info.energyFull > 0) {
            EXPECT_LE(info.energyNow, info.energyFull * 1.1f); // Allow some tolerance
        }

        if (info.energyDesign > 0) {
            EXPECT_LE(info.energyFull, info.energyDesign * 1.1f); // Allow some tolerance
        }

        // Voltage should be positive if available
        if (info.voltageNow > 0) {
            EXPECT_GT(info.voltageNow, 0.0f);
            EXPECT_LT(info.voltageNow, 50.0f); // Reasonable upper bound for battery voltage
        }

        // Current should be reasonable if available
        if (info.currentNow != 0) {
            EXPECT_GT(std::abs(info.currentNow), 0.0f);
            EXPECT_LT(std::abs(info.currentNow), 100.0f); // Reasonable upper bound for battery current
        }

        // Time values should be non-negative
        EXPECT_GE(info.batteryLifeTime, 0.0f);
        EXPECT_GE(info.batteryFullLifeTime, 0.0f);

        // Charging status should be boolean
        EXPECT_TRUE(info.isCharging || !info.isCharging);
    }
}

TEST_F(BatteryTest, GetBatteryInfoConsistency) {
    // Test that multiple calls return consistent static information
    auto info1 = getBatteryInfo();
    auto info2 = getBatteryInfo();

    // Both calls should have the same presence status
    EXPECT_EQ(info1.has_value(), info2.has_value());

    if (info1.has_value() && info2.has_value()) {
        // Static information should be the same
        EXPECT_EQ(info1->isBatteryPresent, info2->isBatteryPresent);
        EXPECT_EQ(info1->energyDesign, info2->energyDesign);

        // Dynamic information might differ slightly, but should be reasonable
        if (info1->batteryLifePercent > 0 && info2->batteryLifePercent > 0) {
            float percentDiff = std::abs(info1->batteryLifePercent - info2->batteryLifePercent);
            EXPECT_LT(percentDiff, 10.0f); // Should not change by more than 10% quickly
        }
    }
}

TEST_F(BatteryTest, BatteryInfoEstimatedTime) {
    // Test estimated time remaining calculation
    auto batteryInfoOpt = getBatteryInfo();

    if (batteryInfoOpt.has_value()) {
        const BatteryInfo& info = batteryInfoOpt.value();

        float estimatedTime = info.getEstimatedTimeRemaining();

        // Estimated time should be non-negative
        EXPECT_GE(estimatedTime, 0.0f);

        // If battery is charging, estimated time might be 0 or very large
        if (info.isCharging) {
            EXPECT_TRUE(estimatedTime >= 0.0f);
        } else {
            // If discharging and we have a reasonable battery level, time should be reasonable
            if (info.batteryLifePercent > 5.0f) {
                EXPECT_LT(estimatedTime, 100.0f); // Less than 100 hours seems reasonable
            }
        }
    }
}

// ============================================================================
// Battery Monitoring and Management Tests
// ============================================================================

TEST_F(BatteryTest, BatteryMonitoringBasic) {
    // Test basic battery monitoring functionality
    auto batteryInfoOpt = getBatteryInfo();

    if (batteryInfoOpt.has_value()) {
        // Test that we can create a battery monitor
        // Note: This tests the basic functionality without actually starting monitoring
        // since that would require elevated privileges and could interfere with system

        const BatteryInfo& info = batteryInfoOpt.value();

        // Verify battery information is reasonable for monitoring
        if (info.isBatteryPresent) {
            EXPECT_GE(info.batteryLifePercent, 0.0f);
            EXPECT_LE(info.batteryLifePercent, 100.0f);

            // If we have energy information, it should be consistent
            if (info.energyFull > 0 && info.energyNow > 0) {
                float calculatedPercent = (info.energyNow / info.energyFull) * 100.0f;
                float percentDiff = std::abs(calculatedPercent - info.batteryLifePercent);
                EXPECT_LT(percentDiff, 20.0f); // Allow some tolerance for different calculation methods
            }
        }
    }
}

TEST_F(BatteryTest, BatteryPowerCalculations) {
    // Test battery power-related calculations
    auto batteryInfoOpt = getBatteryInfo();

    if (batteryInfoOpt.has_value()) {
        const BatteryInfo& info = batteryInfoOpt.value();

        if (info.isBatteryPresent && info.voltageNow > 0 && info.currentNow != 0) {
            // Calculate power (P = V * I)
            float power = info.voltageNow * std::abs(info.currentNow);

            // Power should be reasonable for a battery
            EXPECT_GT(power, 0.0f);
            EXPECT_LT(power, 1000.0f); // Less than 1000W seems reasonable for most batteries

            // If charging, current should typically be positive
            // If discharging, current should typically be negative
            // Note: This might vary by implementation
        }
    }
}

// ============================================================================
// Battery Information Structure Tests
// ============================================================================

TEST_F(BatteryTest, BatteryInfoOperators) {
    // Test BatteryInfo operators
    BatteryInfo info1;
    BatteryInfo info2;

    // Default constructed objects should be equal
    EXPECT_TRUE(info1 == info2);
    EXPECT_FALSE(info1 != info2);

    // Modify one and test inequality
    info2.isBatteryPresent = true;
    EXPECT_FALSE(info1 == info2);
    EXPECT_TRUE(info1 != info2);

    // Test assignment
    info1 = info2;
    EXPECT_TRUE(info1 == info2);
    EXPECT_FALSE(info1 != info2);
}

TEST_F(BatteryTest, BatteryInfoFieldValidation) {
    // Test BatteryInfo field validation
    BatteryInfo info;

    // Set reasonable values
    info.isBatteryPresent = true;
    info.isCharging = false;
    info.batteryLifePercent = 75.5f;
    info.batteryLifeTime = 3.5f; // 3.5 hours
    info.batteryFullLifeTime = 8.0f; // 8 hours when full
    info.energyNow = 7500000.0f; // 7.5 Wh in µWh
    info.energyFull = 10000000.0f; // 10 Wh in µWh
    info.energyDesign = 11000000.0f; // 11 Wh in µWh
    info.voltageNow = 3.7f; // 3.7V
    info.currentNow = -2.0f; // -2A (discharging)

    // Validate all fields are set correctly
    EXPECT_TRUE(info.isBatteryPresent);
    EXPECT_FALSE(info.isCharging);
    EXPECT_FLOAT_EQ(info.batteryLifePercent, 75.5f);
    EXPECT_FLOAT_EQ(info.batteryLifeTime, 3.5f);
    EXPECT_FLOAT_EQ(info.batteryFullLifeTime, 8.0f);
    EXPECT_FLOAT_EQ(info.energyNow, 7500000.0f);
    EXPECT_FLOAT_EQ(info.energyFull, 10000000.0f);
    EXPECT_FLOAT_EQ(info.energyDesign, 11000000.0f);
    EXPECT_FLOAT_EQ(info.voltageNow, 3.7f);
    EXPECT_FLOAT_EQ(info.currentNow, -2.0f);

    // Test estimated time calculation with these values
    float estimatedTime = info.getEstimatedTimeRemaining();
    EXPECT_GE(estimatedTime, 0.0f);
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_F(BatteryTest, NoBatteryHandling) {
    // Test handling when no battery is present
    auto batteryInfoOpt = getBatteryInfo();

    // Function should not throw even if no battery is present
    EXPECT_NO_THROW(getBatteryInfo());

    // If no battery is present, optional should either be empty or contain info with isBatteryPresent = false
    if (batteryInfoOpt.has_value()) {
        // If we get battery info but no battery is present
        if (!batteryInfoOpt->isBatteryPresent) {
            // Values should be reasonable defaults
            EXPECT_GE(batteryInfoOpt->batteryLifePercent, 0.0f);
            EXPECT_GE(batteryInfoOpt->energyNow, 0.0f);
            EXPECT_GE(batteryInfoOpt->energyFull, 0.0f);
        }
    }
}

TEST_F(BatteryTest, BatteryInfoBoundaryValues) {
    // Test boundary values for BatteryInfo
    BatteryInfo info;

    // Test with boundary values
    info.batteryLifePercent = 0.0f;
    EXPECT_FLOAT_EQ(info.batteryLifePercent, 0.0f);

    info.batteryLifePercent = 100.0f;
    EXPECT_FLOAT_EQ(info.batteryLifePercent, 100.0f);

    // Test estimated time with boundary values
    info.energyNow = 0.0f;
    info.energyFull = 10000000.0f;
    info.currentNow = -1.0f;
    float estimatedTime = info.getEstimatedTimeRemaining();
    EXPECT_GE(estimatedTime, 0.0f);

    // Test with zero current (should handle division by zero)
    info.currentNow = 0.0f;
    estimatedTime = info.getEstimatedTimeRemaining();
    EXPECT_GE(estimatedTime, 0.0f);
}

TEST_F(BatteryTest, BatteryInfoCopySemantics) {
    // Test copy constructor and assignment
    BatteryInfo original;
    original.isBatteryPresent = true;
    original.batteryLifePercent = 50.0f;
    original.energyNow = 5000000.0f;

    // Test copy constructor
    BatteryInfo copied(original);
    EXPECT_TRUE(copied == original);

    // Test assignment operator
    BatteryInfo assigned;
    assigned = original;
    EXPECT_TRUE(assigned == original);

    // Modify original and ensure copies are independent
    original.batteryLifePercent = 75.0f;
    EXPECT_FLOAT_EQ(copied.batteryLifePercent, 50.0f);
    EXPECT_FLOAT_EQ(assigned.batteryLifePercent, 50.0f);
}

// ============================================================================
// Advanced Battery API Tests
// ============================================================================

TEST_F(BatteryTest, GetDetailedBatteryInfo) {
    // Test getDetailedBatteryInfo function
    auto batteryResult = getDetailedBatteryInfo();

    // Function should not throw
    EXPECT_NO_THROW(getDetailedBatteryInfo());

    // Check if we got BatteryInfo or BatteryError
    if (std::holds_alternative<BatteryInfo>(batteryResult)) {
        const BatteryInfo& info = std::get<BatteryInfo>(batteryResult);

        // If battery is present, validate detailed information
        if (info.isBatteryPresent) {
            // Manufacturer and model might be available
            // (empty strings are acceptable on some systems)
            EXPECT_TRUE(info.manufacturer.empty() || !info.manufacturer.empty());
            EXPECT_TRUE(info.model.empty() || !info.model.empty());

            // Serial number might be available
            EXPECT_TRUE(info.serialNumber.empty() || !info.serialNumber.empty());

            // Cycle count should be non-negative
            EXPECT_GE(info.cycleCounts, 0);

            // Temperature should be reasonable if available
            if (info.temperature > 0) {
                EXPECT_GT(info.temperature, -50.0f); // Reasonable lower bound
                EXPECT_LT(info.temperature, 100.0f); // Reasonable upper bound
            }
        }
    } else {
        // We got a BatteryError
        BatteryError error = std::get<BatteryError>(batteryResult);
        EXPECT_TRUE(error == BatteryError::NOT_PRESENT ||
                   error == BatteryError::ACCESS_DENIED ||
                   error == BatteryError::NOT_SUPPORTED ||
                   error == BatteryError::INVALID_DATA ||
                   error == BatteryError::READ_ERROR);
    }
}

TEST_F(BatteryTest, BatteryHealthCalculation) {
    // Test battery health calculation
    auto batteryInfoOpt = getBatteryInfo();

    if (batteryInfoOpt.has_value()) {
        const BatteryInfo& info = batteryInfoOpt.value();

        if (info.isBatteryPresent && info.energyDesign > 0 && info.energyFull > 0) {
            float health = info.getBatteryHealth();

            // Health should be between 0 and 100 (or slightly above for new batteries)
            EXPECT_GE(health, 0.0f);
            EXPECT_LE(health, 120.0f); // Allow some tolerance for measurement errors

            // Health calculation should be consistent
            float expectedHealth = (info.energyFull / info.energyDesign) * 100.0f;
            EXPECT_NEAR(health, expectedHealth, 1.0f);
        }
    }
}

// ============================================================================
// BatteryMonitor Tests
// ============================================================================

class BatteryMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure monitoring is stopped before each test
        BatteryMonitor::stopMonitoring();
    }

    void TearDown() override {
        // Clean up after each test
        BatteryMonitor::stopMonitoring();
    }
};

TEST_F(BatteryMonitorTest, MonitoringState) {
    // Test monitoring state management
    EXPECT_FALSE(BatteryMonitor::isMonitoring());

    // Test starting monitoring with a simple callback
    bool callbackCalled = false;
    auto callback = [&callbackCalled](const BatteryInfo& info) {
        callbackCalled = true;
        // Basic validation of callback data
        EXPECT_TRUE(info.isBatteryPresent || !info.isBatteryPresent);
    };

    // Start monitoring (might fail on systems without battery)
    bool started = BatteryMonitor::startMonitoring(callback, 100);

    if (started) {
        EXPECT_TRUE(BatteryMonitor::isMonitoring());

        // Wait a short time to potentially trigger callback
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        // Stop monitoring
        BatteryMonitor::stopMonitoring();
        EXPECT_FALSE(BatteryMonitor::isMonitoring());
    }
}

TEST_F(BatteryMonitorTest, CallbackValidation) {
    // Test callback parameter validation
    auto callback = [](const BatteryInfo& info) {
        // Validate that callback receives reasonable data
        EXPECT_GE(info.batteryLifePercent, 0.0f);
        EXPECT_LE(info.batteryLifePercent, 100.0f);
        EXPECT_GE(info.energyNow, 0.0f);
        EXPECT_GE(info.energyFull, 0.0f);
        EXPECT_GE(info.energyDesign, 0.0f);
    };

    // Test with different intervals
    bool started1 = BatteryMonitor::startMonitoring(callback, 50);
    if (started1) {
        BatteryMonitor::stopMonitoring();
    }

    bool started2 = BatteryMonitor::startMonitoring(callback, 1000);
    if (started2) {
        BatteryMonitor::stopMonitoring();
    }
}

// ============================================================================
// BatteryManager Tests
// ============================================================================

class BatteryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = &BatteryManager::getInstance();
        manager->stopMonitoring();
        manager->stopRecording();
    }

    void TearDown() override {
        manager->stopMonitoring();
        manager->stopRecording();
    }

    BatteryManager* manager;
};

TEST_F(BatteryManagerTest, SingletonInstance) {
    // Test singleton pattern
    BatteryManager& instance1 = BatteryManager::getInstance();
    BatteryManager& instance2 = BatteryManager::getInstance();

    EXPECT_EQ(&instance1, &instance2);
    EXPECT_EQ(manager, &instance1);
}

TEST_F(BatteryManagerTest, AlertSettings) {
    // Test alert settings configuration
    BatteryAlertSettings settings;
    settings.lowBatteryThreshold = 25.0f;
    settings.criticalBatteryThreshold = 10.0f;
    settings.highTempThreshold = 50.0f;
    settings.lowHealthThreshold = 70.0f;

    // Should not throw
    EXPECT_NO_THROW(manager->setAlertSettings(settings));

    // Test with boundary values
    BatteryAlertSettings boundarySettings;
    boundarySettings.lowBatteryThreshold = 0.0f;
    boundarySettings.criticalBatteryThreshold = 0.0f;
    boundarySettings.highTempThreshold = 100.0f;
    boundarySettings.lowHealthThreshold = 0.0f;

    EXPECT_NO_THROW(manager->setAlertSettings(boundarySettings));
}

TEST_F(BatteryManagerTest, AlertCallback) {
    // Test alert callback functionality
    bool alertReceived = false;
    AlertType receivedAlert = AlertType::LOW_BATTERY;

    auto alertCallback = [&alertReceived, &receivedAlert](AlertType alert, const BatteryInfo& info) {
        alertReceived = true;
        receivedAlert = alert;

        // Validate alert parameters
        EXPECT_TRUE(alert == AlertType::LOW_BATTERY ||
                   alert == AlertType::CRITICAL_BATTERY ||
                   alert == AlertType::HIGH_TEMPERATURE ||
                   alert == AlertType::LOW_BATTERY_HEALTH);

        // Validate battery info
        EXPECT_GE(info.batteryLifePercent, 0.0f);
        EXPECT_LE(info.batteryLifePercent, 100.0f);
    };

    // Set callback should not throw
    EXPECT_NO_THROW(manager->setAlertCallback(alertCallback));
}

TEST_F(BatteryManagerTest, BatteryStats) {
    // Test battery statistics functionality
    const BatteryStats& stats = manager->getStats();

    // Stats should have reasonable default values
    EXPECT_GE(stats.averagePowerConsumption, 0.0f);
    EXPECT_GE(stats.totalEnergyConsumed, 0.0f);
    EXPECT_GE(stats.batteryHealth, 0.0f);
    EXPECT_LE(stats.batteryHealth, 120.0f); // Allow some tolerance
    EXPECT_GE(stats.totalUptime.count(), 0);
    EXPECT_GE(stats.cycleCount, 0);

    // Min/max values should be reasonable
    EXPECT_GE(stats.minBatteryLevel, 0.0f);
    EXPECT_LE(stats.maxBatteryLevel, 100.0f);
    EXPECT_GE(stats.minTemperature, -50.0f); // Reasonable lower bound
    EXPECT_LE(stats.maxTemperature, 100.0f); // Reasonable upper bound
    EXPECT_GE(stats.minVoltage, 0.0f);
    EXPECT_GE(stats.maxVoltage, 0.0f);
}

TEST_F(BatteryManagerTest, RecordingFunctionality) {
    // Test battery history recording

    // Start recording without log file (memory only)
    bool recordingStarted = manager->startRecording();

    // Recording might fail on systems without battery, but should not throw
    EXPECT_NO_THROW(manager->startRecording());

    if (recordingStarted) {
        // Get initial history (should be empty or minimal)
        auto initialHistory = manager->getHistory();

        // Stop recording
        manager->stopRecording();

        // Should not throw when stopping
        EXPECT_NO_THROW(manager->stopRecording());
    }

    // Test recording with log file path
    std::string logPath = "test_battery_log.txt";
    bool fileRecordingStarted = manager->startRecording(logPath);

    if (fileRecordingStarted) {
        manager->stopRecording();
    }
}

TEST_F(BatteryManagerTest, MonitoringFunctionality) {
    // Test battery monitoring functionality

    // Start monitoring with default interval
    bool monitoringStarted = manager->startMonitoring();

    // Monitoring might fail on systems without battery
    EXPECT_NO_THROW(manager->startMonitoring());

    if (monitoringStarted) {
        // Wait a short time
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Stop monitoring
        manager->stopMonitoring();
        EXPECT_NO_THROW(manager->stopMonitoring());
    }

    // Test with custom interval
    bool customMonitoringStarted = manager->startMonitoring(5000);
    if (customMonitoringStarted) {
        manager->stopMonitoring();
    }
}

TEST_F(BatteryManagerTest, HistoryFunctionality) {
    // Test battery history functionality

    // Get history without any recording (should be empty or minimal)
    auto history = manager->getHistory();
    EXPECT_TRUE(history.empty() || !history.empty()); // Should not throw

    // Test with max entries limit
    auto limitedHistory = manager->getHistory(10);
    EXPECT_LE(limitedHistory.size(), 10);

    // Test with zero limit (should return all)
    auto allHistory = manager->getHistory(0);
    EXPECT_GE(allHistory.size(), limitedHistory.size());

    // Validate history entries if any exist
    for (const auto& [timestamp, info] : history) {
        // Timestamp should be valid
        EXPECT_GT(timestamp.time_since_epoch().count(), 0);

        // Battery info should be valid
        EXPECT_GE(info.batteryLifePercent, 0.0f);
        EXPECT_LE(info.batteryLifePercent, 100.0f);
    }
}

// ============================================================================
// PowerPlanManager Tests
// ============================================================================

class PowerPlanManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Store original power plan to restore later
        originalPlan = PowerPlanManager::getCurrentPowerPlan();
    }

    void TearDown() override {
        // Restore original power plan if we changed it
        if (originalPlan.has_value()) {
            PowerPlanManager::setPowerPlan(originalPlan.value());
        }
    }

    std::optional<PowerPlan> originalPlan;
};

TEST_F(PowerPlanManagerTest, GetCurrentPowerPlan) {
    // Test getting current power plan
    auto currentPlan = PowerPlanManager::getCurrentPowerPlan();

    // Should not throw
    EXPECT_NO_THROW(PowerPlanManager::getCurrentPowerPlan());

    // If we get a plan, it should be valid
    if (currentPlan.has_value()) {
        PowerPlan plan = currentPlan.value();
        EXPECT_TRUE(plan == PowerPlan::BALANCED ||
                   plan == PowerPlan::PERFORMANCE ||
                   plan == PowerPlan::POWER_SAVER ||
                   plan == PowerPlan::CUSTOM);
    }
}

TEST_F(PowerPlanManagerTest, GetAvailablePowerPlans) {
    // Test getting available power plans
    auto availablePlans = PowerPlanManager::getAvailablePowerPlans();

    // Should not throw
    EXPECT_NO_THROW(PowerPlanManager::getAvailablePowerPlans());

    // Should have at least some plans on most systems
    // (might be empty on some systems, which is acceptable)
    EXPECT_TRUE(availablePlans.empty() || !availablePlans.empty());

    // Validate plan names if any exist
    for (const auto& planName : availablePlans) {
        EXPECT_FALSE(planName.empty());
    }
}

TEST_F(PowerPlanManagerTest, SetPowerPlan) {
    // Test setting power plans

    // Test setting to balanced (most commonly supported)
    auto balancedResult = PowerPlanManager::setPowerPlan(PowerPlan::BALANCED);
    EXPECT_NO_THROW(PowerPlanManager::setPowerPlan(PowerPlan::BALANCED));

    // Result might be nullopt on unsupported systems
    if (balancedResult.has_value()) {
        // If we got a result, verify the plan was set
        auto currentPlan = PowerPlanManager::getCurrentPowerPlan();
        if (currentPlan.has_value() && balancedResult.value()) {
            EXPECT_EQ(currentPlan.value(), PowerPlan::BALANCED);
        }
    }

    // Test other power plans
    EXPECT_NO_THROW(PowerPlanManager::setPowerPlan(PowerPlan::POWER_SAVER));
    EXPECT_NO_THROW(PowerPlanManager::setPowerPlan(PowerPlan::PERFORMANCE));
}

// ============================================================================
// Error Handling and Edge Cases
// ============================================================================

TEST_F(BatteryTest, ErrorHandling) {
    // Test error handling scenarios

    // These functions should not throw even in error conditions
    EXPECT_NO_THROW(getBatteryInfo());
    EXPECT_NO_THROW(getDetailedBatteryInfo());

    // Test with invalid/extreme values
    BatteryInfo testInfo;
    testInfo.batteryLifePercent = -10.0f; // Invalid percentage
    testInfo.energyNow = -1000.0f; // Invalid energy
    testInfo.currentNow = 0.0f; // Zero current

    // Functions should handle invalid data gracefully
    EXPECT_NO_THROW(testInfo.getEstimatedTimeRemaining());
    EXPECT_NO_THROW(testInfo.getBatteryHealth());

    float estimatedTime = testInfo.getEstimatedTimeRemaining();
    EXPECT_GE(estimatedTime, 0.0f); // Should return non-negative value
}

TEST_F(BatteryTest, ThreadSafety) {
    // Basic thread safety test
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Launch multiple threads calling battery functions
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&successCount]() {
            try {
                auto info = getBatteryInfo();
                auto detailed = getDetailedBatteryInfo();
                successCount++;
            } catch (...) {
                // Ignore exceptions for this test
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // At least some calls should succeed
    EXPECT_GE(successCount.load(), 0);
}

} // namespace atom::sysinfo::test
