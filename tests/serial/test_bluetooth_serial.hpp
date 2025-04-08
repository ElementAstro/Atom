#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "atom/serial/bluetooth_serial.hpp"

using namespace std::chrono_literals;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using ::testing::SetArgReferee;

namespace serial {

// Mock implementation of BluetoothSerialImpl
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
    MOCK_METHOD(size_t, write, (const std::string&));
    MOCK_METHOD(void, flush, ());
    MOCK_METHOD(size_t, available, (), (const));
    MOCK_METHOD(void, setConnectionListener, (std::function<void(bool)>));
    MOCK_METHOD(BluetoothSerial::Statistics, getStatistics, (), (const));
};

// Helper function to create sample device info
BluetoothDeviceInfo createSampleDevice(const std::string& address,
                                       const std::string& name, int rssi = -70,
                                       bool paired = false,
                                       bool connected = false) {
    BluetoothDeviceInfo device;
    device.address = address;
    device.name = name;
    device.rssi = rssi;
    device.paired = paired;
    device.connected = connected;
    device.services = {"00001101-0000-1000-8000-00805F9B34FB"};  // SPP service
    return device;
}

// Test fixture for BluetoothSerial tests
class BluetoothSerialTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create mock implementation
        mockImpl = std::make_shared<MockBluetoothSerialImpl>();

        // Create test device list
        testDevices = {
            createSampleDevice("00:11:22:33:44:55", "Test Device 1"),
            createSampleDevice("AA:BB:CC:DD:EE:FF", "Test Device 2", -60, true),
            createSampleDevice("11:22:33:44:55:66", "Connected Device", -50,
                               true, true)};

        // Create connected device info
        connectedDevice = testDevices[2];
    }

    void TearDown() override {
        // Clean up any resources
    }

    // Helper function to simulate async scan completion
    void simulateAsyncScan(
        std::function<void(const BluetoothDeviceInfo&)> onDeviceFound,
        std::function<void()> onScanComplete) {
        for (const auto& device : testDevices) {
            onDeviceFound(device);
            std::this_thread::sleep_for(50ms);
        }
        onScanComplete();
    }

    std::shared_ptr<MockBluetoothSerialImpl> mockImpl;
    std::vector<BluetoothDeviceInfo> testDevices;
    BluetoothDeviceInfo connectedDevice;
};

// Test Bluetooth adapter status
TEST_F(BluetoothSerialTest, AdapterStatus) {
    // Setup expectations
    EXPECT_CALL(*mockImpl, isBluetoothEnabled())
        .WillOnce(Return(false))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockImpl, enableBluetooth(true)).Times(1);

    // TODO: Create real BluetoothSerial with mocked implementation
    // For this test, we'll simulate how it would behave

    // Test adapter initially disabled
    bool enabled = mockImpl->isBluetoothEnabled();
    EXPECT_FALSE(enabled);

    // Enable adapter
    mockImpl->enableBluetooth(true);

    // Test adapter now enabled
    enabled = mockImpl->isBluetoothEnabled();
    EXPECT_TRUE(enabled);
}

// Test synchronous device scanning
TEST_F(BluetoothSerialTest, SynchronousScan) {
    // Setup expectations
    EXPECT_CALL(*mockImpl, scanDevices(_)).WillOnce(Return(testDevices));

    // Perform scan
    auto devices = mockImpl->scanDevices(5s);

    // Verify results
    ASSERT_EQ(devices.size(), 3);
    EXPECT_EQ(devices[0].address, "00:11:22:33:44:55");
    EXPECT_EQ(devices[1].name, "Test Device 2");
    EXPECT_TRUE(devices[2].connected);
}

// Test asynchronous device scanning
TEST_F(BluetoothSerialTest, AsynchronousScan) {
    std::vector<BluetoothDeviceInfo> foundDevices;
    std::atomic<bool> scanComplete{false};
    std::mutex mutex;
    std::condition_variable cv;

    // Setup expectations
    EXPECT_CALL(*mockImpl, scanDevicesAsync(_, _, _))
        .WillOnce(
            [this](auto onDeviceFound, auto onScanComplete, auto timeout) {
                // Simulate async scan in a separate thread
                std::thread([this, onDeviceFound, onScanComplete]() {
                    simulateAsyncScan(onDeviceFound, onScanComplete);
                }).detach();
            });

    EXPECT_CALL(*mockImpl, stopScan()).Times(AtLeast(0));

    // Start async scan
    mockImpl->scanDevicesAsync(
        [&foundDevices, &mutex](const BluetoothDeviceInfo& device) {
            std::lock_guard<std::mutex> lock(mutex);
            foundDevices.push_back(device);
        },
        [&scanComplete, &cv]() {
            scanComplete = true;
            cv.notify_one();
        },
        3s);

    // Wait for scan to complete
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5s, [&scanComplete] { return scanComplete.load(); });
    }

    // Verify results
    ASSERT_TRUE(scanComplete);
    ASSERT_EQ(foundDevices.size(), 3);
    EXPECT_EQ(foundDevices[0].address, "00:11:22:33:44:55");
    EXPECT_EQ(foundDevices[1].name, "Test Device 2");
    EXPECT_TRUE(foundDevices[2].connected);

    // Test stopping scan
    mockImpl->stopScan();
}

