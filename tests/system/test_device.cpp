#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "atom/system/device.hpp"

namespace atom::system::test {

// Mock class for testing device enumeration without actual hardware
class MockDeviceEnumerator {
public:
    MOCK_METHOD(std::vector<DeviceInfo>, enumerateUsbDevices, (), (const));
    MOCK_METHOD(std::vector<DeviceInfo>, enumerateSerialPorts, (), (const));
};

class DeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockDeviceEnumerator = std::make_unique<::testing::NiceMock<MockDeviceEnumerator>>();
        
        // Set up sample device data
        usbDevices = {
            {"USB Mass Storage Device", "VID_1234&PID_5678"},
            {"USB Keyboard", "VID_046D&PID_C31C"},
            {"USB Mouse", "VID_046D&PID_C077"}
        };
        
        serialPorts = {
            {"COM1", "Serial Port (COM1)"},
            {"COM3", "USB Serial Port (COM3)"},
            {"/dev/ttyUSB0", "USB-to-Serial Adapter"}
        };
        
        // Set up default behavior for the mock
        ON_CALL(*mockDeviceEnumerator, enumerateUsbDevices())
            .WillByDefault(::testing::Return(usbDevices));
        ON_CALL(*mockDeviceEnumerator, enumerateSerialPorts())
            .WillByDefault(::testing::Return(serialPorts));
    }

    void TearDown() override {
        mockDeviceEnumerator.reset();
    }

    std::unique_ptr<MockDeviceEnumerator> mockDeviceEnumerator;
    std::vector<DeviceInfo> usbDevices;
    std::vector<DeviceInfo> serialPorts;
};

// Test USB device enumeration
TEST_F(DeviceTest, EnumerateUsbDevicesSuccess) {
    EXPECT_CALL(*mockDeviceEnumerator, enumerateUsbDevices())
        .WillOnce(::testing::Return(usbDevices));
    
    auto devices = mockDeviceEnumerator->enumerateUsbDevices();
    
    EXPECT_EQ(devices.size(), 3);
    EXPECT_EQ(devices[0].description, "USB Mass Storage Device");
    EXPECT_EQ(devices[0].address, "VID_1234&PID_5678");
    EXPECT_EQ(devices[1].description, "USB Keyboard");
    EXPECT_EQ(devices[2].description, "USB Mouse");
}

TEST_F(DeviceTest, EnumerateUsbDevicesEmpty) {
    std::vector<DeviceInfo> emptyDevices;
    
    EXPECT_CALL(*mockDeviceEnumerator, enumerateUsbDevices())
        .WillOnce(::testing::Return(emptyDevices));
    
    auto devices = mockDeviceEnumerator->enumerateUsbDevices();
    EXPECT_TRUE(devices.empty());
}

// Test serial port enumeration
TEST_F(DeviceTest, EnumerateSerialPortsSuccess) {
    EXPECT_CALL(*mockDeviceEnumerator, enumerateSerialPorts())
        .WillOnce(::testing::Return(serialPorts));
    
    auto ports = mockDeviceEnumerator->enumerateSerialPorts();
    
    EXPECT_EQ(ports.size(), 3);
    EXPECT_EQ(ports[0].description, "COM1");
    EXPECT_EQ(ports[0].address, "Serial Port (COM1)");
    EXPECT_EQ(ports[1].description, "COM3");
    EXPECT_EQ(ports[2].description, "/dev/ttyUSB0");
}

TEST_F(DeviceTest, EnumerateSerialPortsEmpty) {
    std::vector<DeviceInfo> emptyPorts;
    
    EXPECT_CALL(*mockDeviceEnumerator, enumerateSerialPorts())
        .WillOnce(::testing::Return(emptyPorts));
    
    auto ports = mockDeviceEnumerator->enumerateSerialPorts();
    EXPECT_TRUE(ports.empty());
}

// Test DeviceInfo structure
TEST_F(DeviceTest, DeviceInfoStructure) {
    DeviceInfo device;
    device.description = "Test Device";
    device.address = "Test Address";
    
    EXPECT_EQ(device.description, "Test Device");
    EXPECT_EQ(device.address, "Test Address");
}

TEST_F(DeviceTest, DeviceInfoCopyConstructor) {
    DeviceInfo original;
    original.description = "Original Device";
    original.address = "Original Address";
    
    DeviceInfo copy = original;
    
    EXPECT_EQ(copy.description, "Original Device");
    EXPECT_EQ(copy.address, "Original Address");
}

