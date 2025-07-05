// filepath: d:\msys64\home\qwdma\Atom\atom\system\test_voltage.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "atom/system/voltage.hpp"

#ifdef _WIN32
#include "atom/system/voltage_windows.hpp"
#elif defined(__linux__)
#include "atom/system/voltage_linux.hpp"
#endif

using namespace atom::system;
using namespace std::chrono_literals;

// Mock VoltageMonitor for testing
class MockVoltageMonitor : public VoltageMonitor {
public:
    MOCK_METHOD(std::optional<double>, getInputVoltage, (), (const, override));
    MOCK_METHOD(std::optional<double>, getBatteryVoltage, (), (const, override));
    MOCK_METHOD(std::vector<PowerSourceInfo>, getAllPowerSources, (), (const, override));
    MOCK_METHOD(std::string, getPlatformName, (), (const, override));
};

class VoltageMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a real voltage monitor
        realMonitor = VoltageMonitor::create();

        // Create a mock voltage monitor for controlled tests
        mockMonitor = std::make_unique<::testing::NiceMock<MockVoltageMonitor>>();

        // Set up default behavior for the mock
        ON_CALL(*mockMonitor, getPlatformName())
            .WillByDefault(::testing::Return("MockPlatform"));

        ON_CALL(*mockMonitor, getInputVoltage())
            .WillByDefault(::testing::Return(std::optional<double>(220.0)));

        ON_CALL(*mockMonitor, getBatteryVoltage())
            .WillByDefault(::testing::Return(std::optional<double>(12.0)));

        ON_CALL(*mockMonitor, getAllPowerSources())
            .WillByDefault(::testing::Return(createSamplePowerSources()));
    }

    void TearDown() override {
        realMonitor.reset();
        mockMonitor.reset();
    }

    // Helper method to create sample power sources for testing
    std::vector<PowerSourceInfo> createSamplePowerSources() {
        std::vector<PowerSourceInfo> sources;

        // Create an AC power source
        PowerSourceInfo acSource;
        acSource.name = "Test AC Adapter";
        acSource.type = PowerSourceType::AC;
        acSource.voltage = 220.0;
        acSource.current = 1.5;
        sources.push_back(acSource);

        // Create a battery power source
        PowerSourceInfo batterySource;
        batterySource.name = "Test Battery";
        batterySource.type = PowerSourceType::Battery;
        batterySource.voltage = 12.0;
        batterySource.current = 0.8;
        batterySource.chargePercent = 75;
        batterySource.isCharging = true;
        sources.push_back(batterySource);

        // Create a USB power source
        PowerSourceInfo usbSource;
        usbSource.name = "Test USB";
        usbSource.type = PowerSourceType::USB;
        usbSource.voltage = 5.0;
        usbSource.current = 0.5;
        sources.push_back(usbSource);

        return sources;
    }

    std::unique_ptr<VoltageMonitor> realMonitor;
    std::unique_ptr<MockVoltageMonitor> mockMonitor;
};

// Test creation of a VoltageMonitor instance
TEST_F(VoltageMonitorTest, Create) {
    auto monitor = VoltageMonitor::create();
    ASSERT_NE(monitor, nullptr);

    // Check that the platform name is not empty
    EXPECT_FALSE(monitor->getPlatformName().empty());

    // Platform name should be Windows, Linux, or MacOS
    std::string platform = monitor->getPlatformName();
    bool validPlatform = (platform == "Windows" ||
                         platform == "Linux" ||
                         platform == "MacOS");
    EXPECT_TRUE(validPlatform);
}

