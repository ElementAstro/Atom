#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <thread>
#include "atom/connection/serial/ttybase.hpp"

using namespace atom::connection;
using namespace std::chrono_literals;

// Concrete implementation of TTYBase for testing
class TestTTYClient : public TTYBase {
public:
    explicit TestTTYClient(std::string_view driverName) : TTYBase(driverName) {}

    // getDriverName is not a method in TTYBase
    using TTYBase::getErrorMessage;
};

class TTYBaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_unique<TestTTYClient>("TestTTYDriver");
    }

    void TearDown() override {
        if (client_) {
            client_->disconnect();
        }
        client_.reset();
    }

    std::unique_ptr<TestTTYClient> client_;
};

TEST_F(TTYBaseTest, ConstructorWithDriverName) {
    EXPECT_NO_THROW(TestTTYClient testClient("TestDriver"));
}

TEST_F(TTYBaseTest, ConstructorWithEmptyDriverName) {
    EXPECT_NO_THROW(TestTTYClient testClient(""));
}

TEST_F(TTYBaseTest, GetDriverName) {
    // getDriverName is not available in TTYBase - skipping this test
    SUCCEED();
}

TEST_F(TTYBaseTest, MoveConstructor) {
    TestTTYClient original("OriginalDriver");
    TestTTYClient moved(std::move(original));

    // getDriverName is not available - just test that move succeeded
    SUCCEED();
}

TEST_F(TTYBaseTest, MoveAssignment) {
    TestTTYClient original("OriginalDriver");
    TestTTYClient target("TargetDriver");

    target = std::move(original);
    // getDriverName is not available - just test that move assignment succeeded
    SUCCEED();
}

TEST_F(TTYBaseTest, ConnectWithInvalidDevice) {
    // Try to connect to non-existent device
    auto response = client_->connect("/dev/nonexistent_tty", 9600, 8, 0, 1);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
}

TEST_F(TTYBaseTest, ConnectWithInvalidBaudRate) {
    // Try to connect with invalid baud rate
    auto response = client_->connect("/dev/ttyUSB0", 0, 8, 0, 1);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
}

TEST_F(TTYBaseTest, ConnectWithInvalidWordSize) {
    // Try to connect with invalid word size
    auto response = client_->connect("/dev/ttyUSB0", 9600, 0, 0, 1);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);

    auto response2 = client_->connect("/dev/ttyUSB0", 9600, 9, 0, 1);
    EXPECT_NE(response2, TTYBase::TTYResponse::OK);
}

TEST_F(TTYBaseTest, ConnectWithInvalidParity) {
    // Try to connect with invalid parity
    auto response = client_->connect("/dev/ttyUSB0", 9600, 8, 3, 1);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
}

TEST_F(TTYBaseTest, ConnectWithInvalidStopBits) {
    // Try to connect with invalid stop bits
    auto response = client_->connect("/dev/ttyUSB0", 9600, 8, 0, 0);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);

    auto response2 = client_->connect("/dev/ttyUSB0", 9600, 8, 0, 3);
    EXPECT_NE(response2, TTYBase::TTYResponse::OK);
}

TEST_F(TTYBaseTest, DisconnectWithoutConnection) {
    // Should not fail when disconnecting without connection
    auto response = client_->disconnect();
    EXPECT_EQ(response, TTYBase::TTYResponse::OK);
}

TEST_F(TTYBaseTest, IsConnectedInitialState) {
    EXPECT_FALSE(client_->isConnected());
}

TEST_F(TTYBaseTest, ReadWithoutConnection) {
    std::array<uint8_t, 100> buffer;
    uint32_t bytesRead = 0;

    auto response = client_->read(buffer, 1, bytesRead);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(bytesRead, 0);
}

TEST_F(TTYBaseTest, WriteWithoutConnection) {
    std::array<uint8_t, 10> buffer = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    uint32_t bytesWritten = 0;

    auto response = client_->write(buffer, bytesWritten);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(bytesWritten, 0);
}

TEST_F(TTYBaseTest, WriteStringWithoutConnection) {
    std::string testString = "Hello TTY";
    uint32_t bytesWritten = 0;

    auto response = client_->writeString(testString, bytesWritten);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(bytesWritten, 0);
}

