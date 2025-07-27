/**
 * @file simple_test.cpp
 * @brief Simple battery module tests
 * 
 * Basic tests for the battery module functionality without external testing frameworks.
 */

#include "atom/sysinfo/battery/battery.hpp"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

using namespace atom::system::battery;

// Test counter
int g_testsPassed = 0;
int g_testsTotal = 0;

#define TEST(name) \
    do { \
        std::cout << "Running test: " << name << "... "; \
        g_testsTotal++; \
        bool testResult = true; \
        try {

#define EXPECT_TRUE(condition) \
    if (!(condition)) { \
        std::cout << "FAILED - Expected true: " << #condition << std::endl; \
        testResult = false; \
    }

#define EXPECT_FALSE(condition) \
    if (condition) { \
        std::cout << "FAILED - Expected false: " << #condition << std::endl; \
        testResult = false; \
    }

#define EXPECT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        std::cout << "FAILED - Expected: " << (expected) << ", Actual: " << (actual) << std::endl; \
        testResult = false; \
    }

#define END_TEST() \
        } catch (const std::exception& e) { \
            std::cout << "FAILED - Exception: " << e.what() << std::endl; \
            testResult = false; \
        } \
        if (testResult) { \
            std::cout << "PASSED" << std::endl; \
            g_testsPassed++; \
        } \
    } while(0)

void testBatteryInfo() {
    TEST("BatteryInfo basic functionality") {
        BatteryInfo info;
        
        // Test default values
        EXPECT_FALSE(info.isBatteryPresent);
        EXPECT_FALSE(info.isCharging);
        EXPECT_EQ(0.0f, info.batteryLifePercent);
        EXPECT_EQ(0.0f, info.energyNow);
        EXPECT_EQ(0.0f, info.voltageNow);
        EXPECT_EQ(0, info.cycleCounts);
        
        // Test health calculation with no data
        EXPECT_EQ(0.0f, info.getBatteryHealth());
        
        // Test with some data
        info.energyFull = 50.0f;
        info.energyDesign = 60.0f;
        float expectedHealth = (50.0f / 60.0f) * 100.0f;
        EXPECT_EQ(expectedHealth, info.getBatteryHealth());
        
        // Test equality
        BatteryInfo info2 = info;
        EXPECT_TRUE(info == info2);
        
        info2.batteryLifePercent = 50.0f;
        EXPECT_TRUE(info != info2);
    } END_TEST();
}

void testBatteryInfoRetrieval() {
    TEST("Battery info retrieval") {
        // Test basic battery info
        auto basicInfo = getBatteryInfo();
        // Note: This might be null on systems without battery
        if (basicInfo) {
            std::cout << "(Battery detected) ";
            // If we have a battery, test some basic properties
            EXPECT_TRUE(basicInfo->batteryLifePercent >= 0.0f);
            EXPECT_TRUE(basicInfo->batteryLifePercent <= 100.0f);
        } else {
            std::cout << "(No battery detected) ";
        }
        
        // Test detailed battery info
        auto detailedResult = getDetailedBatteryInfo();
        if (auto* detailedInfo = std::get_if<BatteryInfo>(&detailedResult)) {
            std::cout << "(Detailed info available) ";
            EXPECT_TRUE(detailedInfo->batteryLifePercent >= 0.0f);
            EXPECT_TRUE(detailedInfo->batteryLifePercent <= 100.0f);
        } else {
            auto error = std::get<BatteryError>(detailedResult);
            std::cout << "(Detailed info error: " << static_cast<int>(error) << ") ";
        }
    } END_TEST();
}

void testMultiBatteryInfo() {
    TEST("Multi-battery info") {
        auto allBatteries = getAllBatteries();
        
        if (!allBatteries.isEmpty()) {
            std::cout << "(Found " << allBatteries.size() << " batteries) ";
            EXPECT_TRUE(allBatteries.activeBatteryCount >= 0);
            EXPECT_TRUE(allBatteries.totalCapacity >= 0.0f);
            EXPECT_TRUE(allBatteries.totalEnergyRemaining >= 0.0f);
            
            // Test primary battery
            auto* primary = allBatteries.getPrimaryBattery();
            if (primary) {
                EXPECT_TRUE(primary->batteryLifePercent >= 0.0f);
                EXPECT_TRUE(primary->batteryLifePercent <= 100.0f);
            }
        } else {
            std::cout << "(No batteries found) ";
        }
    } END_TEST();
}

void testBatteryMonitor() {
    TEST("Battery monitor") {
        // Test that monitor is not running initially
        EXPECT_FALSE(BatteryMonitor::isMonitoring());
        
        // Test starting monitor (might fail if no battery)
        bool callbackCalled = false;
        auto callback = [&callbackCalled](const BatteryInfo& info) {
            callbackCalled = true;
            std::cout << "(Callback called with " << info.batteryLifePercent << "%) ";
        };
        
        bool started = BatteryMonitor::startMonitoring(callback, 100);  // 100ms interval
        if (started) {
            std::cout << "(Monitor started) ";
            EXPECT_TRUE(BatteryMonitor::isMonitoring());
            
            // Wait a bit to see if callback is called
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            // Stop monitoring
            BatteryMonitor::stopMonitoring();
            EXPECT_FALSE(BatteryMonitor::isMonitoring());
        } else {
            std::cout << "(Monitor failed to start) ";
        }
    } END_TEST();
}