// Test PowerSourceInfo::toString() method
TEST_F(VoltageMonitorTest, PowerSourceInfoToString) {
    // Create a complete power source info
    PowerSourceInfo info;
    info.name = "Test Source";
    info.type = PowerSourceType::Battery;
    info.voltage = 12.5;
    info.current = 1.2;
    info.chargePercent = 80;
    info.isCharging = true;

    std::string infoStr = info.toString();

    // Verify all information is included in the string
    EXPECT_TRUE(infoStr.find("Test Source") != std::string::npos);
    EXPECT_TRUE(infoStr.find("Battery") != std::string::npos);
    EXPECT_TRUE(infoStr.find("12.50V") != std::string::npos);
    EXPECT_TRUE(infoStr.find("1.20A") != std::string::npos);
    EXPECT_TRUE(infoStr.find("80%") != std::string::npos);
    EXPECT_TRUE(infoStr.find("Charging") != std::string::npos);

    // Now test with some fields missing
    PowerSourceInfo partialInfo;
    partialInfo.name = "Partial Info";
    partialInfo.type = PowerSourceType::AC;
    // Missing voltage, current, etc.

    std::string partialStr = partialInfo.toString();
    EXPECT_TRUE(partialStr.find("Partial Info") != std::string::npos);
    EXPECT_TRUE(partialStr.find("AC Power") != std::string::npos);
    EXPECT_FALSE(partialStr.find("V") != std::string::npos); // No voltage
}

// Test powerSourceTypeToString function
TEST_F(VoltageMonitorTest, PowerSourceTypeToString) {
    EXPECT_EQ(powerSourceTypeToString(PowerSourceType::AC), "AC Power");
    EXPECT_EQ(powerSourceTypeToString(PowerSourceType::Battery), "Battery");
    EXPECT_EQ(powerSourceTypeToString(PowerSourceType::USB), "USB");
    EXPECT_EQ(powerSourceTypeToString(PowerSourceType::Unknown), "Unknown");

    // Test with explicit cast to test default case
    EXPECT_EQ(powerSourceTypeToString(static_cast<PowerSourceType>(999)), "Undefined");
}

// Test getInputVoltage method
TEST_F(VoltageMonitorTest, GetInputVoltage) {
    // Using the mock monitor for deterministic testing
    auto voltage = mockMonitor->getInputVoltage();
    ASSERT_TRUE(voltage.has_value());
    EXPECT_EQ(*voltage, 220.0);

    // Test with the real monitor
    // Note: This might return nullopt if no AC power is connected
    auto realVoltage = realMonitor->getInputVoltage();
    if (realVoltage) {
        // If a value is returned, it should be positive
        EXPECT_GT(*realVoltage, 0.0);
        // And reasonable (most AC power is between 100V and 250V)
        EXPECT_GE(*realVoltage, 100.0);
        EXPECT_LE(*realVoltage, 250.0);
    }
}

// Test getBatteryVoltage method
TEST_F(VoltageMonitorTest, GetBatteryVoltage) {
    // Using the mock monitor for deterministic testing
    auto voltage = mockMonitor->getBatteryVoltage();
    ASSERT_TRUE(voltage.has_value());
    EXPECT_EQ(*voltage, 12.0);

    // Test with the real monitor
    // Note: This might return nullopt if no battery is present
    auto realVoltage = realMonitor->getBatteryVoltage();
    if (realVoltage) {
        // If a value is returned, it should be positive
        EXPECT_GT(*realVoltage, 0.0);
        // Battery voltage is typically between 3V and 24V
        EXPECT_GE(*realVoltage, 3.0);
        EXPECT_LE(*realVoltage, 24.0);
    }
}

