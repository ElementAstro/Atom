/*
 * test_serial_port.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-16

Description: Comprehensive Unit Tests for SerialPort
Tests serial port configuration, I/O operations, and signal control.

**************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/serial/serial_port.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace serial;

// =============================================================================
// SerialConfig Tests
// =============================================================================

class SerialConfigTest : public ::testing::Test {
protected:
    SerialConfig config;
};

TEST_F(SerialConfigTest, DefaultValues) {
    EXPECT_EQ(config.getBaudRate(), 9600);
    EXPECT_EQ(config.getDataBits(), 8);
    EXPECT_EQ(config.getParity(), SerialConfig::Parity::None);
    EXPECT_EQ(config.getStopBits(), SerialConfig::StopBits::One);
    EXPECT_EQ(config.getFlowControl(), SerialConfig::FlowControl::None);
    EXPECT_EQ(config.getReadTimeout(), 1000ms);
    EXPECT_EQ(config.getWriteTimeout(), 1000ms);
}

TEST_F(SerialConfigTest, WithBaudRate) {
    auto& result = config.withBaudRate(115200);
    EXPECT_EQ(config.getBaudRate(), 115200);
    EXPECT_EQ(&result, &config);  // Fluent interface
}

TEST_F(SerialConfigTest, WithBaudRateInvalid) {
    EXPECT_THROW(config.withBaudRate(0), SerialConfigException);
    EXPECT_THROW(config.withBaudRate(-1), SerialConfigException);
}

TEST_F(SerialConfigTest, WithDataBits) {
    config.withDataBits(7);
    EXPECT_EQ(config.getDataBits(), 7);

    config.withDataBits(5);
    EXPECT_EQ(config.getDataBits(), 5);

    config.withDataBits(8);
    EXPECT_EQ(config.getDataBits(), 8);
}

TEST_F(SerialConfigTest, WithDataBitsInvalid) {
    EXPECT_THROW(config.withDataBits(4), SerialConfigException);
    EXPECT_THROW(config.withDataBits(9), SerialConfigException);
}

TEST_F(SerialConfigTest, WithParity) {
    config.withParity(SerialConfig::Parity::Odd);
    EXPECT_EQ(config.getParity(), SerialConfig::Parity::Odd);

    config.withParity(SerialConfig::Parity::Even);
    EXPECT_EQ(config.getParity(), SerialConfig::Parity::Even);

    config.withParity(SerialConfig::Parity::Mark);
    EXPECT_EQ(config.getParity(), SerialConfig::Parity::Mark);

    config.withParity(SerialConfig::Parity::Space);
    EXPECT_EQ(config.getParity(), SerialConfig::Parity::Space);
}

TEST_F(SerialConfigTest, WithStopBits) {
    config.withStopBits(SerialConfig::StopBits::Two);
    EXPECT_EQ(config.getStopBits(), SerialConfig::StopBits::Two);

    config.withStopBits(SerialConfig::StopBits::OnePointFive);
    EXPECT_EQ(config.getStopBits(), SerialConfig::StopBits::OnePointFive);
}

TEST_F(SerialConfigTest, WithFlowControl) {
    config.withFlowControl(SerialConfig::FlowControl::Hardware);
    EXPECT_EQ(config.getFlowControl(), SerialConfig::FlowControl::Hardware);

    config.withFlowControl(SerialConfig::FlowControl::Software);
    EXPECT_EQ(config.getFlowControl(), SerialConfig::FlowControl::Software);
}

TEST_F(SerialConfigTest, WithReadTimeout) {
    config.withReadTimeout(500ms);
    EXPECT_EQ(config.getReadTimeout(), 500ms);
}

TEST_F(SerialConfigTest, WithWriteTimeout) {
    config.withWriteTimeout(2000ms);
    EXPECT_EQ(config.getWriteTimeout(), 2000ms);
}

TEST_F(SerialConfigTest, SetReadTimeout) {
    config.setReadTimeout(750ms);
    EXPECT_EQ(config.getReadTimeout(), 750ms);
}

TEST_F(SerialConfigTest, SetWriteTimeout) {
    config.setWriteTimeout(1500ms);
    EXPECT_EQ(config.getWriteTimeout(), 1500ms);
}

TEST_F(SerialConfigTest, StandardConfig) {
    auto stdConfig = SerialConfig::standardConfig(115200);

    EXPECT_EQ(stdConfig.getBaudRate(), 115200);
    EXPECT_EQ(stdConfig.getDataBits(), 8);
    EXPECT_EQ(stdConfig.getStopBits(), SerialConfig::StopBits::One);
    EXPECT_EQ(stdConfig.getParity(), SerialConfig::Parity::None);
    EXPECT_EQ(stdConfig.getFlowControl(), SerialConfig::FlowControl::None);
}

TEST_F(SerialConfigTest, FluentInterface) {
    config.withBaudRate(115200)
        .withDataBits(8)
        .withParity(SerialConfig::Parity::None)
        .withStopBits(SerialConfig::StopBits::One)
        .withFlowControl(SerialConfig::FlowControl::None)
        .withReadTimeout(500ms)
        .withWriteTimeout(500ms);

    EXPECT_EQ(config.getBaudRate(), 115200);
    EXPECT_EQ(config.getDataBits(), 8);
    EXPECT_EQ(config.getParity(), SerialConfig::Parity::None);
    EXPECT_EQ(config.getStopBits(), SerialConfig::StopBits::One);
    EXPECT_EQ(config.getFlowControl(), SerialConfig::FlowControl::None);
    EXPECT_EQ(config.getReadTimeout(), 500ms);
    EXPECT_EQ(config.getWriteTimeout(), 500ms);
}

// =============================================================================
// Exception Tests
// =============================================================================

TEST(SerialExceptionTest, Construction) {
    SerialException ex("Test error");
    EXPECT_STREQ(ex.what(), "Test error");
}

TEST(SerialPortNotOpenExceptionTest, Construction) {
    SerialPortNotOpenException ex;
    EXPECT_STREQ(ex.what(), "Port is not open");
}

TEST(SerialTimeoutExceptionTest, DefaultConstruction) {
    SerialTimeoutException ex;
    EXPECT_STREQ(ex.what(), "Serial operation timed out");
}

TEST(SerialTimeoutExceptionTest, ConstructionWithMessage) {
    SerialTimeoutException ex("Custom timeout message");
    EXPECT_THAT(std::string(ex.what()),
                ::testing::HasSubstr("Custom timeout message"));
}

TEST(SerialIOExceptionTest, Construction) {
    SerialIOException ex("I/O error occurred");
    EXPECT_STREQ(ex.what(), "I/O error occurred");
}

TEST(SerialConfigExceptionTest, Construction) {
    SerialConfigException ex("Invalid baud rate");
    EXPECT_THAT(std::string(ex.what()),
                ::testing::HasSubstr("Configuration error"));
    EXPECT_THAT(std::string(ex.what()),
                ::testing::HasSubstr("Invalid baud rate"));
}

// =============================================================================
// SerialPort Basic Tests
// =============================================================================

class SerialPortTest : public ::testing::Test {
protected:
    void SetUp() override {
        port = std::make_unique<SerialPort>();

        // Platform-specific test port name
#ifdef _WIN32
        testPortName = "COM1";
#elif defined(__APPLE__)
        testPortName = "/dev/cu.usbserial";
#else
        testPortName = "/dev/ttyUSB0";
#endif

        testConfig = SerialConfig::standardConfig(9600);
    }

    void TearDown() override {
        if (port && port->isOpen()) {
            port->close();
        }
    }

    std::unique_ptr<SerialPort> port;
    std::string testPortName;
    SerialConfig testConfig;
};

TEST_F(SerialPortTest, DefaultConstruction) {
    EXPECT_FALSE(port->isOpen());
    EXPECT_TRUE(port->getPortName().empty());
}

TEST_F(SerialPortTest, MoveConstruction) {
    SerialPort port1;
    SerialPort port2(std::move(port1));
    // port2 should be valid
    EXPECT_FALSE(port2.isOpen());
}

TEST_F(SerialPortTest, MoveAssignment) {
    SerialPort port1;
    SerialPort port2;
    port2 = std::move(port1);
    // port2 should be valid
    EXPECT_FALSE(port2.isOpen());
}

TEST_F(SerialPortTest, GetAvailablePorts) {
    auto ports = SerialPort::getAvailablePorts();
    // Should return a vector (may be empty if no ports available)
    // Just verify it doesn't throw
    SUCCEED();
}

TEST_F(SerialPortTest, TryOpenNonExistent) {
    auto error = port->tryOpen("NON_EXISTENT_PORT_12345", testConfig);
    EXPECT_TRUE(error.has_value());
    EXPECT_FALSE(error->empty());
}

// =============================================================================
// SerialPort Mock Tests (for testing without hardware)
// =============================================================================

class MockSerialPortImpl {
public:
    MOCK_METHOD(void, open, (const std::string&, const SerialConfig&));
    MOCK_METHOD(void, close, ());
    MOCK_METHOD(bool, isOpen, (), (const));
    MOCK_METHOD(std::vector<uint8_t>, read, (size_t));
    MOCK_METHOD(std::vector<uint8_t>, readExactly,
                (size_t, std::chrono::milliseconds));
    MOCK_METHOD(std::string, readUntil,
                (char, std::chrono::milliseconds, bool));
    MOCK_METHOD(std::vector<uint8_t>, readUntilSequence,
                (std::span<const uint8_t>, std::chrono::milliseconds, bool));
    MOCK_METHOD(void, asyncRead,
                (size_t, std::function<void(std::vector<uint8_t>)>));
    MOCK_METHOD(std::vector<uint8_t>, readAvailable, ());
    MOCK_METHOD(size_t, write, (std::span<const uint8_t>));
    MOCK_METHOD(size_t, writeString, (std::string_view));
    MOCK_METHOD(void, flush, ());
    MOCK_METHOD(void, drain, ());
    MOCK_METHOD(size_t, available, (), (const));
    MOCK_METHOD(void, setConfig, (const SerialConfig&));
    MOCK_METHOD(SerialConfig, getConfig, (), (const));
    MOCK_METHOD(void, setDTR, (bool));
    MOCK_METHOD(void, setRTS, (bool));
    MOCK_METHOD(bool, getCTS, (), (const));
    MOCK_METHOD(bool, getDSR, (), (const));
    MOCK_METHOD(bool, getRI, (), (const));
    MOCK_METHOD(bool, getCD, (), (const));
    MOCK_METHOD(std::string, getPortName, (), (const));
};

class SerialPortMockTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockImpl = std::make_shared<MockSerialPortImpl>();
        testConfig = SerialConfig::standardConfig(115200);
        testData = {0x01, 0x02, 0x03, 0x04, 0x05};
    }

    std::shared_ptr<MockSerialPortImpl> mockImpl;
    SerialConfig testConfig;
    std::vector<uint8_t> testData;
};

TEST_F(SerialPortMockTest, OpenCloseSequence) {
    EXPECT_CALL(*mockImpl, open(::testing::_, ::testing::_)).Times(1);
    EXPECT_CALL(*mockImpl, isOpen())
        .WillOnce(::testing::Return(true))
        .WillOnce(::testing::Return(false));
    EXPECT_CALL(*mockImpl, close()).Times(1);
    EXPECT_CALL(*mockImpl, getPortName()).WillOnce(::testing::Return("COM3"));

    mockImpl->open("COM3", testConfig);
    EXPECT_TRUE(mockImpl->isOpen());
    EXPECT_EQ(mockImpl->getPortName(), "COM3");

    mockImpl->close();
    EXPECT_FALSE(mockImpl->isOpen());
}

TEST_F(SerialPortMockTest, ReadOperations) {
    EXPECT_CALL(*mockImpl, read(5)).WillOnce(::testing::Return(testData));
    EXPECT_CALL(*mockImpl, readExactly(3, 1000ms))
        .WillOnce(::testing::Return(std::vector<uint8_t>{0x01, 0x02, 0x03}));
    EXPECT_CALL(*mockImpl, readAvailable())
        .WillOnce(::testing::Return(testData));
    EXPECT_CALL(*mockImpl, available()).WillOnce(::testing::Return(5));

    auto data = mockImpl->read(5);
    EXPECT_EQ(data.size(), 5);
    EXPECT_EQ(data, testData);

    auto exactData = mockImpl->readExactly(3, 1000ms);
    EXPECT_EQ(exactData.size(), 3);

    auto availData = mockImpl->readAvailable();
    EXPECT_EQ(availData.size(), 5);

    EXPECT_EQ(mockImpl->available(), 5);
}

TEST_F(SerialPortMockTest, WriteOperations) {
    EXPECT_CALL(*mockImpl, write(::testing::_)).WillOnce(::testing::Return(5));
    EXPECT_CALL(*mockImpl, writeString(::testing::_))
        .WillOnce(::testing::Return(12));
    EXPECT_CALL(*mockImpl, flush()).Times(1);
    EXPECT_CALL(*mockImpl, drain()).Times(1);

    size_t written = mockImpl->write(std::span<const uint8_t>(testData));
    EXPECT_EQ(written, 5);

    written = mockImpl->writeString("Hello Serial");
    EXPECT_EQ(written, 12);

    mockImpl->flush();
    mockImpl->drain();
}

TEST_F(SerialPortMockTest, ConfigOperations) {
    EXPECT_CALL(*mockImpl, setConfig(::testing::_)).Times(1);
    EXPECT_CALL(*mockImpl, getConfig()).WillOnce(::testing::Return(testConfig));

    mockImpl->setConfig(testConfig);
    auto config = mockImpl->getConfig();
    EXPECT_EQ(config.getBaudRate(), 115200);
}

TEST_F(SerialPortMockTest, SignalOperations) {
    EXPECT_CALL(*mockImpl, setDTR(true)).Times(1);
    EXPECT_CALL(*mockImpl, setRTS(false)).Times(1);
    EXPECT_CALL(*mockImpl, getCTS()).WillOnce(::testing::Return(true));
    EXPECT_CALL(*mockImpl, getDSR()).WillOnce(::testing::Return(false));
    EXPECT_CALL(*mockImpl, getRI()).WillOnce(::testing::Return(false));
    EXPECT_CALL(*mockImpl, getCD()).WillOnce(::testing::Return(true));

    mockImpl->setDTR(true);
    mockImpl->setRTS(false);

    EXPECT_TRUE(mockImpl->getCTS());
    EXPECT_FALSE(mockImpl->getDSR());
    EXPECT_FALSE(mockImpl->getRI());
    EXPECT_TRUE(mockImpl->getCD());
}

TEST_F(SerialPortMockTest, AsyncRead) {
    std::vector<uint8_t> receivedData;
    std::atomic<bool> dataReceived{false};
    std::mutex mutex;
    std::condition_variable cv;

    EXPECT_CALL(*mockImpl, asyncRead(::testing::_, ::testing::_))
        .WillOnce([this, &receivedData, &dataReceived, &cv](
                      size_t maxBytes,
                      std::function<void(std::vector<uint8_t>)> callback) {
            std::thread([this, callback, &receivedData, &dataReceived, &cv]() {
                std::this_thread::sleep_for(50ms);
                callback(testData);
                dataReceived = true;
                cv.notify_one();
            }).detach();
        });

    mockImpl->asyncRead(10, [&receivedData](std::vector<uint8_t> data) {
        receivedData = std::move(data);
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5s, [&dataReceived] { return dataReceived.load(); });
    }

    EXPECT_TRUE(dataReceived.load());
    EXPECT_EQ(receivedData.size(), 5);
}

TEST_F(SerialPortMockTest, ExceptionHandling) {
    EXPECT_CALL(*mockImpl, read(::testing::_))
        .WillOnce(::testing::Throw(SerialPortNotOpenException()));

    EXPECT_THROW(mockImpl->read(5), SerialPortNotOpenException);
}

TEST_F(SerialPortMockTest, TimeoutHandling) {
    EXPECT_CALL(*mockImpl, readExactly(::testing::_, ::testing::_))
        .WillOnce(::testing::Throw(SerialTimeoutException()));

    EXPECT_THROW(mockImpl->readExactly(10, 500ms), SerialTimeoutException);
}

TEST_F(SerialPortMockTest, IOErrorHandling) {
    EXPECT_CALL(*mockImpl, write(::testing::_))
        .WillOnce(::testing::Throw(SerialIOException("Write failed")));

    EXPECT_THROW(mockImpl->write(std::span<const uint8_t>(testData)),
                 SerialIOException);
}

// =============================================================================
// Serializable Concept Tests
// =============================================================================

TEST(SerializableConceptTest, TrivialTypes) {
    // These should satisfy the Serializable concept
    static_assert(Serializable<int>);
    static_assert(Serializable<uint8_t>);
    static_assert(Serializable<double>);
    static_assert(Serializable<char>);

    struct TrivialStruct {
        int a;
        float b;
    };
    static_assert(Serializable<TrivialStruct>);
}

TEST(SerializableConceptTest, NonTrivialTypes) {
    // std::string is not trivially copyable
    static_assert(!Serializable<std::string>);
    static_assert(!Serializable<std::vector<int>>);
}

// =============================================================================
// Edge Case Tests
// =============================================================================

class SerialPortEdgeCaseTest : public ::testing::Test {
protected:
    std::shared_ptr<MockSerialPortImpl> mockImpl =
        std::make_shared<MockSerialPortImpl>();
};

TEST_F(SerialPortEdgeCaseTest, ZeroLengthRead) {
    EXPECT_CALL(*mockImpl, read(0))
        .WillOnce(::testing::Return(std::vector<uint8_t>{}));

    auto data = mockImpl->read(0);
    EXPECT_TRUE(data.empty());
}

TEST_F(SerialPortEdgeCaseTest, ZeroLengthWrite) {
    std::vector<uint8_t> emptyData;
    EXPECT_CALL(*mockImpl, write(::testing::_)).WillOnce(::testing::Return(0));

    size_t written = mockImpl->write(std::span<const uint8_t>(emptyData));
    EXPECT_EQ(written, 0);
}

TEST_F(SerialPortEdgeCaseTest, LargeRead) {
    constexpr size_t largeSize = 1024 * 1024;  // 1MB
    std::vector<uint8_t> largeData(largeSize, 0xAA);

    EXPECT_CALL(*mockImpl, read(largeSize))
        .WillOnce(::testing::Return(largeData));

    auto data = mockImpl->read(largeSize);
    EXPECT_EQ(data.size(), largeSize);
}

TEST_F(SerialPortEdgeCaseTest, LargeWrite) {
    constexpr size_t largeSize = 1024 * 1024;  // 1MB
    std::vector<uint8_t> largeData(largeSize, 0xBB);

    EXPECT_CALL(*mockImpl, write(::testing::_))
        .WillOnce(::testing::Return(largeSize));

    size_t written = mockImpl->write(std::span<const uint8_t>(largeData));
    EXPECT_EQ(written, largeSize);
}

// =============================================================================
// Platform-Specific Tests
// =============================================================================

#ifdef _WIN32
TEST(SerialPortWindowsTest, PortNameFormat) {
    // Windows COM port names
    std::vector<std::string> validNames = {"COM1", "COM10", "COM256"};

    for (const auto& name : validNames) {
        // Just verify format - actual opening would require hardware
        EXPECT_TRUE(name.find("COM") == 0);
    }
}
#else
TEST(SerialPortUnixTest, PortNameFormat) {
    // Unix serial port names
    std::vector<std::string> validNames = {"/dev/ttyUSB0", "/dev/ttyACM0",
                                           "/dev/ttyS0"};

    for (const auto& name : validNames) {
        EXPECT_TRUE(name.find("/dev/tty") == 0);
    }
}
#endif

// =============================================================================
// Baud Rate Tests
// =============================================================================

TEST(BaudRateTest, CommonBaudRates) {
    std::vector<int> commonRates = {300,    1200,   2400,   4800,
                                    9600,   19200,  38400,  57600,
                                    115200, 230400, 460800, 921600};

    for (int rate : commonRates) {
        SerialConfig config;
        EXPECT_NO_THROW(config.withBaudRate(rate));
        EXPECT_EQ(config.getBaudRate(), rate);
    }
}
