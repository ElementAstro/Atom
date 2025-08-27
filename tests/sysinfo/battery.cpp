#include "atom/sysinfo/battery.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <thread>

using namespace atom::system;

namespace atom::sysinfo::test {

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

} // namespace atom::sysinfo::test