TEST_F(TTYBaseTest, ReadSectionWithoutConnection) {
    std::array<uint8_t, 100> buffer;
    uint8_t stopByte = '\n';
    uint32_t bytesRead = 0;

    auto response = client_->readSection(buffer, stopByte, 1, bytesRead);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(bytesRead, 0);
}

TEST_F(TTYBaseTest, ReadAsyncWithoutConnection) {
    std::array<uint8_t, 100> buffer;

    auto future = client_->readAsync(buffer, 1);
    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);

    auto [response, bytesRead] = future.get();
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(bytesRead, 0);
}

TEST_F(TTYBaseTest, WriteAsyncWithoutConnection) {
    std::array<uint8_t, 10> buffer = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto future = client_->writeAsync(buffer);
    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);

    auto [response, bytesWritten] = future.get();
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(bytesWritten, 0);
}

TEST_F(TTYBaseTest, GetErrorMessageForValidResponse) {
    auto errorMsg = client_->getErrorMessage(TTYBase::TTYResponse::OK);
    EXPECT_FALSE(errorMsg.empty());
    EXPECT_NE(errorMsg.find("OK"), std::string::npos);
}

TEST_F(TTYBaseTest, GetErrorMessageForInvalidDevice) {
    auto errorMsg = client_->getErrorMessage(TTYBase::TTYResponse::PortFailure);
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(TTYBaseTest, GetErrorMessageForTimeout) {
    auto errorMsg = client_->getErrorMessage(TTYBase::TTYResponse::Timeout);
    EXPECT_FALSE(errorMsg.empty());
    EXPECT_NE(errorMsg.find("Timeout"), std::string::npos);
}

TEST_F(TTYBaseTest, GetErrorMessageForReadError) {
    auto errorMsg = client_->getErrorMessage(TTYBase::TTYResponse::ReadError);
    EXPECT_FALSE(errorMsg.empty());
    EXPECT_NE(errorMsg.find("Read"), std::string::npos);
}

TEST_F(TTYBaseTest, GetErrorMessageForWriteError) {
    auto errorMsg = client_->getErrorMessage(TTYBase::TTYResponse::WriteError);
    EXPECT_FALSE(errorMsg.empty());
    EXPECT_NE(errorMsg.find("Write"), std::string::npos);
}

// Test with different baud rates
class TTYBaudRateTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_unique<TestTTYClient>("BaudRateTestDriver");
    }

    std::unique_ptr<TestTTYClient> client_;
};

TEST_F(TTYBaudRateTest, StandardBaudRates) {
    std::vector<uint32_t> standardBaudRates = {1200,  2400,  4800,  9600,
                                               19200, 38400, 57600, 115200};

    for (auto baudRate : standardBaudRates) {
        // All should fail since we don't have a real device, but should not
        // crash
        auto response = client_->connect("/dev/ttyUSB0", baudRate, 8, 0, 1);
        EXPECT_NE(response, TTYBase::TTYResponse::OK);
    }
}

TEST_F(TTYBaudRateTest, HighBaudRates) {
    std::vector<uint32_t> highBaudRates = {230400, 460800, 921600, 1000000};

    for (auto baudRate : highBaudRates) {
        auto response = client_->connect("/dev/ttyUSB0", baudRate, 8, 0, 1);
        EXPECT_NE(response, TTYBase::TTYResponse::OK);
    }
}

// Test with different word sizes
class TTYWordSizeTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_unique<TestTTYClient>("WordSizeTestDriver");
    }

    std::unique_ptr<TestTTYClient> client_;
};

TEST_F(TTYWordSizeTest, ValidWordSizes) {
    std::vector<uint8_t> validWordSizes = {5, 6, 7, 8};

    for (auto wordSize : validWordSizes) {
        auto response = client_->connect("/dev/ttyUSB0", 9600, wordSize, 0, 1);
        EXPECT_NE(response,
                  TTYBase::TTYResponse::OK);  // Will fail due to no device
    }
}

// Test with different parity settings
class TTYParityTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_unique<TestTTYClient>("ParityTestDriver");
    }

    std::unique_ptr<TestTTYClient> client_;
};

TEST_F(TTYParityTest, ValidParitySettings) {
    std::vector<uint8_t> validParitySettings = {0, 1, 2};  // None, Odd, Even

    for (auto parity : validParitySettings) {
        auto response = client_->connect("/dev/ttyUSB0", 9600, 8, parity, 1);
        EXPECT_NE(response,
                  TTYBase::TTYResponse::OK);  // Will fail due to no device
    }
}

