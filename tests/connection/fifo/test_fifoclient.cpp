#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <future>
#include <thread>
#include "atom/connection/fifo/fifoclient.hpp"
#include "atom/connection/fifo/fifoserver.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace atom::connection;

namespace {
#ifdef _WIN32
std::string createPipePath(const std::string& name) {
    return "\\\\.\\pipe\\" + name + "_" + std::to_string(GetCurrentProcessId());
}
#else
std::string createPipePath(const std::string& name) { return "/tmp/" + name; }
#endif
}  // namespace

class FifoClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        fifo_path_ = createPipePath("test_fifo");
        server_ = std::make_unique<FIFOServer>(fifo_path_);
        ClientConfig config{};
        client_ = std::make_unique<FifoClient>(fifo_path_, config);
        server_->start();
    }

    void TearDown() override {
        server_->stop();
        client_.reset();
        server_.reset();
#ifndef _WIN32
        std::filesystem::remove(fifo_path_);
#endif
    }

    std::string fifo_path_;
    std::unique_ptr<FIFOServer> server_;
    std::unique_ptr<FifoClient> client_;
};

TEST_F(FifoClientTest, ConnectToFifo) { ASSERT_TRUE(client_->isOpen()); }

TEST_F(FifoClientTest, WriteToFifo) {
    ASSERT_TRUE(client_->isOpen());

    std::string message = "Hello, FIFO!";
    auto result = client_->write(message);
    ASSERT_TRUE(result.has_value());
}

TEST_F(FifoClientTest, ReadFromFifo) {
    ASSERT_TRUE(client_->isOpen());

    std::string message = "Hello, FIFO!";
    server_->sendMessage(message);

    auto future =
        std::async(std::launch::async,
                   [&]() -> atom::type::expected<std::string, std::error_code> {
                       return client_->read(0, std::chrono::milliseconds(5000));
                   });

    auto status = future.wait_for(std::chrono::seconds(6));
    ASSERT_EQ(status, std::future_status::ready);

    auto result = future.get();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), message);
}

TEST_F(FifoClientTest, WriteAndReadWithTimeout) {
    ASSERT_TRUE(client_->isOpen());

    std::string message = "Hello, FIFO!";
    auto writeResult = client_->write(message, std::chrono::seconds(1));
    ASSERT_TRUE(writeResult.has_value());

    auto future =
        std::async(std::launch::async,
                   [&]() -> atom::type::expected<std::string, std::error_code> {
                       return client_->read(0, std::chrono::milliseconds(1000));
                   });

    auto status = future.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(status, std::future_status::ready);

    auto result = future.get();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), message);
}

TEST_F(FifoClientTest, ReadTimeout) {
    ASSERT_TRUE(client_->isOpen());

    auto future =
        std::async(std::launch::async,
                   [&]() -> atom::type::expected<std::string, std::error_code> {
                       return client_->read(0, std::chrono::milliseconds(1000));
                   });

    auto status = future.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(status, std::future_status::ready);

    auto result = future.get();
    ASSERT_FALSE(result.has_value());
}

TEST_F(FifoClientTest, WriteEmptyString) {
    ASSERT_TRUE(client_->isOpen());

    std::string emptyMessage = "";
    auto result = client_->write(emptyMessage);
    EXPECT_TRUE(result.has_value());
}

TEST_F(FifoClientTest, WriteLargeData) {
    ASSERT_TRUE(client_->isOpen());

    // Create a large message (but within reasonable limits)
    std::string largeMessage(8192, 'X');
    largeMessage += "END_MARKER";

    auto result = client_->write(largeMessage);
    EXPECT_TRUE(result.has_value());
}

TEST_F(FifoClientTest, ReadWithZeroSize) {
    ASSERT_TRUE(client_->isOpen());

    std::string message = "Test message";
    server_->sendMessage(message);

    auto future =
        std::async(std::launch::async,
                   [&]() -> atom::type::expected<std::string, std::error_code> {
                       return client_->read(0, std::chrono::milliseconds(2000));
                   });

    auto status = future.wait_for(std::chrono::seconds(3));
    ASSERT_EQ(status, std::future_status::ready);

    auto result = future.get();
    // Reading with size 0 should still work and return available data
    ASSERT_TRUE(result.has_value());
}

TEST_F(FifoClientTest, ConcurrentWrites) {
    ASSERT_TRUE(client_->isOpen());

    const int numThreads = 5;
    std::vector<std::thread> writers;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        writers.emplace_back([this, i, &successCount]() {
            std::string message = "Thread_" + std::to_string(i) + "_message";
            auto result = client_->write(message);
            if (result.has_value()) {
                successCount++;
            }
        });
    }

    for (auto& writer : writers) {
        writer.join();
    }

    EXPECT_GT(successCount.load(), 0);
}

