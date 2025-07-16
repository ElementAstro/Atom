#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "atom/connection/fifoserver.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace atom::connection::test {

// Helper function to generate a unique FIFO path
std::string generateUniqueFifoPath() {
    auto now =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return (std::filesystem::temp_directory_path() /
            ("test_fifo_server_" + std::to_string(now)))
        .string();
}

// Mock classes for callbacks
class MockMessageCallback {
public:
    MOCK_METHOD(void, call, (const std::string& message, bool success), ());
};

class MockStatusCallback {
public:
    MOCK_METHOD(void, call, (bool connected), ());
};

// Test fixture for FIFOServer
class FIFOServerTest : public ::testing::Test {
protected:
    std::string fifo_path_;
    std::unique_ptr<FIFOServer> server_;

    void SetUp() override {
        fifo_path_ = generateUniqueFifoPath();
        // Server is created in individual tests to allow for config variations
    }

    void TearDown() override {
        if (server_) {
            server_->stop(
                false);  // Ensure server is stopped before destruction
            server_.reset();
        }
        // Clean up FIFO file
#ifndef _WIN32
        std::filesystem::remove(fifo_path_);
#endif
    }

    // Helper to simulate a client reading from the FIFO
    std::string clientRead(
        size_t max_size,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
#ifdef _WIN32
        HANDLE pipe = CreateFileA(fifo_path_.c_str(), GENERIC_READ, 0, NULL,
                                  OPEN_EXISTING, 0, NULL);
        if (pipe == INVALID_HANDLE_VALUE) {
            return "";  // Pipe not open yet or error
        }

        std::vector<char> buffer(max_size);
        DWORD bytes_read = 0;
        BOOL success =
            ReadFile(pipe, buffer.data(), static_cast<DWORD>(max_size),
                     &bytes_read, NULL);
        CloseHandle(pipe);
        if (success && bytes_read > 0) {
            return std::string(buffer.data(), bytes_read);
        }
        return "";
#else
        int fd = open(fifo_path_.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd == -1) {
            return "";  // FIFO not open yet or error
        }

        std::vector<char> buffer(max_size);
        auto start = std::chrono::steady_clock::now();
        ssize_t bytes_read = -1;

        while (std::chrono::steady_clock::now() - start < timeout) {
            bytes_read = read(fd, buffer.data(), max_size);
            if (bytes_read > 0) {
                break;
            } else if (bytes_read == -1 &&
                       (errno == EAGAIN || errno == EWOULDBLOCK)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                break;  // Error or EOF
            }
        }
        close(fd);
        if (bytes_read > 0) {
            return std::string(buffer.data(), bytes_read);
        }
        return "";
#endif
    }
};

// Test Cases

// Constructor and Destructor
TEST_F(FIFOServerTest, ConstructorDefaultConfig) {
    EXPECT_NO_THROW(server_ = std::make_unique<FIFOServer>(fifo_path_));
    ASSERT_NE(server_, nullptr);
    EXPECT_EQ(server_->getFifoPath(), fifo_path_);
    EXPECT_FALSE(server_->isRunning());
    EXPECT_EQ(server_->getConfig().max_queue_size, 1000);
}

TEST_F(FIFOServerTest, ConstructorCustomConfig) {
    ServerConfig config;
    config.max_queue_size = 500;
    config.log_level = LogLevel::Debug;
    EXPECT_NO_THROW(server_ = std::make_unique<FIFOServer>(fifo_path_, config));
    ASSERT_NE(server_, nullptr);
    EXPECT_EQ(server_->getConfig().max_queue_size, 500);
    EXPECT_EQ(server_->getConfig().log_level, LogLevel::Debug);
}

TEST_F(FIFOServerTest, ConstructorEmptyPathThrows) {
    EXPECT_THROW(server_ = std::make_unique<FIFOServer>(""),
                 std::invalid_argument);
}

// Start/Stop
TEST_F(FIFOServerTest, StartAndStop) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    MockStatusCallback mock_status_cb;
    EXPECT_CALL(mock_status_cb, call(true)).Times(1);
    EXPECT_CALL(mock_status_cb, call(false)).Times(1);
    server_->registerStatusCallback(std::bind(
        &MockStatusCallback::call, &mock_status_cb, std::placeholders::_1));

    server_->start();
    EXPECT_TRUE(server_->isRunning());

    server_->stop();
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(FIFOServerTest, StopFlushesQueue) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();

    // Open client side to allow server to write