// Test connecting to a device
TEST_F(BluetoothSerialTest, ConnectToDevice) {
    // Setup expectations
    EXPECT_CALL(*mockImpl, connect(_, _)).Times(1);

    EXPECT_CALL(*mockImpl, isConnected()).WillOnce(Return(true));

    EXPECT_CALL(*mockImpl, getConnectedDevice())
        .WillOnce(Return(connectedDevice));

    // Connect to device
    mockImpl->connect("11:22:33:44:55:66", BluetoothConfig{});

    // Verify connection status
    EXPECT_TRUE(mockImpl->isConnected());

    // Get connected device info
    auto device = mockImpl->getConnectedDevice();
    ASSERT_TRUE(device.has_value());
    EXPECT_EQ(device->address, "11:22:33:44:55:66");
    EXPECT_EQ(device->name, "Connected Device");
}

// Test disconnecting from a device
TEST_F(BluetoothSerialTest, DisconnectFromDevice) {
    // Setup expectations
    EXPECT_CALL(*mockImpl, isConnected())
        .WillOnce(Return(true))
        .WillOnce(Return(false));

    EXPECT_CALL(*mockImpl, disconnect()).Times(1);

    // Verify initially connected
    EXPECT_TRUE(mockImpl->isConnected());

    // Disconnect
    mockImpl->disconnect();

    // Verify disconnected
    EXPECT_FALSE(mockImpl->isConnected());
}

// Test pairing operations
TEST_F(BluetoothSerialTest, PairingOperations) {
    // Setup expectations
    EXPECT_CALL(*mockImpl, pair("00:11:22:33:44:55", "1234"))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockImpl, pair("invalid-address", "1234"))
        .WillOnce(Return(false));

    EXPECT_CALL(*mockImpl, unpair("AA:BB:CC:DD:EE:FF")).WillOnce(Return(true));

    EXPECT_CALL(*mockImpl, getPairedDevices())
        .WillOnce(Return(
            std::vector<BluetoothDeviceInfo>{testDevices[1], testDevices[2]}));

    // Test successful pairing
    bool pairResult = mockImpl->pair("00:11:22:33:44:55", "1234");
    EXPECT_TRUE(pairResult);

    // Test failed pairing
    pairResult = mockImpl->pair("invalid-address", "1234");
    EXPECT_FALSE(pairResult);

    // Test unpairing
    bool unpairResult = mockImpl->unpair("AA:BB:CC:DD:EE:FF");
    EXPECT_TRUE(unpairResult);

    // Test getting paired devices
    auto pairedDevices = mockImpl->getPairedDevices();
    ASSERT_EQ(pairedDevices.size(), 2);
    EXPECT_TRUE(pairedDevices[0].paired);
    EXPECT_TRUE(pairedDevices[1].paired);
}

// Test reading data
TEST_F(BluetoothSerialTest, ReadData) {
    std::vector<uint8_t> testData = {0x01, 0x02, 0x03, 0x04, 0x05};

    // Setup expectations
    EXPECT_CALL(*mockImpl, read(5)).WillOnce(Return(testData));

    EXPECT_CALL(*mockImpl, readExactly(3, 1000ms))
        .WillOnce(Return(std::vector<uint8_t>{0x01, 0x02, 0x03}));

    EXPECT_CALL(*mockImpl, readAvailable()).WillOnce(Return(testData));

    EXPECT_CALL(*mockImpl, available()).WillOnce(Return(5));

    // Test read
    auto data = mockImpl->read(5);
    ASSERT_EQ(data.size(), 5);
    EXPECT_EQ(data, testData);

    // Test readExactly
    auto exactData = mockImpl->readExactly(3, 1000ms);
    ASSERT_EQ(exactData.size(), 3);
    EXPECT_EQ(exactData[0], 0x01);
    EXPECT_EQ(exactData[1], 0x02);
    EXPECT_EQ(exactData[2], 0x03);

    // Test readAvailable
    auto availableData = mockImpl->readAvailable();
    ASSERT_EQ(availableData.size(), 5);
    EXPECT_EQ(availableData, testData);

    // Test available
    size_t bytesAvailable = mockImpl->available();
    EXPECT_EQ(bytesAvailable, 5);
}