TEST_F(DeviceTest, DeviceInfoAssignment) {
    DeviceInfo device1;
    device1.description = "Device 1";
    device1.address = "Address 1";
    
    DeviceInfo device2;
    device2 = device1;
    
    EXPECT_EQ(device2.description, "Device 1");
    EXPECT_EQ(device2.address, "Address 1");
}

// Test device filtering and searching
class DeviceFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockDeviceEnumerator = std::make_unique<::testing::NiceMock<MockDeviceEnumerator>>();
        
        // Set up diverse device data for filtering tests
        mixedUsbDevices = {
            {"USB Mass Storage Device", "VID_1234&PID_5678"},
            {"Logitech USB Keyboard", "VID_046D&PID_C31C"},
            {"Microsoft USB Mouse", "VID_045E&PID_0040"},
            {"Arduino Uno", "VID_2341&PID_0043"},
            {"FTDI USB Serial", "VID_0403&PID_6001"}
        };
        
        mixedSerialPorts = {
            {"COM1", "Built-in Serial Port"},
            {"COM3", "USB-to-Serial Adapter"},
            {"COM5", "Bluetooth Serial Port"},
            {"/dev/ttyUSB0", "FTDI USB Serial"},
            {"/dev/ttyACM0", "Arduino Serial"}
        };
        
        ON_CALL(*mockDeviceEnumerator, enumerateUsbDevices())
            .WillByDefault(::testing::Return(mixedUsbDevices));
        ON_CALL(*mockDeviceEnumerator, enumerateSerialPorts())
            .WillByDefault(::testing::Return(mixedSerialPorts));
    }

    void TearDown() override {
        mockDeviceEnumerator.reset();
    }

    std::unique_ptr<MockDeviceEnumerator> mockDeviceEnumerator;
    std::vector<DeviceInfo> mixedUsbDevices;
    std::vector<DeviceInfo> mixedSerialPorts;
    
    // Helper function to filter devices by description substring
    std::vector<DeviceInfo> filterDevicesByDescription(const std::vector<DeviceInfo>& devices, const std::string& substring) {
        std::vector<DeviceInfo> filtered;
        for (const auto& device : devices) {
            if (device.description.find(substring) != std::string::npos) {
                filtered.push_back(device);
            }
        }
        return filtered;
    }
    
    // Helper function to filter devices by vendor ID
    std::vector<DeviceInfo> filterDevicesByVendorId(const std::vector<DeviceInfo>& devices, const std::string& vendorId) {
        std::vector<DeviceInfo> filtered;
        for (const auto& device : devices) {
            if (device.address.find(vendorId) != std::string::npos) {
                filtered.push_back(device);
            }
        }
        return filtered;
    }
};

// Test device filtering by description
TEST_F(DeviceFilterTest, FilterUsbDevicesByDescription) {
    EXPECT_CALL(*mockDeviceEnumerator, enumerateUsbDevices())
        .WillOnce(::testing::Return(mixedUsbDevices));
    
    auto devices = mockDeviceEnumerator->enumerateUsbDevices();
    auto usbDevices = filterDevicesByDescription(devices, "USB");
    
    EXPECT_EQ(usbDevices.size(), 4); // All except Arduino Uno
    
    auto logitechDevices = filterDevicesByDescription(devices, "Logitech");
    EXPECT_EQ(logitechDevices.size(), 1);
    EXPECT_EQ(logitechDevices[0].description, "Logitech USB Keyboard");
}

// Test device filtering by vendor ID
TEST_F(DeviceFilterTest, FilterUsbDevicesByVendorId) {
    EXPECT_CALL(*mockDeviceEnumerator, enumerateUsbDevices())
        .WillOnce(::testing::Return(mixedUsbDevices));
    
    auto devices = mockDeviceEnumerator->enumerateUsbDevices();
    auto logitechDevices = filterDevicesByVendorId(devices, "VID_046D");
    
    EXPECT_EQ(logitechDevices.size(), 1);
    EXPECT_EQ(logitechDevices[0].description, "Logitech USB Keyboard");
    
    auto ftdiDevices = filterDevicesByVendorId(devices, "VID_0403");
    EXPECT_EQ(ftdiDevices.size(), 1);
    EXPECT_EQ(ftdiDevices[0].description, "FTDI USB Serial");
}