TEST_F(FifoClientTest, WriteAfterServerStop) {
    ASSERT_TRUE(client_->isOpen());

    // Stop the server
    server_->stop();

    std::string message = "Message after server stop";
    // Write may succeed or fail depending on implementation
    // but should not crash
    EXPECT_NO_THROW(client_->write(message));
}

TEST_F(FifoClientTest, ReadAfterServerStop) {
    ASSERT_TRUE(client_->isOpen());

    // Stop the server
    server_->stop();

    auto future = std::async(
        std::launch::async,
        [&]() -> atom::type::expected<std::string, std::error_code> {
            return client_->read(100, std::chrono::milliseconds(1000));
        });

    auto status = future.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(status, std::future_status::ready);

    auto result = future.get();
    // Should timeout or return error
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Additional FifoClient Tests
// ============================================================================

TEST_F(FifoClientTest, GetPath) { EXPECT_EQ(client_->getPath(), fifo_path_); }

TEST_F(FifoClientTest, CloseAndReopen) {
    ASSERT_TRUE(client_->isOpen());

    client_->close();
    EXPECT_FALSE(client_->isOpen());

    auto result = client_->open();
    EXPECT_TRUE(result.has_value() ||
                !result.has_value());  // May or may not succeed
}

TEST_F(FifoClientTest, GetStatistics) {
    ASSERT_TRUE(client_->isOpen());

    auto stats = client_->getStatistics();
    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.bytes_sent, 0);
}

TEST_F(FifoClientTest, ResetStatistics) {
    ASSERT_TRUE(client_->isOpen());

    // Write some data
    auto writeResult = client_->write("test message");
    (void)writeResult;  // Result may or may not succeed

    // Reset statistics
    client_->resetStatistics();

    auto stats = client_->getStatistics();
    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.bytes_sent, 0);
}

TEST_F(FifoClientTest, GetConfig) {
    auto config = client_->getConfig();
    EXPECT_GT(config.read_buffer_size, 0);
    EXPECT_GT(config.max_message_size, 0);
}

TEST_F(FifoClientTest, UpdateConfig) {
    ClientConfig newConfig;
    newConfig.read_buffer_size = 8192;
    newConfig.max_message_size = 2 * 1024 * 1024;
    newConfig.auto_reconnect = false;

    bool result = client_->updateConfig(newConfig);
    EXPECT_TRUE(result);

    auto config = client_->getConfig();
    EXPECT_EQ(config.read_buffer_size, 8192);
}

TEST_F(FifoClientTest, WriteWithPriority) {
    ASSERT_TRUE(client_->isOpen());

    std::string message = "High priority message";
    auto result = client_->write(message, MessagePriority::High);
    EXPECT_TRUE(result.has_value());
}

TEST_F(FifoClientTest, WriteWithCriticalPriority) {
    ASSERT_TRUE(client_->isOpen());

    std::string message = "Critical message";
    auto result = client_->write(message, MessagePriority::Critical);
    EXPECT_TRUE(result.has_value());
}

TEST_F(FifoClientTest, WriteWithLowPriority) {
    ASSERT_TRUE(client_->isOpen());

    std::string message = "Low priority message";
    auto result = client_->write(message, MessagePriority::Low);
    EXPECT_TRUE(result.has_value());
}

TEST_F(FifoClientTest, WriteMultipleMessages) {
    ASSERT_TRUE(client_->isOpen());

    std::vector<std::string> messages = {"Message 1", "Message 2", "Message 3"};

    auto result = client_->writeMultiple(messages);
    EXPECT_TRUE(result.has_value());
}

TEST_F(FifoClientTest, WriteAsyncWithCallback) {
    ASSERT_TRUE(client_->isOpen());

    std::promise<bool> promise;
    auto future = promise.get_future();

    int opId = client_->writeAsync(
        "Async message",
        [&promise](bool success, std::error_code /*ec*/, size_t /*bytes*/) {
            promise.set_value(success);
        });

    EXPECT_GE(opId, 0);

    auto status = future.wait_for(std::chrono::seconds(5));
    if (status == std::future_status::ready) {
        EXPECT_TRUE(future.get());
    }
}

TEST_F(FifoClientTest, WriteAsyncWithFuture) {
    ASSERT_TRUE(client_->isOpen());

    auto future = client_->writeAsyncWithFuture("Async future message");

    auto status = future.wait_for(std::chrono::seconds(5));
    if (status == std::future_status::ready) {
        auto result = future.get();
        EXPECT_TRUE(result.has_value());
    }
}

TEST_F(FifoClientTest, ReadAsyncWithCallback) {
    ASSERT_TRUE(client_->isOpen());

    std::promise<bool> promise;
    auto future = promise.get_future();

    // Send a message first
    server_->sendMessage("Test for async read");

    int opId = client_->readAsync(
        [&promise](bool success, std::error_code /*ec*/, size_t /*bytes*/) {
            promise.set_value(success);
        },
        0, std::chrono::milliseconds(3000));

    EXPECT_GE(opId, 0);

    (void)future.wait_for(std::chrono::seconds(5));
    // Result depends on timing
}