#ifndef _WIN32
    int client_fd = open(fifo_path_.c_str(), O_RDONLY | O_NONBLOCK);
    ASSERT_NE(client_fd, -1);
#endif

    server_->sendMessage("Message 1");
    server_->sendMessage("Message 2");
    server_->sendMessage("Message 3");

    EXPECT_EQ(server_->getQueueSize(), 3);

    server_->stop(true);  // Flush queue

    EXPECT_FALSE(server_->isRunning());
    EXPECT_EQ(server_->getQueueSize(), 0);

    // Verify messages were written (or at least attempted)
    // This is tricky to verify reliably without a full client simulation
    // but we can check if the queue is empty.
#ifndef _WIN32
    close(client_fd);
#endif
}

TEST_F(FIFOServerTest, StopDoesNotFlushQueue) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();

    server_->sendMessage("Message 1");
    server_->sendMessage("Message 2");

    EXPECT_EQ(server_->getQueueSize(), 2);

    server_->stop(false);  // Do not flush queue

    EXPECT_FALSE(server_->isRunning());
    EXPECT_EQ(server_->getQueueSize(), 2);  // Messages should still be in queue
}

TEST_F(FIFOServerTest, StartAlreadyRunning) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();
    EXPECT_TRUE(server_->isRunning());
    EXPECT_NO_THROW(server_->start());  // Should not throw, just log a warning
    EXPECT_TRUE(server_->isRunning());
}

// SendMessage (synchronous)
TEST_F(FIFOServerTest, SendSingleMessage) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();

    std::string test_message = "Hello, FIFO!";
    bool sent = server_->sendMessage(test_message);
    EXPECT_TRUE(sent);
    EXPECT_EQ(server_->getQueueSize(), 1);

    // Simulate client reading
    std::string received_message = clientRead(test_message.size() + 1);
    EXPECT_EQ(received_message, test_message);  // Server doesn't add newline

    // Give some time for the message to be processed by the server's internal
    // thread
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(server_->getQueueSize(), 0);
    EXPECT_EQ(server_->getStatistics().messages_sent, 1);
    EXPECT_EQ(server_->getStatistics().bytes_sent, test_message.size());
}

TEST_F(FIFOServerTest, SendMessageWithPriority) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();

    server_->sendMessage("Low Priority", MessagePriority::Low);
    server_->sendMessage("High Priority", MessagePriority::High);
    server_->sendMessage("Normal Priority", MessagePriority::Normal);

    EXPECT_EQ(server_->getQueueSize(), 3);

    // Simulate client reading to drain the queue
    std::string msg1 = clientRead(100);
    std::string msg2 = clientRead(100);
    std::string msg3 = clientRead(100);

    // Due to priority queue, High should come first, then Normal, then Low
    // This is hard to test reliably with a simple clientRead, as the server's
    // internal thread processes messages. We can only verify the queue drains.
    EXPECT_EQ(server_->getQueueSize(), 0);
    EXPECT_EQ(server_->getStatistics().messages_sent, 3);
}

TEST_F(FIFOServerTest, SendMessageWhenNotRunning) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);  // Not started
    bool sent = server_->sendMessage("Test");
    EXPECT_FALSE(sent);
    EXPECT_EQ(server_->getQueueSize(), 0);
    EXPECT_EQ(server_->getStatistics().messages_failed,
              0);  // Should not increment failed if not running
}

TEST_F(FIFOServerTest, SendEmptyMessage) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();
    bool sent = server_->sendMessage("");
    EXPECT_FALSE(sent);
    EXPECT_EQ(server_->getQueueSize(), 0);
    EXPECT_EQ(server_->getStatistics().messages_failed,
              0);  // Should not increment failed for empty message
}

TEST_F(FIFOServerTest, SendMessageTooLarge) {
    ServerConfig config;
    config.max_message_size = 10;  // Small limit
    server_ = std::make_unique<FIFOServer>(fifo_path_, config);
    server_->start();

    std::string large_message = "This message is too long.";  // > 10 bytes
    bool sent = server_->sendMessage(large_message);
    EXPECT_FALSE(sent);
    EXPECT_EQ(server_->getQueueSize(), 0);
    EXPECT_EQ(server_->getStatistics().messages_failed, 1);
}