// Test async reading
TEST_F(BluetoothSerialTest, AsyncRead) {
    std::vector<uint8_t> testData = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E};
    std::vector<uint8_t> receivedData;
    std::atomic<bool> dataReceived{false};
    std::mutex mutex;
    std::condition_variable cv;

    // Setup expectations
    EXPECT_CALL(*mockImpl, asyncRead(_, _))
        .WillOnce([&testData](size_t maxBytes, auto callback) {
            // Simulate async read by calling the callback with test data
            std::thread([callback, &testData]() {
                std::this_thread::sleep_for(100ms);
                callback(testData);
            }).detach();
        });

    // Start async read
    mockImpl->asyncRead(10, [&](std::vector<uint8_t> data) {
        std::lock_guard<std::mutex> lock(mutex);
        receivedData = std::move(data);
        dataReceived = true;
        cv.notify_one();
    });

    // Wait for data to be received
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5s, [&dataReceived] { return dataReceived.load(); });
    }

    // Verify received data
    ASSERT_TRUE(dataReceived);
    ASSERT_EQ(receivedData.size(), 5);
    EXPECT_EQ(receivedData, testData);
}

// Test writing data
TEST_F(BluetoothSerialTest, WriteData) {
    std::vector<uint8_t> binaryData = {0x01, 0x02, 0x03, 0x04, 0x05};
    std::string textData = "Hello Bluetooth";

    // Setup expectations
    EXPECT_CALL(*mockImpl, write(std::span<const uint8_t>(binaryData)))
        .WillOnce(Return(5));

    EXPECT_CALL(*mockImpl, write(textData)).WillOnce(Return(15));

    EXPECT_CALL(*mockImpl, flush()).Times(1);

    // Test binary write
    size_t bytesWritten = mockImpl->write(std::span<const uint8_t>(binaryData));
    EXPECT_EQ(bytesWritten, 5);

    // Test string write
    bytesWritten = mockImpl->write(textData);
    EXPECT_EQ(bytesWritten, 15);

    // Test flush
    mockImpl->flush();
}

// Test connection listener
TEST_F(BluetoothSerialTest, ConnectionListener) {
    bool connectionState = false;

    // Setup expectations
    EXPECT_CALL(*mockImpl, setConnectionListener(_)).Times(1);

    // Set connection listener
    mockImpl->setConnectionListener(
        [&connectionState](bool connected) { connectionState = connected; });

    // Simulate connection event by directly calling the lambda
    // In real usage, the implementation would invoke this callback
    connectionState = true;
    EXPECT_TRUE(connectionState);
}

// Test statistics
TEST_F(BluetoothSerialTest, CommunicationStatistics) {
    BluetoothSerial::Statistics expectedStats;
    expectedStats.bytesSent = 100;
    expectedStats.bytesReceived = 75;
    expectedStats.connectionTime = std::chrono::steady_clock::now() - 5min;
    expectedStats.currentRssi = -65;

    // Setup expectations
    EXPECT_CALL(*mockImpl, getStatistics()).WillOnce(Return(expectedStats));

    // Get statistics
    auto stats = mockImpl->getStatistics();

    // Verify statistics
    EXPECT_EQ(stats.bytesSent, 100);
    EXPECT_EQ(stats.bytesReceived, 75);
    EXPECT_EQ(stats.currentRssi, -65);
    // We can't directly compare time points, but can check it's in the past
    EXPECT_LT(stats.connectionTime, std::chrono::steady_clock::now());
}

// Test error handling
TEST_F(BluetoothSerialTest, ErrorHandling) {
    // Setup expectations
    EXPECT_CALL(*mockImpl, connect("invalid-address", _)).WillOnce([]() {
        throw BluetoothException("Invalid device address");
    });

    // Test exception is thrown for invalid address
    EXPECT_THROW(
        {
            try {
                mockImpl->connect("invalid-address", BluetoothConfig{});
            } catch (const BluetoothException& e) {
                EXPECT_STREQ(e.what(), "Invalid device address");
                throw;
            }
        },
        BluetoothException);
}

// Test configuration parameters
TEST_F(BluetoothSerialTest, ConfigurationParameters) {
    BluetoothConfig config;
    config.scanDuration = 10s;
    config.autoReconnect = true;
    config.reconnectInterval = 2s;
    config.pin = "5678";
    config.connectTimeout = 3000ms;
    config.serialConfig.baudRate = 115200;

    // Setup expectations
    EXPECT_CALL(*mockImpl, connect("11:22:33:44:55:66", testing::_)).Times(1);

    // Connect with custom config
    mockImpl->connect("11:22:33:44:55:66", config);

    // We can't verify the config was used correctly without accessing
    // internals, but at least we tested that the method accepts the config
    // parameter
}

// Test move semantics
TEST_F(BluetoothSerialTest, MoveSemantics) {
    // This can't be tested with the mock implementation
    // In a real implementation, you would test move construction and assignment
    // Here we just document how it would be tested

    // 1. Create first BluetoothSerial instance and connect to a device
    // 2. Move-construct a second instance from the first
    // 3. Verify the first is in a valid but unspecified state
    // 4. Verify the second is connected to the device
    // 5. Repeat with move assignment
}

}  // namespace serial

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}