void testPowerPlanManager() {
    TEST("Power plan manager") {
        // Test getting current power plan
        auto currentPlan = PowerPlanManager::getCurrentPowerPlan();
        if (currentPlan) {
            std::cout << "(Current plan: " << static_cast<int>(*currentPlan) << ") ";
        } else {
            std::cout << "(No current plan available) ";
        }
        
        // Test getting available plans
        auto availablePlans = PowerPlanManager::getAvailablePowerPlans();
        EXPECT_TRUE(availablePlans.size() > 0);
        std::cout << "(Available plans: " << availablePlans.size() << ") ";
        
        // Test setting power plan (might fail on some systems)
        auto result = PowerPlanManager::setPowerPlan(PowerPlan::BALANCED);
        if (result) {
            std::cout << "(Set plan result: " << (*result ? "success" : "failed") << ") ";
        } else {
            std::cout << "(Set plan not supported) ";
        }
    } END_TEST();
}

void testBatteryManager() {
    TEST("Battery manager") {
        auto& manager = BatteryManager::getInstance();
        
        // Test alert settings
        BatteryAlertSettings settings;
        settings.lowBatteryThreshold = 25.0f;
        settings.criticalBatteryThreshold = 10.0f;
        manager.setAlertSettings(settings);
        
        // Test alert callback
        bool alertCalled = false;
        manager.setAlertCallback([&alertCalled](AlertType type, const BatteryInfo& info) {
            alertCalled = true;
            std::cout << "(Alert: " << static_cast<int>(type) << ") ";
        });
        
        // Test stats
        const auto& stats = manager.getStats();
        EXPECT_TRUE(stats.minBatteryLevel >= 0.0f);
        EXPECT_TRUE(stats.maxBatteryLevel >= 0.0f);
        
        // Test recording
        bool recordingStarted = manager.startRecording();  // Memory-only
        EXPECT_TRUE(recordingStarted);
        
        manager.stopRecording();
        
        std::cout << "(Manager tests completed) ";
    } END_TEST();
}

void testCalibrator() {
    TEST("Battery calibrator") {
        // Test calibration status
        EXPECT_FALSE(BatteryCalibrator::isCalibrating());
        EXPECT_EQ(0.0f, BatteryCalibrator::getCalibrationProgress());
        
        // Test calibration data
        auto calibData = BatteryCalibrator::getCalibrationData();
        EXPECT_TRUE(calibData.actualCapacity >= 0.0f);
        EXPECT_TRUE(calibData.designCapacity >= 0.0f);
        
        // Test needs calibration check
        bool needsCalib = BatteryCalibrator::needsCalibration();
        std::cout << "(Needs calibration: " << (needsCalib ? "yes" : "no") << ") ";
    } END_TEST();
}

void testThermalManager() {
    TEST("Thermal manager") {
        // Test thermal settings
        ThermalSettings settings;
        settings.warningTemperature = 35.0f;
        settings.criticalTemperature = 45.0f;
        ThermalManager::setThermalSettings(settings);
        
        auto retrievedSettings = ThermalManager::getThermalSettings();
        EXPECT_EQ(35.0f, retrievedSettings.warningTemperature);
        EXPECT_EQ(45.0f, retrievedSettings.criticalTemperature);
        
        // Test thermal monitoring
        ThermalManager::setThermalMonitoring(true);
        EXPECT_TRUE(ThermalManager::isThermalMonitoringEnabled());
        
        ThermalManager::setThermalMonitoring(false);
        EXPECT_FALSE(ThermalManager::isThermalMonitoringEnabled());
        
        // Test temperature reading
        auto temp = ThermalManager::getSystemTemperature();
        if (temp) {
            std::cout << "(System temp: " << *temp << "°C) ";
        } else {
            std::cout << "(No temperature available) ";
        }
    } END_TEST();
}

void testAdaptivePowerManager() {
    TEST("Adaptive power manager") {
        // Test initial state
        EXPECT_FALSE(AdaptivePowerManager::isAdaptivePowerEnabled());
        
        // Test profiles
        auto profiles = AdaptivePowerManager::getAvailableProfiles();
        EXPECT_TRUE(profiles.size() > 0);
        std::cout << "(Available profiles: " << profiles.size() << ") ";
        
        // Test setting profile
        bool profileSet = AdaptivePowerManager::setOptimizationProfile("balanced");
        EXPECT_TRUE(profileSet);
        
        auto currentProfile = AdaptivePowerManager::getCurrentProfile();
        EXPECT_EQ("balanced", currentProfile);
        
        // Test enabling adaptive power (might start background thread)
        bool enabled = AdaptivePowerManager::enableAdaptivePower();
        if (enabled) {
            std::cout << "(Adaptive power enabled) ";
            EXPECT_TRUE(AdaptivePowerManager::isAdaptivePowerEnabled());
            
            // Disable it quickly
            AdaptivePowerManager::disableAdaptivePower();
            EXPECT_FALSE(AdaptivePowerManager::isAdaptivePowerEnabled());
        } else {
            std::cout << "(Adaptive power failed to enable) ";
        }
    } END_TEST();
}

int main() {
    std::cout << "Battery Module Simple Tests" << std::endl;
    std::cout << "============================" << std::endl << std::endl;
    
    // Run all tests
    testBatteryInfo();
    testBatteryInfoRetrieval();
    testMultiBatteryInfo();
    testBatteryMonitor();
    testPowerPlanManager();
    testBatteryManager();
    testCalibrator();
    testThermalManager();
    testAdaptivePowerManager();
    
    // Print results
    std::cout << std::endl;
    std::cout << "Test Results:" << std::endl;
    std::cout << "=============" << std::endl;
    std::cout << "Passed: " << g_testsPassed << "/" << g_testsTotal << std::endl;
    
    if (g_testsPassed == g_testsTotal) {
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