// Test getAllPowerSources method
TEST_F(VoltageMonitorTest, GetAllPowerSources) {
    // Using the mock monitor for deterministic testing
    auto sources = mockMonitor->getAllPowerSources();
    ASSERT_EQ(sources.size(), 3);

    // Check first source (AC)
    EXPECT_EQ(sources[0].name, "Test AC Adapter");
    EXPECT_EQ(sources[0].type, PowerSourceType::AC);
    ASSERT_TRUE(sources[0].voltage.has_value());
    EXPECT_EQ(*sources[0].voltage, 220.0);

    // Check second source (Battery)
    EXPECT_EQ(sources[1].name, "Test Battery");
    EXPECT_EQ(sources[1].type, PowerSourceType::Battery);
    ASSERT_TRUE(sources[1].voltage.has_value());
    EXPECT_EQ(*sources[1].voltage, 12.0);
    ASSERT_TRUE(sources[1].chargePercent.has_value());
    EXPECT_EQ(*sources[1].chargePercent, 75);
    ASSERT_TRUE(sources[1].isCharging.has_value());
    EXPECT_TRUE(*sources[1].isCharging);

    // Check third source (USB)
    EXPECT_EQ(sources[2].name, "Test USB");
    EXPECT_EQ(sources[2].type, PowerSourceType::USB);
    ASSERT_TRUE(sources[2].voltage.has_value());
    EXPECT_EQ(*sources[2].voltage, 5.0);

    // Test with the real monitor
    auto realSources = realMonitor->getAllPowerSources();
    // We don't know how many power sources are available on the test system
    // Just check that each source has at least a name and type
    for (const auto& source : realSources) {
        EXPECT_FALSE(source.name.empty());
        // Type should be a valid enumeration value
        EXPECT_TRUE(source.type == PowerSourceType::AC ||
                   source.type == PowerSourceType::Battery ||
                   source.type == PowerSourceType::USB ||
                   source.type == PowerSourceType::Unknown);
    }
}

// Test getPlatformName method
TEST_F(VoltageMonitorTest, GetPlatformName) {
    // Using the mock monitor for deterministic testing
    EXPECT_EQ(mockMonitor->getPlatformName(), "MockPlatform");

    // Test with the real monitor
    std::string platform = realMonitor->getPlatformName();
    EXPECT_FALSE(platform.empty());

    // Platform name should match the current platform
#ifdef _WIN32
    EXPECT_EQ(platform, "Windows");
#elif defined(__linux__)
    EXPECT_EQ(platform, "Linux");
#elif defined(__APPLE__)
    EXPECT_EQ(platform, "MacOS");
#endif
}

// Test when getInputVoltage returns nullopt
TEST_F(VoltageMonitorTest, GetInputVoltageNullopt) {
    // Make the mock return nullopt
    EXPECT_CALL(*mockMonitor, getInputVoltage())
        .WillOnce(::testing::Return(std::nullopt));

    auto voltage = mockMonitor->getInputVoltage();
    EXPECT_FALSE(voltage.has_value());
}

// Test when getBatteryVoltage returns nullopt
TEST_F(VoltageMonitorTest, GetBatteryVoltageNullopt) {
    // Make the mock return nullopt
    EXPECT_CALL(*mockMonitor, getBatteryVoltage())
        .WillOnce(::testing::Return(std::nullopt));

    auto voltage = mockMonitor->getBatteryVoltage();
    EXPECT_FALSE(voltage.has_value());
}

// Test when getAllPowerSources returns an empty list
TEST_F(VoltageMonitorTest, GetAllPowerSourcesEmpty) {
    // Make the mock return an empty list
    EXPECT_CALL(*mockMonitor, getAllPowerSources())
        .WillOnce(::testing::Return(std::vector<PowerSourceInfo>()));

    auto sources = mockMonitor->getAllPowerSources();
    EXPECT_TRUE(sources.empty());
}

// Platform-specific tests