TEST_F(FIFOServerTest, SendMessageQueueFull) {
    ServerConfig config;
    config.max_queue_size = 1;  // Small queue
    server_ = std::make_unique<FIFOServer>(fifo_path_, config);
    server_->start();

    bool sent1 = server_->sendMessage("Message 1");
    EXPECT_TRUE(sent1);
    EXPECT_EQ(server_->getQueueSize(), 1);

    bool sent2 = server_->sendMessage("Message 2");  // Should fail
    EXPECT_FALSE(sent2);
    EXPECT_EQ(server_->getQueueSize(), 1);  // Still 1 message
    EXPECT_EQ(server_->getStatistics().messages_failed, 1);
}

// SendMessage (Messageable concept)
TEST_F(FIFOServerTest, SendMessageInt) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();
    bool sent = server_->sendMessage(123);
    EXPECT_TRUE(sent);
    std::string received = clientRead(10);
    EXPECT_EQ(received, "123");
}

TEST_F(FIFOServerTest, SendMessageDouble) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();
    bool sent = server_->sendMessage(3.14);
    EXPECT_TRUE(sent);
    std::string received = clientRead(10);
    // std::to_string for double might have precision issues, check prefix
    EXPECT_TRUE(received.rfind("3.14", 0) == 0);
}

// SendMessageAsync
TEST_F(FIFOServerTest, SendMessageAsync) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();

    std::string test_message = "Async message.";
    std::future<bool> future_result = server_->sendMessageAsync(test_message);

    // Check if the future is ready within a reasonable time
    auto status = future_result.wait_for(std::chrono::milliseconds(100));
    EXPECT_EQ(status, std::future_status::ready);

    bool sent = future_result.get();
    EXPECT_TRUE(sent);
    EXPECT_EQ(server_->getQueueSize(),
              1);  // Message should be queued immediately

    // Allow time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(server_->getQueueSize(), 0);
    EXPECT_EQ(server_->getStatistics().messages_sent, 1);
}

TEST_F(FIFOServerTest, SendMessageAsyncWhenNotRunning) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);  // Not started
    std::future<bool> future_result = server_->sendMessageAsync("Test");
    auto status = future_result.wait_for(std::chrono::milliseconds(100));
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_FALSE(future_result.get());
}

// SendMessages (range-based)
TEST_F(FIFOServerTest, SendMultipleMessages) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();

    std::vector<std::string> messages = {"Msg1", "Msg2", "Msg3"};
    size_t queued_count = server_->sendMessages(messages);
    EXPECT_EQ(queued_count, 3);
    EXPECT_EQ(server_->getQueueSize(), 3);

    // Allow time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(server_->getQueueSize(), 0);
    EXPECT_EQ(server_->getStatistics().messages_sent, 3);
}

TEST_F(FIFOServerTest, SendMultipleMessagesWithPriority) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();

    std::vector<std::string> messages = {"MsgA", "MsgB"};
    size_t queued_count =
        server_->sendMessages(messages, MessagePriority::Critical);
    EXPECT_EQ(queued_count, 2);
    EXPECT_EQ(server_->getQueueSize(), 2);
}

TEST_F(FIFOServerTest, SendMultipleMessagesSomeTooLarge) {
    ServerConfig config;
    config.max_message_size = 5;  // Small limit
    server_ = std::make_unique<FIFOServer>(fifo_path_, config);
    server_->start();

    std::vector<std::string> messages = {"Short", "TooLongMessage",
                                         "AlsoShort"};
    size_t queued_count = server_->sendMessages(messages);
    EXPECT_EQ(queued_count, 2);  // "Short" and "AlsoShort"
    EXPECT_EQ(server_->getQueueSize(), 2);
    EXPECT_EQ(server_->getStatistics().messages_failed, 1);  // "TooLongMessage"
}

TEST_F(FIFOServerTest, SendMultipleMessagesQueueFull) {
    ServerConfig config;
    config.max_queue_size = 1;
    server_ = std::make_unique<FIFOServer>(fifo_path_, config);
    server_->start();

    std::vector<std::string> messages = {"Msg1", "Msg2"};
    size_t queued_count = server_->sendMessages(messages);
    EXPECT_EQ(queued_count, 1);  // Only Msg1 should be queued
    EXPECT_EQ(server_->getQueueSize(), 1);
    EXPECT_EQ(server_->getStatistics().messages_failed, 1);  // Msg2 failed
}

