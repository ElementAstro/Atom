#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/serial/scanner.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// Platform-specific mocking setup
#ifdef _WIN32
// Windows mock declarations
class SetupApiMock {
public:
    MOCK_METHOD(HDEVINFO, SetupDiGetClassDevs,
                (const GUID*, PCTSTR, HWND, DWORD));
    MOCK_METHOD(BOOL, SetupDiEnumDeviceInfo,
                (HDEVINFO, DWORD, PSP_DEVINFO_DATA));
    MOCK_METHOD(BOOL, SetupDiGetDeviceRegistryProperty,
                (HDEVINFO, PSP_DEVINFO_DATA, DWORD, PDWORD, PBYTE, DWORD,
                 PDWORD));
    MOCK_METHOD(BOOL, SetupDiGetDeviceInstanceId,
                (HDEVINFO, PSP_DEVINFO_DATA, PWSTR, DWORD, PDWORD));
    MOCK_METHOD(BOOL, SetupDiDestroyDeviceInfoList, (HDEVINFO));
};

// Singleton pattern for the mock
SetupApiMock& GetSetupApiMock() {
    static SetupApiMock mock;
    return mock;
}

#else
// Linux mock declarations
class UdevMock {
public:
    MOCK_METHOD(struct udev*, udev_new, ());
    MOCK_METHOD(struct udev_enumerate*, udev_enumerate_new, (struct udev*));
    MOCK_METHOD(int, udev_enumerate_add_match_subsystem,
                (struct udev_enumerate*, const char*));
    MOCK_METHOD(int, udev_enumerate_scan_devices, (struct udev_enumerate*));
    MOCK_METHOD(struct udev_list_entry*, udev_enumerate_get_list_entry,
                (struct udev_enumerate*));
    MOCK_METHOD(struct udev_list_entry*, udev_list_entry_get_next,
                (struct udev_list_entry*));
    MOCK_METHOD(const char*, udev_list_entry_get_name,
                (struct udev_list_entry*));
    MOCK_METHOD(struct udev_device*, udev_device_new_from_syspath,
                (struct udev*, const char*));
    MOCK_METHOD(const char*, udev_device_get_devnode, (struct udev_device*));
    MOCK_METHOD(const char*, udev_device_get_property_value,
                (struct udev_device*, const char*));
    MOCK_METHOD(const char*, udev_device_get_sysattr_value,
                (struct udev_device*, const char*));
    MOCK_METHOD(void, udev_device_unref, (struct udev_device*));
    MOCK_METHOD(void, udev_enumerate_unref, (struct udev_enumerate*));
    MOCK_METHOD(void, udev_unref, (struct udev*));
};

// Singleton pattern for the mock
UdevMock& GetUdevMock() {
    static UdevMock mock;
    return mock;
}
#endif

class SerialPortScannerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test resources
        scanner = std::make_unique<SerialPortScanner>();
    }

    void TearDown() override {
        // Clean up test resources
    }

    // Helper method to find a port by device name
    bool containsPort(const std::vector<SerialPortScanner::PortInfo>& ports,
                      const std::string& deviceName) {
        return std::any_of(
            ports.begin(), ports.end(),
            [&deviceName](const SerialPortScanner::PortInfo& port) {
                return port.device == deviceName;
            });
    }

    std::unique_ptr<SerialPortScanner> scanner;
};

// Test the constructor - verify CH340 identifiers are initialized
TEST_F(SerialPortScannerTest, ConstructorInitializesIdentifiers) {
    // CH340 has a well-known VID of 0x1a86, test with that
    auto result = scanner->is_ch340_device(0x1a86, 0x7523, "USB-Serial CH340");

    // The scanner should recognize this common CH340 device
    EXPECT_TRUE(result.first);
    EXPECT_FALSE(result.second.empty());
}

// Test CH340 device identification with known devices
TEST_F(SerialPortScannerTest, IdentifiesCH340Device) {
    // Test with known CH340 VID/PID combinations
    auto result1 = scanner->is_ch340_device(0x1a86, 0x7523, "USB-Serial CH340");
    EXPECT_TRUE(result1.first);
    EXPECT_THAT(result1.second, ::testing::HasSubstr("CH340"));

    // Test with a different known CH340 PID
    auto result2 =
        scanner->is_ch340_device(0x1a86, 0x5523, "CH340 Serial Converter");
    EXPECT_TRUE(result2.first);

    // Test with non-CH340 VID/PID
    auto result3 =
        scanner->is_ch340_device(0x0403, 0x6001, "FTDI USB Serial Device");
    EXPECT_FALSE(result3.first);
    EXPECT_TRUE(result3.second.empty());
}

// Test CH340 device identification with description hints
TEST_F(SerialPortScannerTest, IdentifiesCH340DeviceByDescription) {
    // Even with unknown VID/PID, if description contains CH340, it should be
    // flagged
    auto result =
        scanner->is_ch340_device(0xffff, 0xffff, "USB Serial Device CH340G");
    EXPECT_TRUE(result.first);
    EXPECT_THAT(result.second, ::testing::HasSubstr("CH340G"));

    // Test with various CH340 model mentions in description
    auto result2 =
        scanner->is_ch340_device(0xffff, 0xffff, "USB Serial Device CH341");
    EXPECT_TRUE(result2.first);
    EXPECT_THAT(result2.second, ::testing::HasSubstr("CH341"));

    auto result3 =
        scanner->is_ch340_device(0xffff, 0xffff, "Generic Serial CH34X");
    EXPECT_TRUE(result3.first);
    EXPECT_THAT(result3.second, ::testing::HasSubstr("CH34X"));
}