#ifdef _WIN32
// Windows-specific tests
TEST_F(VoltageMonitorTest, WindowsSpecificTests) {
    // Check that our real monitor is a WindowsVoltageMonitor
    EXPECT_EQ(typeid(*realMonitor).name(), typeid(WindowsVoltageMonitor).name());

    // Test that platform name is correctly reported
    EXPECT_EQ(realMonitor->getPlatformName(), "Windows");

    // Additional Windows-specific tests could go here
}
#elif defined(__linux__)
// Linux-specific tests
TEST_F(VoltageMonitorTest, LinuxSpecificTests) {
    // Check that our real monitor is a LinuxVoltageMonitor
    EXPECT_EQ(typeid(*realMonitor).name(), typeid(LinuxVoltageMonitor).name());

    // Test that platform name is correctly reported
    EXPECT_EQ(realMonitor->getPlatformName(), "Linux");

    // Test LinuxVoltageMonitor specific methods
    auto* linuxMonitor = dynamic_cast<LinuxVoltageMonitor*>(realMonitor.get());
    ASSERT_NE(linuxMonitor, nullptr);

    // Test conversion methods
    EXPECT_NEAR(LinuxVoltageMonitor::microvoltsToVolts("1000000"), 1.0, 0.001);
    EXPECT_NEAR(LinuxVoltageMonitor::microampsToAmps("1000000"), 1.0, 0.001);

    // Invalid input should return 0
    EXPECT_EQ(LinuxVoltageMonitor::microvoltsToVolts("invalid"), 0.0);
    EXPECT_EQ(LinuxVoltageMonitor::microampsToAmps("invalid"), 0.0);
}
#endif

// Test edge cases and error handling

// Test with invalid power source types
TEST_F(VoltageMonitorTest, InvalidPowerSourceType) {
    PowerSourceInfo info;
    info.name = "Invalid Type Test";
    // Set an invalid type using a cast
    info.type = static_cast<PowerSourceType>(999);

    std::string infoStr = info.toString();
    EXPECT_TRUE(infoStr.find("Undefined") != std::string::npos);
}

// Test with extreme values
TEST_F(VoltageMonitorTest, ExtremeValues) {
    PowerSourceInfo info;
    info.name = "Extreme Values Test";
    info.type = PowerSourceType::Battery;

    // Very high voltage
    info.voltage = 1000000.0;
    // Very high current
    info.current = 1000000.0;
    // 100% charge
    info.chargePercent = 100;

    std::string infoStr = info.toString();
    EXPECT_TRUE(infoStr.find("1000000.00V") != std::string::npos);
    EXPECT_TRUE(infoStr.find("1000000.00A") != std::string::npos);
    EXPECT_TRUE(infoStr.find("100%") != std::string::npos);
}

// Test PowerSourceInfo with negative values (shouldn't happen in reality)
TEST_F(VoltageMonitorTest, NegativeValues) {
    PowerSourceInfo info;
    info.name = "Negative Values Test";
    info.type = PowerSourceType::Battery;

    // Negative voltage (shouldn't happen in reality)
    info.voltage = -12.0;
    // Negative current (could indicate discharge)
    info.current = -1.5;
    // Negative charge percentage (shouldn't happen in reality)
    info.chargePercent = -10;

    std::string infoStr = info.toString();
    EXPECT_TRUE(infoStr.find("-12.00V") != std::string::npos);
    EXPECT_TRUE(infoStr.find("-1.50A") != std::string::npos);
    EXPECT_TRUE(infoStr.find("-10%") != std::string::npos);
}

// Integration test - test all methods in sequence
TEST_F(VoltageMonitorTest, IntegrationTest) {
    // Create a new monitor
    auto monitor = VoltageMonitor::create();

    // Test platform name
    std::string platform = monitor->getPlatformName();
    EXPECT_FALSE(platform.empty());

    // Get input voltage
    auto inputVoltage = monitor->getInputVoltage();
    // Don't assert on value, just that it works

    // Get battery voltage
    auto batteryVoltage = monitor->getBatteryVoltage();
    // Don't assert on value, just that it works

    // Get all power sources
    auto sources = monitor->getAllPowerSources();

    // Print all source information using toString
    for (const auto& source : sources) {
        std::string sourceInfo = source.toString();
        EXPECT_FALSE(sourceInfo.empty());
    }
}

// Performance test (disabled by default)
TEST_F(VoltageMonitorTest, DISABLED_PerformanceTest) {
    // Time how long it takes to get power source information
    const int iterations = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        auto sources = realMonitor->getAllPowerSources();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Average time to get all power sources: "
              << (duration / static_cast<double>(iterations))
              << " ms" << std::endl;

    // No specific assertion, but it shouldn't take too long
}

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