// Callbacks
TEST_F(FIFOServerTest, RegisterAndUnregisterMessageCallback) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    MockMessageCallback mock_msg_cb;

    int id1 = server_->registerMessageCallback(
        std::bind(&MockMessageCallback::call, &mock_msg_cb,
                  std::placeholders::_1, std::placeholders::_2));
    EXPECT_NE(id1, -1);

    int id2 = server_->registerMessageCallback(
        std::bind(&MockMessageCallback::call, &mock_msg_cb,
                  std::placeholders::_1, std::placeholders::_2));
    EXPECT_NE(id2, -1);
    EXPECT_NE(id1, id2);  // IDs should be unique

    EXPECT_TRUE(server_->unregisterMessageCallback(id1));
    EXPECT_FALSE(server_->unregisterMessageCallback(999));  // Non-existent ID
    EXPECT_FALSE(
        server_->unregisterMessageCallback(id1));  // Already unregistered
}

TEST_F(FIFOServerTest, MessageCallbackInvokedOnSend) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();

    MockMessageCallback mock_msg_cb;
    EXPECT_CALL(mock_msg_cb, call("Test Message", true)).Times(1);
    server_->registerMessageCallback(
        std::bind(&MockMessageCallback::call, &mock_msg_cb,
                  std::placeholders::_1, std::placeholders::_2));

    server_->sendMessage("Test Message");
    std::this_thread::sleep_for(
        std::chrono::milliseconds(200));  // Give time for async processing
}

TEST_F(FIFOServerTest, MessageCallbackInvokedOnFailedSend) {
    ServerConfig config;
    config.max_message_size = 1;
    server_ = std::make_unique<FIFOServer>(fifo_path_, config);
    server_->start();

    MockMessageCallback mock_msg_cb;
    EXPECT_CALL(mock_msg_cb, call("Too long", false)).Times(1);
    server_->registerMessageCallback(
        std::bind(&MockMessageCallback::call, &mock_msg_cb,
                  std::placeholders::_1, std::placeholders::_2));

    server_->sendMessage("Too long");
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Give time for async processing
}

TEST_F(FIFOServerTest, RegisterAndUnregisterStatusCallback) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    MockStatusCallback mock_status_cb;

    int id1 = server_->registerStatusCallback(std::bind(
        &MockStatusCallback::call, &mock_status_cb, std::placeholders::_1));
    EXPECT_NE(id1, -1);

    EXPECT_TRUE(server_->unregisterStatusCallback(id1));
}

TEST_F(FIFOServerTest, StatusCallbackInvokedOnConnectDisconnect) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    MockStatusCallback mock_status_cb;
    EXPECT_CALL(mock_status_cb, call(true)).Times(1);
    EXPECT_CALL(mock_status_cb, call(false)).Times(1);
    server_->registerStatusCallback(std::bind(
        &MockStatusCallback::call, &mock_status_cb, std::placeholders::_1));

    server_->start();
    // Simulate client opening the FIFO to trigger connection
#ifndef _WIN32
    int client_fd = open(fifo_path_.c_str(), O_RDONLY | O_NONBLOCK);
    ASSERT_NE(client_fd, -1);
    std::this_thread::sleep_for(std::chrono::milliseconds(
        100));  // Give time for connection to register
    close(client_fd);
#endif
    server_->stop();
}

// Configuration and Statistics
TEST_F(FIFOServerTest, GetConfig) {
    ServerConfig custom_config;
    custom_config.max_queue_size = 777;
    custom_config.enable_compression = true;
    server_ = std::make_unique<FIFOServer>(fifo_path_, custom_config);

    ServerConfig retrieved_config = server_->getConfig();
    EXPECT_EQ(retrieved_config.max_queue_size, 777);
    EXPECT_TRUE(retrieved_config.enable_compression);
}

TEST_F(FIFOServerTest, UpdateConfig) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    ServerConfig initial_config = server_->getConfig();
    EXPECT_EQ(initial_config.max_queue_size, 1000);

    ServerConfig new_config = initial_config;
    new_config.max_queue_size = 2000;  // Increase
    new_config.log_level = LogLevel::Error;
    new_config.max_message_size = 500;

    EXPECT_TRUE(server_->updateConfig(new_config));
    ServerConfig updated_config = server_->getConfig();
    EXPECT_EQ(updated_config.max_queue_size, 2000);
    EXPECT_EQ(updated_config.log_level, LogLevel::Error);
    EXPECT_EQ(updated_config.max_message_size, 500);

    // Test decreasing max_queue_size (should be ignored)
    ServerConfig smaller_queue_config = updated_config;
    smaller_queue_config.max_queue_size = 100;
    EXPECT_TRUE(server_->updateConfig(smaller_queue_config));
    EXPECT_EQ(server_->getConfig().max_queue_size, 2000);  // Should remain 2000
}