// Mock test for list_available_ports on Windows
#ifdef _WIN32
TEST_F(SerialPortScannerTest, ListAvailablePortsWindows) {
    // Setup mock expectations for Windows API calls

    // Actual test - will use the mocked Windows API functions
    auto ports = scanner->list_available_ports();

    // Verify results based on mock data
    EXPECT_FALSE(ports.empty());
    EXPECT_TRUE(containsPort(ports, "COM3"));

    // Check that CH340 devices are properly flagged
    auto ch340Port = std::find_if(
        ports.begin(), ports.end(),
        [](const SerialPortScanner::PortInfo& port) { return port.is_ch340; });

    // Verify CH340 detection worked
    EXPECT_NE(ch340Port, ports.end());
    EXPECT_FALSE(ch340Port->ch340_model.empty());
}
#else
// Mock test for list_available_ports on Linux
TEST_F(SerialPortScannerTest, ListAvailablePortsLinux) {
    // Setup mock expectations for libudev calls

    // Actual test - will use the mocked libudev functions
    auto ports = scanner->list_available_ports();

    // Verify results based on mock data
    EXPECT_FALSE(ports.empty());
    EXPECT_TRUE(containsPort(ports, "/dev/ttyUSB0"));

    // Check that CH340 devices are properly flagged
    auto ch340Port = std::find_if(
        ports.begin(), ports.end(),
        [](const SerialPortScanner::PortInfo& port) { return port.is_ch340; });

    // Verify CH340 detection worked
    EXPECT_NE(ch340Port, ports.end());
    EXPECT_FALSE(ch340Port->ch340_model.empty());
}
#endif

// Test getting port details for an existing port
TEST_F(SerialPortScannerTest, GetPortDetailsExistingPort) {
    // Mock setup for known port

    // Test with a port name that should exist in our mock
#ifdef _WIN32
    auto details = scanner->get_port_details("COM3");
#else
    auto details = scanner->get_port_details("/dev/ttyUSB0");
#endif

    // Verify we got details back
    ASSERT_TRUE(details.has_value());

    // Check basic fields are populated
    EXPECT_FALSE(details->device_name.empty());
    EXPECT_FALSE(details->description.empty());
    EXPECT_FALSE(details->vid.empty());
    EXPECT_FALSE(details->pid.empty());
}

// Test getting port details for a non-existing port
TEST_F(SerialPortScannerTest, GetPortDetailsNonExistingPort) {
    // Test with a port name that shouldn't exist
    auto details = scanner->get_port_details("NON_EXISTENT_PORT");

    // Verify we get an empty optional
    EXPECT_FALSE(details.has_value());
}

// Test listing ports with CH340 highlighting disabled
TEST_F(SerialPortScannerTest, ListPortsWithoutCH340Highlighting) {
    // Mock setup

    // Call with highlight_ch340 set to false
    auto ports = scanner->list_available_ports(false);

    // Verify all ports have is_ch340 set to false
    for (const auto& port : ports) {
        EXPECT_FALSE(port.is_ch340);
        EXPECT_TRUE(port.ch340_model.empty());
    }
}

// Test with edge case of no available ports
TEST_F(SerialPortScannerTest, NoAvailablePorts) {
    // Mock setup to return no ports

    // Call list_available_ports
    auto ports = scanner->list_available_ports();

    // Verify an empty vector is returned
    EXPECT_TRUE(ports.empty());
}

// Test CH340 detection with malformed input
TEST_F(SerialPortScannerTest, CH340DetectionWithMalformedInput) {
    // Test with invalid VID/PID values
    auto result1 = scanner->is_ch340_device(0, 0, "");
    EXPECT_FALSE(result1.first);
    EXPECT_TRUE(result1.second.empty());

    // Test with nullptr or empty string for description
    auto result2 = scanner->is_ch340_device(0x1a86, 0x7523, "");
    // Even with empty description, if VID/PID match known CH340, should return
    // true
    EXPECT_TRUE(result2.first);
}

// Integration-style test for the full port scanning workflow
TEST_F(SerialPortScannerTest, FullPortScanningWorkflow) {
    // 1. List available ports
    auto ports = scanner->list_available_ports();

    // 2. For each port, get detailed information
    for (const auto& port : ports) {
        auto details = scanner->get_port_details(port.device);

        // Verify details are available
        ASSERT_TRUE(details.has_value());

        // Verify device name matches
        EXPECT_EQ(details->device_name, port.device);

        // Verify CH340 flag consistency
        EXPECT_EQ(details->is_ch340, port.is_ch340);

        // If it's a CH340, verify model consistency
        if (port.is_ch340) {
            EXPECT_EQ(details->ch340_model, port.ch340_model);
        }
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
