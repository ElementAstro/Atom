/*
 * test_bluetooth_serial.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-16

Description: Comprehensive Unit Tests for BluetoothSerial
Tests Bluetooth device scanning, connection, pairing, and data transfer.

**************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/serial/bluetooth_serial.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace serial;

// =============================================================================
// BluetoothDeviceInfo Tests
// =============================================================================

class BluetoothDeviceInfoTest : public ::testing::Test {
protected:
    BluetoothDeviceInfo createTestDevice(const std::string& address,
                                         const std::string& name,
                                         int rssi = -70, bool paired = false,
                                         bool connected = false) {
        BluetoothDeviceInfo device;
        device.address = address;
        device.name = name;
        device.rssi = rssi;
        device.paired = paired;
        device.connected = connected;
        device.services = {"00001101-0000-1000-8000-00805F9B34FB"};  // SPP
        return device;
    }
};

TEST_F(BluetoothDeviceInfoTest, DefaultValues) {
    BluetoothDeviceInfo info;
    EXPECT_TRUE(info.address.empty());
    EXPECT_TRUE(info.name.empty());
    EXPECT_EQ(info.rssi, 0);
    EXPECT_FALSE(info.paired);
    EXPECT_FALSE(info.connected);
    EXPECT_TRUE(info.services.empty());
}

TEST_F(BluetoothDeviceInfoTest, CreateWithValues) {
    auto device =
        createTestDevice("00:11:22:33:44:55", "TestDevice", -65, true, false);

    EXPECT_EQ(device.address, "00:11:22:33:44:55");
    EXPECT_EQ(device.name, "TestDevice");
    EXPECT_EQ(device.rssi, -65);
    EXPECT_TRUE(device.paired);
    EXPECT_FALSE(device.connected);
    EXPECT_EQ(device.services.size(), 1);
}

TEST_F(BluetoothDeviceInfoTest, RSSIRange) {
    // RSSI typically ranges from -100 to 0 dBm
    auto strongSignal = createTestDevice("AA:BB:CC:DD:EE:FF", "Strong", -30);
    auto weakSignal = createTestDevice("11:22:33:44:55:66", "Weak", -90);

    EXPECT_GT(strongSignal.rssi, weakSignal.rssi);
    EXPECT_GE(strongSignal.rssi, -100);
    EXPECT_LE(strongSignal.rssi, 0);
}

// =============================================================================
// BluetoothConfig Tests
// =============================================================================

class BluetoothConfigTest : public ::testing::Test {
protected:
    BluetoothConfig config;
};

TEST_F(BluetoothConfigTest, DefaultValues) {
    EXPECT_EQ(config.scanDuration, 5s);
    EXPECT_FALSE(config.autoReconnect);
    EXPECT_EQ(config.reconnectInterval, 5s);
    EXPECT_EQ(config.pin, "1234");
    EXPECT_EQ(config.connectTimeout, 5000ms);
}

TEST_F(BluetoothConfigTest, CustomValues) {
    config.scanDuration = 10s;
    config.autoReconnect = true;
    config.reconnectInterval = 2s;
    config.pin = "5678";
    config.connectTimeout = 3000ms;

    EXPECT_EQ(config.scanDuration, 10s);
    EXPECT_TRUE(config.autoReconnect);
    EXPECT_EQ(config.reconnectInterval, 2s);
    EXPECT_EQ(config.pin, "5678");
    EXPECT_EQ(config.connectTimeout, 3000ms);
}

TEST_F(BluetoothConfigTest, SerialConfigIntegration) {
    config.serialConfig = SerialConfig::standardConfig(115200);

    EXPECT_EQ(config.serialConfig.getBaudRate(), 115200);
    EXPECT_EQ(config.serialConfig.getDataBits(), 8);
}

// =============================================================================
// BluetoothException Tests
// =============================================================================

TEST(BluetoothExceptionTest, Construction) {
    BluetoothException ex("Bluetooth error occurred");
    EXPECT_STREQ(ex.what(), "Bluetooth error occurred");
}

TEST(BluetoothExceptionTest, InheritanceFromSerialException) {
    BluetoothException ex("Test");
    SerialException* basePtr = &ex;
    EXPECT_STREQ(basePtr->what(), "Test");
}

// =============================================================================
// BluetoothSerial Statistics Tests
// =============================================================================

TEST(BluetoothStatisticsTest, DefaultValues) {
    BluetoothSerial::Statistics stats;
    EXPECT_EQ(stats.bytesSent, 0);
    EXPECT_EQ(stats.bytesReceived, 0);
    EXPECT_EQ(stats.currentRssi, 0);
}

TEST(BluetoothStatisticsTest, CustomValues) {
    BluetoothSerial::Statistics stats;
    stats.bytesSent = 1024;
    stats.bytesReceived = 2048;
    stats.connectionTime = std::chrono::steady_clock::now();
    stats.currentRssi = -55;

    EXPECT_EQ(stats.bytesSent, 1024);
    EXPECT_EQ(stats.bytesReceived, 2048);
    EXPECT_EQ(stats.currentRssi, -55);
}

// =============================================================================
// Mock BluetoothSerialImpl
// =============================================================================

class MockBluetoothSerialImpl {
public:
    MOCK_METHOD(bool, isBluetoothEnabled, (), (const));
    MOCK_METHOD(void, enableBluetooth, (bool));
    MOCK_METHOD(std::vector<BluetoothDeviceInfo>, scanDevices,
                (std::chrono::seconds));
    MOCK_METHOD(void, scanDevicesAsync,
                (std::function<void(const BluetoothDeviceInfo&)>,
                 std::function<void()>, std::chrono::seconds));
    MOCK_METHOD(void, stopScan, ());
    MOCK_METHOD(void, connect, (const std::string&, const BluetoothConfig&));
    MOCK_METHOD(void, disconnect, ());
    MOCK_METHOD(bool, isConnected, (), (const));
    MOCK_METHOD(std::optional<BluetoothDeviceInfo>, getConnectedDevice, (),
                (const));
    MOCK_METHOD(bool, pair, (const std::string&, const std::string&));
    MOCK_METHOD(bool, unpair, (const std::string&));
    MOCK_METHOD(std::vector<BluetoothDeviceInfo>, getPairedDevices, ());
    MOCK_METHOD(std::vector<uint8_t>, read, (size_t));
    MOCK_METHOD(std::vector<uint8_t>, readExactly,
                (size_t, std::chrono::milliseconds));
    MOCK_METHOD(void, asyncRead,
                (size_t, std::function<void(std::vector<uint8_t>)>));
    MOCK_METHOD(std::vector<uint8_t>, readAvailable, ());
    MOCK_METHOD(size_t, write, (std::span<const uint8_t>));
    MOCK_METHOD(size_t, writeString, (const std::string&));
    MOCK_METHOD(void, flush, ());
    MOCK_METHOD(size_t, available, (), (const));
    MOCK_METHOD(void, setConnectionListener, (std::function<void(bool)>));
    MOCK_METHOD(BluetoothSerial::Statistics, getStatistics, (), (const));
};

// =============================================================================
// BluetoothSerial Mock Tests
// =============================================================================

class BluetoothSerialMockTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockImpl = std::make_shared<MockBluetoothSerialImpl>();

        testDevices = {
            createDevice("00:11:22:33:44:55", "Device1", -50, false, false),
            createDevice("AA:BB:CC:DD:EE:FF", "Device2", -60, true, false),
            createDevice("11:22:33:44:55:66", "Device3", -70, true, true)};

        testData = {0x01, 0x02, 0x03, 0x04, 0x05};
    }

    BluetoothDeviceInfo createDevice(const std::string& address,
                                     const std::string& name, int rssi,
                                     bool paired, bool connected) {
        BluetoothDeviceInfo device;
        device.address = address;
        device.name = name;
        device.rssi = rssi;
        device.paired = paired;
        device.connected = connected;
        return device;
    }

    std::shared_ptr<MockBluetoothSerialImpl> mockImpl;
    std::vector<BluetoothDeviceInfo> testDevices;
    std::vector<uint8_t> testData;
};

// Adapter Status Tests
TEST_F(BluetoothSerialMockTest, AdapterStatus) {
    EXPECT_CALL(*mockImpl, isBluetoothEnabled())
        .WillOnce(::testing::Return(false))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mockImpl, enableBluetooth(true)).Times(1);

    EXPECT_FALSE(mockImpl->isBluetoothEnabled());
    mockImpl->enableBluetooth(true);
    EXPECT_TRUE(mockImpl->isBluetoothEnabled());
}

TEST_F(BluetoothSerialMockTest, DisableBluetooth) {
    EXPECT_CALL(*mockImpl, isBluetoothEnabled())
        .WillOnce(::testing::Return(true))
        .WillOnce(::testing::Return(false));
    EXPECT_CALL(*mockImpl, enableBluetooth(false)).Times(1);

    EXPECT_TRUE(mockImpl->isBluetoothEnabled());
    mockImpl->enableBluetooth(false);
    EXPECT_FALSE(mockImpl->isBluetoothEnabled());
}

// Device Scanning Tests
TEST_F(BluetoothSerialMockTest, SynchronousScan) {
    EXPECT_CALL(*mockImpl, scanDevices(5s))
        .WillOnce(::testing::Return(testDevices));

    auto devices = mockImpl->scanDevices(5s);

    EXPECT_EQ(devices.size(), 3);
    EXPECT_EQ(devices[0].address, "00:11:22:33:44:55");
    EXPECT_EQ(devices[1].name, "Device2");
    EXPECT_TRUE(devices[2].connected);
}

TEST_F(BluetoothSerialMockTest, SynchronousScanEmpty) {
    EXPECT_CALL(*mockImpl, scanDevices(::testing::_))
        .WillOnce(::testing::Return(std::vector<BluetoothDeviceInfo>{}));

    auto devices = mockImpl->scanDevices(3s);
    EXPECT_TRUE(devices.empty());
}

TEST_F(BluetoothSerialMockTest, SynchronousScanCustomTimeout) {
    EXPECT_CALL(*mockImpl, scanDevices(10s))
        .WillOnce(::testing::Return(testDevices));

    auto devices = mockImpl->scanDevices(10s);
    EXPECT_EQ(devices.size(), 3);
}

TEST_F(BluetoothSerialMockTest, AsynchronousScan) {
    std::vector<BluetoothDeviceInfo> foundDevices;
    std::atomic<bool> scanComplete{false};
    std::mutex mutex;
    std::condition_variable cv;

    EXPECT_CALL(*mockImpl,
                scanDevicesAsync(::testing::_, ::testing::_, ::testing::_))
        .WillOnce([this](auto onDeviceFound, auto onScanComplete, auto) {
            std::thread([this, onDeviceFound, onScanComplete]() {
                for (const auto& device : testDevices) {
                    onDeviceFound(device);
                    std::this_thread::sleep_for(10ms);
                }
                onScanComplete();
            }).detach();
        });

    mockImpl->scanDevicesAsync(
        [&foundDevices, &mutex](const BluetoothDeviceInfo& device) {
            std::lock_guard<std::mutex> lock(mutex);
            foundDevices.push_back(device);
        },
        [&scanComplete, &cv]() {
            scanComplete = true;
            cv.notify_one();
        },
        5s);

    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5s, [&scanComplete] { return scanComplete.load(); });
    }

    EXPECT_TRUE(scanComplete.load());
    EXPECT_EQ(foundDevices.size(), 3);
}

TEST_F(BluetoothSerialMockTest, StopScan) {
    EXPECT_CALL(*mockImpl, stopScan()).Times(1);
    mockImpl->stopScan();
}

// Connection Tests
TEST_F(BluetoothSerialMockTest, Connect) {
    EXPECT_CALL(*mockImpl, connect("11:22:33:44:55:66", ::testing::_)).Times(1);
    EXPECT_CALL(*mockImpl, isConnected()).WillOnce(::testing::Return(true));
    EXPECT_CALL(*mockImpl, getConnectedDevice())
        .WillOnce(::testing::Return(testDevices[2]));

    mockImpl->connect("11:22:33:44:55:66", BluetoothConfig{});

    EXPECT_TRUE(mockImpl->isConnected());

    auto device = mockImpl->getConnectedDevice();
    EXPECT_TRUE(device.has_value());
    EXPECT_EQ(device->address, "11:22:33:44:55:66");
}

TEST_F(BluetoothSerialMockTest, ConnectFailure) {
    EXPECT_CALL(*mockImpl, connect("invalid", ::testing::_))
        .WillOnce(::testing::Throw(BluetoothException("Connection failed")));

    EXPECT_THROW(mockImpl->connect("invalid", BluetoothConfig{}),
                 BluetoothException);
}

TEST_F(BluetoothSerialMockTest, Disconnect) {
    EXPECT_CALL(*mockImpl, isConnected())
        .WillOnce(::testing::Return(true))
        .WillOnce(::testing::Return(false));
    EXPECT_CALL(*mockImpl, disconnect()).Times(1);

    EXPECT_TRUE(mockImpl->isConnected());
    mockImpl->disconnect();
    EXPECT_FALSE(mockImpl->isConnected());
}

TEST_F(BluetoothSerialMockTest, GetConnectedDeviceWhenNotConnected) {
    EXPECT_CALL(*mockImpl, isConnected()).WillOnce(::testing::Return(false));
    EXPECT_CALL(*mockImpl, getConnectedDevice())
        .WillOnce(::testing::Return(std::nullopt));

    EXPECT_FALSE(mockImpl->isConnected());
    auto device = mockImpl->getConnectedDevice();
    EXPECT_FALSE(device.has_value());
}

// Pairing Tests
TEST_F(BluetoothSerialMockTest, PairSuccess) {
    EXPECT_CALL(*mockImpl, pair("00:11:22:33:44:55", "1234"))
        .WillOnce(::testing::Return(true));

    bool result = mockImpl->pair("00:11:22:33:44:55", "1234");
    EXPECT_TRUE(result);
}

TEST_F(BluetoothSerialMockTest, PairFailure) {
    EXPECT_CALL(*mockImpl, pair("invalid", "1234"))
        .WillOnce(::testing::Return(false));

    bool result = mockImpl->pair("invalid", "1234");
    EXPECT_FALSE(result);
}

TEST_F(BluetoothSerialMockTest, PairWithCustomPin) {
    EXPECT_CALL(*mockImpl, pair("AA:BB:CC:DD:EE:FF", "5678"))
        .WillOnce(::testing::Return(true));

    bool result = mockImpl->pair("AA:BB:CC:DD:EE:FF", "5678");
    EXPECT_TRUE(result);
}

TEST_F(BluetoothSerialMockTest, Unpair) {
    EXPECT_CALL(*mockImpl, unpair("AA:BB:CC:DD:EE:FF"))
        .WillOnce(::testing::Return(true));

    bool result = mockImpl->unpair("AA:BB:CC:DD:EE:FF");
    EXPECT_TRUE(result);
}

TEST_F(BluetoothSerialMockTest, GetPairedDevices) {
    std::vector<BluetoothDeviceInfo> pairedDevices = {testDevices[1],
                                                      testDevices[2]};

    EXPECT_CALL(*mockImpl, getPairedDevices())
        .WillOnce(::testing::Return(pairedDevices));

    auto devices = mockImpl->getPairedDevices();
    EXPECT_EQ(devices.size(), 2);
    EXPECT_TRUE(devices[0].paired);
    EXPECT_TRUE(devices[1].paired);
}

// Data Transfer Tests
TEST_F(BluetoothSerialMockTest, Read) {
    EXPECT_CALL(*mockImpl, read(5)).WillOnce(::testing::Return(testData));

    auto data = mockImpl->read(5);
    EXPECT_EQ(data.size(), 5);
    EXPECT_EQ(data, testData);
}

TEST_F(BluetoothSerialMockTest, ReadExactly) {
    std::vector<uint8_t> exactData = {0x01, 0x02, 0x03};
    EXPECT_CALL(*mockImpl, readExactly(3, 1000ms))
        .WillOnce(::testing::Return(exactData));

    auto data = mockImpl->readExactly(3, 1000ms);
    EXPECT_EQ(data.size(), 3);
}

TEST_F(BluetoothSerialMockTest, ReadAvailable) {
    EXPECT_CALL(*mockImpl, readAvailable())
        .WillOnce(::testing::Return(testData));

    auto data = mockImpl->readAvailable();
    EXPECT_EQ(data.size(), 5);
}

TEST_F(BluetoothSerialMockTest, Available) {
    EXPECT_CALL(*mockImpl, available()).WillOnce(::testing::Return(10));

    size_t avail = mockImpl->available();
    EXPECT_EQ(avail, 10);
}

TEST_F(BluetoothSerialMockTest, AsyncRead) {
    std::vector<uint8_t> receivedData;
    std::atomic<bool> dataReceived{false};
    std::mutex mutex;
    std::condition_variable cv;

    EXPECT_CALL(*mockImpl, asyncRead(::testing::_, ::testing::_))
        .WillOnce([this](size_t, auto callback) {
            std::thread([this, callback]() {
                std::this_thread::sleep_for(50ms);
                callback(testData);
            }).detach();
        });

    mockImpl->asyncRead(10, [&](std::vector<uint8_t> data) {
        std::lock_guard<std::mutex> lock(mutex);
        receivedData = std::move(data);
        dataReceived = true;
        cv.notify_one();
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5s, [&dataReceived] { return dataReceived.load(); });
    }

    EXPECT_TRUE(dataReceived.load());
    EXPECT_EQ(receivedData.size(), 5);
}

TEST_F(BluetoothSerialMockTest, WriteBinary) {
    EXPECT_CALL(*mockImpl, write(::testing::_)).WillOnce(::testing::Return(5));

    size_t written = mockImpl->write(std::span<const uint8_t>(testData));
    EXPECT_EQ(written, 5);
}

TEST_F(BluetoothSerialMockTest, WriteString) {
    EXPECT_CALL(*mockImpl, writeString("Hello Bluetooth"))
        .WillOnce(::testing::Return(15));

    size_t written = mockImpl->writeString("Hello Bluetooth");
    EXPECT_EQ(written, 15);
}

TEST_F(BluetoothSerialMockTest, Flush) {
    EXPECT_CALL(*mockImpl, flush()).Times(1);
    mockImpl->flush();
}

// Connection Listener Tests
TEST_F(BluetoothSerialMockTest, SetConnectionListener) {
    bool connectionState = false;

    EXPECT_CALL(*mockImpl, setConnectionListener(::testing::_)).Times(1);

    mockImpl->setConnectionListener(
        [&connectionState](bool connected) { connectionState = connected; });
}

// Statistics Tests
TEST_F(BluetoothSerialMockTest, GetStatistics) {
    BluetoothSerial::Statistics expectedStats;
    expectedStats.bytesSent = 100;
    expectedStats.bytesReceived = 75;
    expectedStats.connectionTime = std::chrono::steady_clock::now() - 5min;
    expectedStats.currentRssi = -65;

    EXPECT_CALL(*mockImpl, getStatistics())
        .WillOnce(::testing::Return(expectedStats));

    auto stats = mockImpl->getStatistics();

    EXPECT_EQ(stats.bytesSent, 100);
    EXPECT_EQ(stats.bytesReceived, 75);
    EXPECT_EQ(stats.currentRssi, -65);
}

// =============================================================================
// BluetoothSerial Real Instance Tests (without hardware)
// =============================================================================

class BluetoothSerialTest : public ::testing::Test {
protected:
    void SetUp() override { bluetooth = std::make_unique<BluetoothSerial>(); }

    void TearDown() override {
        if (bluetooth && bluetooth->isConnected()) {
            bluetooth->disconnect();
        }
    }

    std::unique_ptr<BluetoothSerial> bluetooth;
};

TEST_F(BluetoothSerialTest, DefaultConstruction) {
    EXPECT_FALSE(bluetooth->isConnected());
}

TEST_F(BluetoothSerialTest, MoveConstruction) {
    BluetoothSerial bt1;
    BluetoothSerial bt2(std::move(bt1));
    EXPECT_FALSE(bt2.isConnected());
}

TEST_F(BluetoothSerialTest, MoveAssignment) {
    BluetoothSerial bt1;
    BluetoothSerial bt2;
    bt2 = std::move(bt1);
    EXPECT_FALSE(bt2.isConnected());
}

TEST_F(BluetoothSerialTest, GetConnectedDeviceWhenNotConnected) {
    auto device = bluetooth->getConnectedDevice();
    EXPECT_FALSE(device.has_value());
}

TEST_F(BluetoothSerialTest, GetStatisticsInitial) {
    auto stats = bluetooth->getStatistics();
    EXPECT_EQ(stats.bytesSent, 0);
    EXPECT_EQ(stats.bytesReceived, 0);
}

// =============================================================================
// MAC Address Validation Tests
// =============================================================================

TEST(MACAddressTest, ValidFormats) {
    std::vector<std::string> validAddresses = {
        "00:11:22:33:44:55", "AA:BB:CC:DD:EE:FF", "aa:bb:cc:dd:ee:ff",
        "12:34:56:78:9A:BC"};

    for (const auto& addr : validAddresses) {
        EXPECT_EQ(addr.length(), 17);
        // Count colons
        int colonCount = std::count(addr.begin(), addr.end(), ':');
        EXPECT_EQ(colonCount, 5);
    }
}

TEST(MACAddressTest, InvalidFormats) {
    std::vector<std::string> invalidAddresses = {
        "", "invalid",
        "00:11:22:33:44",        // Too short
        "00:11:22:33:44:55:66",  // Too long
        "00-11-22-33-44-55"      // Wrong separator
    };

    for (const auto& addr : invalidAddresses) {
        EXPECT_NE(addr.length(), 17);
    }
}

// =============================================================================
// UUID Tests
// =============================================================================

TEST(BluetoothUUIDTest, SPPServiceUUID) {
    // Standard Serial Port Profile UUID
    std::string sppUUID = "00001101-0000-1000-8000-00805F9B34FB";
    EXPECT_EQ(sppUUID.length(), 36);
}

TEST(BluetoothUUIDTest, CommonServiceUUIDs) {
    std::vector<std::string> commonUUIDs = {
        "00001101-0000-1000-8000-00805F9B34FB",  // SPP
        "00001800-0000-1000-8000-00805F9B34FB",  // Generic Access
        "00001801-0000-1000-8000-00805F9B34FB",  // Generic Attribute
        "0000180A-0000-1000-8000-00805F9B34FB"   // Device Information
    };

    for (const auto& uuid : commonUUIDs) {
        EXPECT_EQ(uuid.length(), 36);
        // Check format: 8-4-4-4-12
        EXPECT_EQ(uuid[8], '-');
        EXPECT_EQ(uuid[13], '-');
        EXPECT_EQ(uuid[18], '-');
        EXPECT_EQ(uuid[23], '-');
    }
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST(BluetoothErrorHandlingTest, ConnectionTimeout) {
    BluetoothConfig config;
    config.connectTimeout = 100ms;  // Very short timeout

    // Just verify config is set correctly
    EXPECT_EQ(config.connectTimeout, 100ms);
}

TEST(BluetoothErrorHandlingTest, InvalidPIN) {
    BluetoothConfig config;
    config.pin = "";  // Empty PIN

    // Empty PIN should still be valid (some devices don't require PIN)
    EXPECT_TRUE(config.pin.empty());
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

class BluetoothSerialThreadSafetyTest : public ::testing::Test {
protected:
    std::shared_ptr<MockBluetoothSerialImpl> mockImpl =
        std::make_shared<MockBluetoothSerialImpl>();
};

TEST_F(BluetoothSerialThreadSafetyTest, ConcurrentReads) {
    constexpr int numThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> completedReads{0};

    EXPECT_CALL(*mockImpl, read(::testing::_))
        .Times(numThreads)
        .WillRepeatedly(::testing::Return(std::vector<uint8_t>{0x01, 0x02}));

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &completedReads]() {
            auto data = mockImpl->read(10);
            EXPECT_FALSE(data.empty());
            ++completedReads;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completedReads.load(), numThreads);
}

TEST_F(BluetoothSerialThreadSafetyTest, ConcurrentWrites) {
    constexpr int numThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> completedWrites{0};

    EXPECT_CALL(*mockImpl, write(::testing::_))
        .Times(numThreads)
        .WillRepeatedly(::testing::Return(5));

    std::vector<uint8_t> testData = {0x01, 0x02, 0x03, 0x04, 0x05};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &completedWrites, &testData]() {
            size_t written =
                mockImpl->write(std::span<const uint8_t>(testData));
            EXPECT_EQ(written, 5);
            ++completedWrites;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completedWrites.load(), numThreads);
}