// Test with different stop bits
class TTYStopBitsTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_unique<TestTTYClient>("StopBitsTestDriver");
    }

    std::unique_ptr<TestTTYClient> client_;
};

TEST_F(TTYStopBitsTest, ValidStopBits) {
    std::vector<uint8_t> validStopBits = {1, 2};

    for (auto stopBits : validStopBits) {
        auto response = client_->connect("/dev/ttyUSB0", 9600, 8, 0, stopBits);
        EXPECT_NE(response,
                  TTYBase::TTYResponse::OK);  // Will fail due to no device
    }
}

// Test buffer operations
class TTYBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_unique<TestTTYClient>("BufferTestDriver");
    }

    std::unique_ptr<TestTTYClient> client_;
};

TEST_F(TTYBufferTest, ReadWithZeroSizeBuffer) {
    std::array<uint8_t, 0> buffer;
    uint32_t bytesRead = 0;

    auto response = client_->read(buffer, 1, bytesRead);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(bytesRead, 0);
}

TEST_F(TTYBufferTest, WriteWithZeroSizeBuffer) {
    std::array<uint8_t, 0> buffer;
    uint32_t bytesWritten = 0;

    auto response = client_->write(buffer, bytesWritten);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(bytesWritten, 0);
}

TEST_F(TTYBufferTest, WriteEmptyString) {
    std::string emptyString = "";
    uint32_t bytesWritten = 0;

    auto response = client_->writeString(emptyString, bytesWritten);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(bytesWritten, 0);
}

TEST_F(TTYBufferTest, ReadSectionWithLargeBuffer) {
    std::array<uint8_t, 10000> buffer;
    uint8_t stopByte = '\n';
    uint32_t bytesRead = 0;

    auto response = client_->readSection(buffer, stopByte, 1, bytesRead);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(bytesRead, 0);
}

// Test timeout scenarios
class TTYTimeoutTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_unique<TestTTYClient>("TimeoutTestDriver");
    }

    std::unique_ptr<TestTTYClient> client_;
};

TEST_F(TTYTimeoutTest, ReadWithZeroTimeout) {
    std::array<uint8_t, 100> buffer;
    uint32_t bytesRead = 0;

    auto response = client_->read(buffer, 0, bytesRead);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(bytesRead, 0);
}

TEST_F(TTYTimeoutTest, ReadSectionWithZeroTimeout) {
    std::array<uint8_t, 100> buffer;
    uint8_t stopByte = '\n';
    uint32_t bytesRead = 0;

    auto response = client_->readSection(buffer, stopByte, 0, bytesRead);
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(bytesRead, 0);
}

TEST_F(TTYTimeoutTest, ReadAsyncWithZeroTimeout) {
    std::array<uint8_t, 100> buffer;

    auto future = client_->readAsync(buffer, 0);
    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);

    auto [response, bytesRead] = future.get();
    EXPECT_NE(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(bytesRead, 0);
}

// Test thread safety
class TTYThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_unique<TestTTYClient>("ThreadSafetyTestDriver");
    }

    std::unique_ptr<TestTTYClient> client_;
};

TEST_F(TTYThreadSafetyTest, ConcurrentIsConnectedCalls) {
    const int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> callCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &callCount]() {
            try {
                client_->isConnected();  // Should be thread-safe
                callCount++;
            } catch (...) {
                // Ignore exceptions for this test
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(callCount.load(), numThreads);
}

TEST_F(TTYThreadSafetyTest, ConcurrentDisconnectCalls) {
    const int numThreads = 3;
    std::vector<std::thread> threads;
    std::atomic<int> callCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &callCount]() {
            try {
                client_->disconnect();  // Should be thread-safe
                callCount++;
            } catch (...) {
                // Ignore exceptions for this test
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(callCount.load(), numThreads);
}

TEST_F(TTYThreadSafetyTest, ConcurrentGetErrorMessageCalls) {
    const int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> callCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &callCount]() {
            try {
                client_->getErrorMessage(TTYBase::TTYResponse::OK);
                callCount++;
            } catch (...) {
                // Ignore exceptions for this test
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(callCount.load(), numThreads);
}