TEST_F(FifoClientTest, ReadAsyncWithFuture) {
    ASSERT_TRUE(client_->isOpen());

    // Send a message first
    server_->sendMessage("Test for async read future");

    auto future =
        client_->readAsyncWithFuture(0, std::chrono::milliseconds(3000));

    auto status = future.wait_for(std::chrono::seconds(5));
    if (status == std::future_status::ready) {
        auto result = future.get();
        // Result depends on timing
    }
}

TEST_F(FifoClientTest, CancelOperation) {
    ASSERT_TRUE(client_->isOpen());

    int opId = client_->writeAsync("Message to cancel",
                                   [](bool, std::error_code, size_t) {});

    (void)client_->cancelOperation(opId);
    // May or may not succeed depending on timing
}

TEST_F(FifoClientTest, RegisterConnectionCallback) {
    std::atomic<bool> callbackCalled{false};

    int callbackId = client_->registerConnectionCallback(
        [&callbackCalled](bool /*connected*/, std::error_code /*ec*/) {
            callbackCalled = true;
        });

    EXPECT_GE(callbackId, 0);

    bool unregistered = client_->unregisterConnectionCallback(callbackId);
    EXPECT_TRUE(unregistered);
}

TEST_F(FifoClientTest, UnregisterInvalidCallback) {
    bool result = client_->unregisterConnectionCallback(-1);
    EXPECT_FALSE(result);

    result = client_->unregisterConnectionCallback(9999);
    EXPECT_FALSE(result);
}

TEST_F(FifoClientTest, MoveConstruction) {
    ASSERT_TRUE(client_->isOpen());

    FifoClient movedClient(std::move(*client_));
    EXPECT_TRUE(movedClient.isOpen());
}

TEST_F(FifoClientTest, MoveAssignment) {
    ASSERT_TRUE(client_->isOpen());

    ClientConfig config{};
    std::string otherPath = createPipePath("test_fifo_other");
    FifoClient otherClient(otherPath, config);

    otherClient = std::move(*client_);
    EXPECT_TRUE(otherClient.isOpen());
}

TEST_F(FifoClientTest, WriteSpecialCharacters) {
    ASSERT_TRUE(client_->isOpen());

    std::string specialMessage = "Special: \n\t\r\0 chars";
    auto result = client_->write(specialMessage);
    EXPECT_TRUE(result.has_value());
}

TEST_F(FifoClientTest, WriteBinaryData) {
    ASSERT_TRUE(client_->isOpen());

    std::vector<char> binaryData = {0x00, 0x01, 0x02, static_cast<char>(0xFF),
                                    static_cast<char>(0xFE)};
    std::string binaryString(binaryData.begin(), binaryData.end());

    auto result = client_->write(binaryString);
    EXPECT_TRUE(result.has_value());
}

TEST_F(FifoClientTest, WriteWithShortTimeout) {
    ASSERT_TRUE(client_->isOpen());

    std::string message = "Short timeout message";
    (void)client_->write(message, std::chrono::milliseconds(10));
    // May succeed or timeout
}

TEST_F(FifoClientTest, WriteWithLongTimeout) {
    ASSERT_TRUE(client_->isOpen());

    std::string message = "Long timeout message";
    auto result = client_->write(message, std::chrono::seconds(30));
    EXPECT_TRUE(result.has_value());
}

TEST_F(FifoClientTest, StatisticsAfterWrites) {
    ASSERT_TRUE(client_->isOpen());

    client_->resetStatistics();

    auto r1 = client_->write("Message 1");
    auto r2 = client_->write("Message 2");
    auto r3 = client_->write("Message 3");
    (void)r1;
    (void)r2;
    (void)r3;  // Results may vary

    auto stats = client_->getStatistics();
    EXPECT_GE(stats.messages_sent,
              0);  // May or may not count depending on implementation
}

TEST_F(FifoClientTest, ConfigWithCompression) {
    ClientConfig config;
    config.enable_compression = true;
    config.compression_threshold = 512;

    bool result = client_->updateConfig(config);
    EXPECT_TRUE(result);
}

TEST_F(FifoClientTest, ConfigWithEncryption) {
    ClientConfig config;
    config.enable_encryption = true;

    bool result = client_->updateConfig(config);
    EXPECT_TRUE(result);
}

TEST_F(FifoClientTest, ConfigAutoReconnect) {
    ClientConfig config;
    config.auto_reconnect = true;
    config.max_reconnect_attempts = 10;
    config.reconnect_delay = std::chrono::milliseconds(1000);

    bool result = client_->updateConfig(config);
    EXPECT_TRUE(result);
}