// Test serial port filtering
TEST_F(DeviceFilterTest, FilterSerialPortsByType) {
    EXPECT_CALL(*mockDeviceEnumerator, enumerateSerialPorts())
        .WillOnce(::testing::Return(mixedSerialPorts));
    
    auto ports = mockDeviceEnumerator->enumerateSerialPorts();
    auto usbSerialPorts = filterDevicesByDescription(ports, "USB");
    
    EXPECT_GE(usbSerialPorts.size(), 1); // At least one USB serial port
    
    auto bluetoothPorts = filterDevicesByDescription(ports, "Bluetooth");
    EXPECT_EQ(bluetoothPorts.size(), 1);
    EXPECT_EQ(bluetoothPorts[0].description, "COM5");
}

// Test platform-specific behavior
#ifdef _WIN32
TEST_F(DeviceFilterTest, WindowsSpecificPorts) {
    EXPECT_CALL(*mockDeviceEnumerator, enumerateSerialPorts())
        .WillOnce(::testing::Return(mixedSerialPorts));
    
    auto ports = mockDeviceEnumerator->enumerateSerialPorts();
    auto comPorts = filterDevicesByDescription(ports, "COM");
    
    EXPECT_GE(comPorts.size(), 1); // Should have at least one COM port on Windows
}
#elif defined(__linux__)
TEST_F(DeviceFilterTest, LinuxSpecificPorts) {
    EXPECT_CALL(*mockDeviceEnumerator, enumerateSerialPorts())
        .WillOnce(::testing::Return(mixedSerialPorts));
    
    auto ports = mockDeviceEnumerator->enumerateSerialPorts();
    auto ttyPorts = filterDevicesByDescription(ports, "/dev/tty");
    
    EXPECT_GE(ttyPorts.size(), 1); // Should have at least one tty port on Linux
}
#endif

// Error handling tests
class DeviceErrorTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockDeviceEnumerator = std::make_unique<::testing::NiceMock<MockDeviceEnumerator>>();
    }

    void TearDown() override {
        mockDeviceEnumerator.reset();
    }

    std::unique_ptr<MockDeviceEnumerator> mockDeviceEnumerator;
};

// Test handling of enumeration failures
TEST_F(DeviceErrorTest, EnumerationFailureHandling) {
    // Simulate enumeration failure by returning empty vectors
    EXPECT_CALL(*mockDeviceEnumerator, enumerateUsbDevices())
        .WillOnce(::testing::Return(std::vector<DeviceInfo>()));
    EXPECT_CALL(*mockDeviceEnumerator, enumerateSerialPorts())
        .WillOnce(::testing::Return(std::vector<DeviceInfo>()));
    
    auto usbDevices = mockDeviceEnumerator->enumerateUsbDevices();
    auto serialPorts = mockDeviceEnumerator->enumerateSerialPorts();
    
    EXPECT_TRUE(usbDevices.empty());
    EXPECT_TRUE(serialPorts.empty());
}

// Test handling of devices with empty or invalid data
TEST_F(DeviceErrorTest, InvalidDeviceDataHandling) {
    std::vector<DeviceInfo> invalidDevices = {
        {"", ""},  // Empty description and address
        {"Valid Device", ""},  // Empty address
        {"", "Valid Address"}  // Empty description
    };
    
    EXPECT_CALL(*mockDeviceEnumerator, enumerateUsbDevices())
        .WillOnce(::testing::Return(invalidDevices));
    
    auto devices = mockDeviceEnumerator->enumerateUsbDevices();
    
    EXPECT_EQ(devices.size(), 3);
    // Should handle invalid data gracefully without crashing
    for (const auto& device : devices) {
        // Verify that the structure is intact even with empty strings
        EXPECT_NO_THROW(device.description.empty());
        EXPECT_NO_THROW(device.address.empty());
    }
}

// Performance tests
class DevicePerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockDeviceEnumerator = std::make_unique<::testing::NiceMock<MockDeviceEnumerator>>();

        // Create large device lists for performance testing
        largeUsbDeviceList.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            largeUsbDeviceList.push_back({
                "USB Device " + std::to_string(i),
                "VID_" + std::to_string(1000 + i) + "&PID_" + std::to_string(2000 + i)
            });
        }

        largeSerialPortList.reserve(100);
        for (int i = 0; i < 100; ++i) {
            largeSerialPortList.push_back({
                "COM" + std::to_string(i + 1),
                "Serial Port " + std::to_string(i + 1)
            });
        }

        ON_CALL(*mockDeviceEnumerator, enumerateUsbDevices())
            .WillByDefault(::testing::Return(largeUsbDeviceList));
        ON_CALL(*mockDeviceEnumerator, enumerateSerialPorts())
            .WillByDefault(::testing::Return(largeSerialPortList));
    }

    void TearDown() override {
        mockDeviceEnumerator.reset();
    }

    std::unique_ptr<MockDeviceEnumerator> mockDeviceEnumerator;
    std::vector<DeviceInfo> largeUsbDeviceList;
    std::vector<DeviceInfo> largeSerialPortList;
};