TEST_F(FIFOServerTest, GetAndResetStatistics) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();

    server_->sendMessage("Msg1");
    server_->sendMessage("Msg2");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ServerStats stats = server_->getStatistics();
    EXPECT_EQ(stats.messages_sent, 2);
    EXPECT_EQ(stats.bytes_sent,
              std::string("Msg1").size() + std::string("Msg2").size());
    EXPECT_GT(stats.avg_message_size, 0);
    EXPECT_GT(stats.avg_latency_ms, 0);
    EXPECT_EQ(stats.current_queue_size, 0);
    EXPECT_GT(stats.queue_high_watermark, 0);

    server_->resetStatistics();
    ServerStats reset_stats = server_->getStatistics();
    EXPECT_EQ(reset_stats.messages_sent, 0);
    EXPECT_EQ(reset_stats.bytes_sent, 0);
    EXPECT_EQ(reset_stats.avg_message_size, 0);
    EXPECT_EQ(reset_stats.avg_latency_ms, 0);
    EXPECT_EQ(reset_stats.current_queue_size,
              0);  // Queue should be empty after processing
    EXPECT_EQ(reset_stats.queue_high_watermark, 0);
}

TEST_F(FIFOServerTest, SetLogLevel) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->setLogLevel(LogLevel::Warning);
    EXPECT_EQ(server_->getConfig().log_level, LogLevel::Warning);
}

TEST_F(FIFOServerTest, ClearQueue) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();

    server_->sendMessage("Msg1");
    server_->sendMessage("Msg2");
    server_->sendMessage("Msg3");
    EXPECT_EQ(server_->getQueueSize(), 3);

    size_t cleared_count = server_->clearQueue();
    EXPECT_EQ(cleared_count, 3);
    EXPECT_EQ(server_->getQueueSize(), 0);

    // Ensure no messages are sent after clearing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(server_->getStatistics().messages_sent, 0);
}

// Move operations
TEST_F(FIFOServerTest, MoveConstructor) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();
    server_->sendMessage("Original Message");

    FIFOServer moved_server = std::move(*server_);  // Move construction
    server_.reset();  // Original unique_ptr is now null

    EXPECT_TRUE(moved_server.isRunning());
    EXPECT_EQ(moved_server.getFifoPath(), fifo_path_);
    EXPECT_EQ(moved_server.getQueueSize(), 1);

    // Ensure the moved-from object is in a valid but unspecified state
    // (e.g., its internal impl_ pointer is null or points to a
    // default-constructed Impl) We can't directly check server_->impl_ after
    // reset, but we can check its methods if it were still a valid object. For
    // unique_ptr, it's simply null.
}

TEST_F(FIFOServerTest, MoveAssignment) {
    server_ = std::make_unique<FIFOServer>(fifo_path_);
    server_->start();
    server_->sendMessage("Original Message");

    std::string new_fifo_path = generateUniqueFifoPath();
    FIFOServer other_server(new_fifo_path);  // Create another server
    other_server.start();
    other_server.sendMessage("Other Message");

    other_server = std::move(*server_);  // Move assignment
    server_.reset();

    EXPECT_TRUE(other_server.isRunning());
    EXPECT_EQ(other_server.getFifoPath(),
              fifo_path_);  // Should now have original server's path
    EXPECT_EQ(other_server.getQueueSize(),
              1);  // Should have original server's message

    // The server created with new_fifo_path should have been properly shut down
    // and its resources released by the move assignment.
}

// Message TTL
TEST_F(FIFOServerTest, MessageTTL) {
    ServerConfig config;
    config.message_ttl = std::chrono::milliseconds(50);  // Short TTL
    server_ = std::make_unique<FIFOServer>(fifo_path_, config);
    server_->start();

    server_->sendMessage("Message 1 (expired)");
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Wait for TTL to pass
    server_->sendMessage(
        "Message 2 (fresh)");  // This message should be processed

    EXPECT_EQ(server_->getQueueSize(),
              2);  // Both messages are initially queued

    // Allow server to process messages
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(server_->getQueueSize(), 0);
    EXPECT_EQ(server_->getStatistics().messages_sent,
              1);  // Only "Message 2" should be sent
    EXPECT_EQ(server_->getStatistics().messages_failed,
              1);  // "Message 1" should have expired
}

}  // namespace atom::connection::test