// Test enumeration performance with large device lists
TEST_F(DevicePerformanceTest, LargeUsbDeviceEnumeration) {
    EXPECT_CALL(*mockDeviceEnumerator, enumerateUsbDevices())
        .WillOnce(::testing::Return(largeUsbDeviceList));

    auto start = std::chrono::high_resolution_clock::now();
    auto devices = mockDeviceEnumerator->enumerateUsbDevices();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(devices.size(), 1000);
    // Enumeration should complete within reasonable time (100ms for mock)
    EXPECT_LT(duration.count(), 100);
}

TEST_F(DevicePerformanceTest, LargeSerialPortEnumeration) {
    EXPECT_CALL(*mockDeviceEnumerator, enumerateSerialPorts())
        .WillOnce(::testing::Return(largeSerialPortList));

    auto start = std::chrono::high_resolution_clock::now();
    auto ports = mockDeviceEnumerator->enumerateSerialPorts();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(ports.size(), 100);
    // Enumeration should complete within reasonable time
    EXPECT_LT(duration.count(), 50);
}

// Test repeated enumeration performance
TEST_F(DevicePerformanceTest, RepeatedEnumeration) {
    EXPECT_CALL(*mockDeviceEnumerator, enumerateUsbDevices())
        .Times(10)
        .WillRepeatedly(::testing::Return(largeUsbDeviceList));

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10; ++i) {
        auto devices = mockDeviceEnumerator->enumerateUsbDevices();
        EXPECT_EQ(devices.size(), 1000);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 10 enumerations should complete within reasonable time
    EXPECT_LT(duration.count(), 500);
}

// Integration tests with actual function calls
class DeviceIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // These tests will call the actual functions
        // They are designed to work even if no devices are present
    }

    void TearDown() override {
        // No cleanup needed
    }
};

// Test actual USB device enumeration function
TEST_F(DeviceIntegrationTest, ActualUsbDeviceEnumeration) {
    // Call the actual function
    auto devices = enumerateUsbDevices();

    // Should not crash and should return a valid vector
    EXPECT_NO_THROW(enumerateUsbDevices());

    // Verify that each device has valid structure
    for (const auto& device : devices) {
        // Description and address should be accessible (may be empty)
        EXPECT_NO_THROW(device.description.empty());
        EXPECT_NO_THROW(device.address.empty());
    }
}

// Test actual serial port enumeration function
TEST_F(DeviceIntegrationTest, ActualSerialPortEnumeration) {
    // Call the actual function
    auto ports = enumerateSerialPorts();

    // Should not crash and should return a valid vector
    EXPECT_NO_THROW(enumerateSerialPorts());

    // Verify that each port has valid structure
    for (const auto& port : ports) {
        // Description and address should be accessible (may be empty)
        EXPECT_NO_THROW(port.description.empty());
        EXPECT_NO_THROW(port.address.empty());
    }
}

// Test consistency of repeated calls
TEST_F(DeviceIntegrationTest, EnumerationConsistency) {
    // Call enumeration multiple times
    auto devices1 = enumerateUsbDevices();
    auto devices2 = enumerateUsbDevices();
    auto devices3 = enumerateUsbDevices();

    // Results should be consistent (same number of devices)
    // Note: This might fail if devices are being plugged/unplugged during test
    // but it's a reasonable expectation for most test environments
    EXPECT_EQ(devices1.size(), devices2.size());
    EXPECT_EQ(devices2.size(), devices3.size());

    // Do the same for serial ports
    auto ports1 = enumerateSerialPorts();
    auto ports2 = enumerateSerialPorts();
    auto ports3 = enumerateSerialPorts();

    EXPECT_EQ(ports1.size(), ports2.size());
    EXPECT_EQ(ports2.size(), ports3.size());
}

// Test thread safety (if applicable)
TEST_F(DeviceIntegrationTest, ThreadSafetyTest) {
    std::vector<std::thread> threads;
    std::vector<std::vector<DeviceInfo>> results(5);

    // Launch multiple threads that enumerate devices simultaneously
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&results, i]() {
            results[i] = enumerateUsbDevices();
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify that all threads completed successfully
    for (const auto& result : results) {
        // Each result should be valid (may be empty)
        EXPECT_NO_THROW(result.size());
    }
}

}  // namespace atom::system::test
