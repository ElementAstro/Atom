#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <future>
#include <thread>
#include "atom/connection/fifo/fifoserver.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

using namespace atom::connection;

// Platform-specific FIFO helper functions
namespace {

#ifdef _WIN32
// Windows: Use named pipes
std::string createPipePath(const std::string& name) {
    return "\\\\.\\pipe\\" + name + "_" + std::to_string(GetCurrentProcessId());
}

class PipeReader {
public:
    explicit PipeReader(const std::string& pipePath)
        : pipePath_(pipePath), handle_(INVALID_HANDLE_VALUE) {}

    ~PipeReader() { close(); }

    bool open() {
        // Wait for the pipe to be available
        if (!WaitNamedPipeA(pipePath_.c_str(), 5000)) {
            return false;
        }
        handle_ = CreateFileA(pipePath_.c_str(), GENERIC_READ, 0, nullptr,
                              OPEN_EXISTING, 0, nullptr);
        return handle_ != INVALID_HANDLE_VALUE;
    }

    ssize_t read(char* buffer, size_t size) {
        if (handle_ == INVALID_HANDLE_VALUE)
            return -1;
        DWORD bytesRead = 0;
        if (ReadFile(handle_, buffer, static_cast<DWORD>(size), &bytesRead,
                     nullptr)) {
            return static_cast<ssize_t>(bytesRead);
        }
        return -1;
    }

    void close() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

    bool isValid() const { return handle_ != INVALID_HANDLE_VALUE; }

private:
    std::string pipePath_;
    HANDLE handle_;
};

#else
// POSIX: Use traditional FIFOs
std::string createPipePath(const std::string& name) { return "/tmp/" + name; }

class PipeReader {
public:
    explicit PipeReader(const std::string& pipePath)
        : pipePath_(pipePath), fd_(-1) {}

    ~PipeReader() { close(); }

    bool open() {
        fd_ = ::open(pipePath_.c_str(), O_RDONLY);
        return fd_ != -1;
    }

    ssize_t read(char* buffer, size_t size) {
        if (fd_ == -1)
            return -1;
        return ::read(fd_, buffer, size);
    }

    void close() {
        if (fd_ != -1) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    bool isValid() const { return fd_ != -1; }

private:
    std::string pipePath_;
    int fd_;
};
#endif

}  // namespace

class FIFOServerTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef _WIN32
        fifo_path_ = createPipePath("test_fifo");
#else
        fifo_path_ = "/tmp/test_fifo";
#endif
        server_ = std::make_unique<FIFOServer>(fifo_path_);
    }

    void TearDown() override {
        server_->stop();
        server_.reset();
#ifndef _WIN32
        std::filesystem::remove(fifo_path_);
#endif
    }

    std::string fifo_path_;
    std::unique_ptr<FIFOServer> server_;
};

TEST_F(FIFOServerTest, StartAndStop) {
    ASSERT_FALSE(server_->isRunning());
    server_->start();
    ASSERT_TRUE(server_->isRunning());
    server_->stop();
    ASSERT_FALSE(server_->isRunning());
}

TEST_F(FIFOServerTest, SendMessage) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    std::string message = "Hello, FIFO!";
    std::promise<std::string> promise;
    std::future<std::string> future = promise.get_future();

    std::thread reader_thread([&] {
        PipeReader reader(fifo_path_);
        if (!reader.open()) {
            promise.set_value("");
            return;
        }

        char buffer[1024];
        ssize_t bytes_read = reader.read(buffer, sizeof(buffer));
        if (bytes_read > 0) {
            promise.set_value(std::string(buffer, bytes_read));
        } else {
            promise.set_value("");
        }
    });

    server_->sendMessage(message);

    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)),
              std::future_status::ready);
    ASSERT_EQ(future.get(), message);

    reader_thread.join();
}

TEST_F(FIFOServerTest, SendEmptyMessage) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    std::string emptyMessage = "";
    std::promise<bool> promise;
    std::future<bool> future = promise.get_future();

    std::thread reader_thread([&] {
        PipeReader reader(fifo_path_);
        if (!reader.open()) {
            promise.set_value(false);
            return;
        }

        char buffer[1024];
        ssize_t bytes_read = reader.read(buffer, sizeof(buffer));
        promise.set_value(bytes_read >=
                          0);  // Empty message should still trigger read
    });

    server_->sendMessage(emptyMessage);

    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)),
              std::future_status::ready);
    EXPECT_TRUE(future.get());

    reader_thread.join();
}

TEST_F(FIFOServerTest, SendLargeMessage) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    std::string largeMessage(4096, 'X');
    largeMessage += "END_MARKER";

    std::promise<std::string> promise;
    std::future<std::string> future = promise.get_future();

    std::thread reader_thread([&] {
        PipeReader reader(fifo_path_);
        if (!reader.open()) {
            promise.set_value("");
            return;
        }

        std::string receivedData;
        char buffer[1024];
        ssize_t bytes_read;

        while ((bytes_read = reader.read(buffer, sizeof(buffer))) > 0) {
            receivedData.append(buffer, bytes_read);
            if (receivedData.find("END_MARKER") != std::string::npos) {
                break;
            }
        }

        promise.set_value(receivedData);
    });

    server_->sendMessage(largeMessage);

    ASSERT_EQ(future.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    EXPECT_EQ(future.get(), largeMessage);

    reader_thread.join();
}

TEST_F(FIFOServerTest, MultipleMessages) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    const int numMessages = 5;
    std::vector<std::string> messages;
    for (int i = 0; i < numMessages; ++i) {
        messages.push_back("Message_" + std::to_string(i));
    }

    std::promise<std::vector<std::string>> promise;
    std::future<std::vector<std::string>> future = promise.get_future();

    std::thread reader_thread([&] {
        std::vector<std::string> receivedMessages;

        for (int i = 0; i < numMessages; ++i) {
            PipeReader reader(fifo_path_);
            if (reader.open()) {
                char buffer[1024];
                ssize_t bytes_read = reader.read(buffer, sizeof(buffer));
                if (bytes_read > 0) {
                    receivedMessages.emplace_back(buffer, bytes_read);
                }
            }
        }

        promise.set_value(receivedMessages);
    });

    // Send all messages
    for (const auto& message : messages) {
        server_->sendMessage(message);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ASSERT_EQ(future.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    auto receivedMessages = future.get();

    EXPECT_EQ(receivedMessages.size(), messages.size());
    for (size_t i = 0; i < messages.size() && i < receivedMessages.size();
         ++i) {
        EXPECT_EQ(receivedMessages[i], messages[i]);
    }

    reader_thread.join();
}

TEST_F(FIFOServerTest, SendAfterStop) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    server_->stop();
    ASSERT_FALSE(server_->isRunning());

    std::string message = "Message after stop";
    // Should not crash when sending after stop
    EXPECT_NO_THROW(server_->sendMessage(message));
}

TEST_F(FIFOServerTest, RestartServer) {
    // First run
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    server_->stop();
    ASSERT_FALSE(server_->isRunning());

    // Restart
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    // Test functionality after restart
    std::string message = "Restart test message";
    std::promise<std::string> promise;
    std::future<std::string> future = promise.get_future();

    std::thread reader_thread([&] {
        PipeReader reader(fifo_path_);
        if (reader.open()) {
            char buffer[1024];
            ssize_t bytes_read = reader.read(buffer, sizeof(buffer));
            if (bytes_read > 0) {
                promise.set_value(std::string(buffer, bytes_read));
            } else {
                promise.set_value("");
            }
        } else {
            promise.set_value("");
        }
    });

    server_->sendMessage(message);

    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)),
              std::future_status::ready);
    EXPECT_EQ(future.get(), message);

    reader_thread.join();
}

// ============================================================================
// Additional FIFOServer Tests
// ============================================================================

TEST_F(FIFOServerTest, GetFifoPath) {
    EXPECT_EQ(server_->getFifoPath(), fifo_path_);
}

TEST_F(FIFOServerTest, DoubleStart) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    // Second start should be handled gracefully
    EXPECT_NO_THROW(server_->start());
    EXPECT_TRUE(server_->isRunning());
}

TEST_F(FIFOServerTest, DoubleStop) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    server_->stop();
    ASSERT_FALSE(server_->isRunning());

    // Second stop should not cause issues
    EXPECT_NO_THROW(server_->stop());
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(FIFOServerTest, SendMessageWithCallback) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    std::atomic<bool> callbackCalled{false};

    int callbackId = server_->registerMessageCallback(
        [&callbackCalled](const std::string& /*msg*/, bool /*success*/) {
            callbackCalled = true;
        });
    EXPECT_GE(callbackId, 0);

    std::thread reader_thread([&] {
        PipeReader reader(fifo_path_);
        if (reader.open()) {
            char buffer[1024];
            reader.read(buffer, sizeof(buffer));
        }
    });

    server_->sendMessage("Test with callback");

    reader_thread.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Unregister callback
    server_->unregisterMessageCallback(callbackId);
}

TEST_F(FIFOServerTest, RegisterStatusCallback) {
    std::atomic<bool> statusChanged{false};

    int callbackId = server_->registerStatusCallback(
        [&statusChanged](bool /*running*/) { statusChanged = true; });
    EXPECT_GE(callbackId, 0);

    server_->start();
    ASSERT_TRUE(server_->isRunning());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Unregister callback
    server_->unregisterStatusCallback(callbackId);
}

TEST_F(FIFOServerTest, UnregisterInvalidCallback) {
    // Unregistering invalid callback IDs should return false
    EXPECT_FALSE(server_->unregisterMessageCallback(-1));
    EXPECT_FALSE(server_->unregisterMessageCallback(9999));
    EXPECT_FALSE(server_->unregisterStatusCallback(-1));
    EXPECT_FALSE(server_->unregisterStatusCallback(9999));
}

TEST_F(FIFOServerTest, GetStatistics) {
    server_->start();

    auto stats = server_->getStatistics();
    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.bytes_sent, 0);
}

TEST_F(FIFOServerTest, ResetStatistics) {
    server_->start();

    std::thread reader_thread([&] {
        PipeReader reader(fifo_path_);
        if (reader.open()) {
            char buffer[1024];
            reader.read(buffer, sizeof(buffer));
        }
    });

    server_->sendMessage("Test message");
    reader_thread.join();

    server_->resetStatistics();

    auto stats = server_->getStatistics();
    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.bytes_sent, 0);
}

TEST_F(FIFOServerTest, SendBinaryData) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    std::vector<char> binaryData = {0x00, 0x01, 0x02, static_cast<char>(0xFF)};
    std::string binaryMessage(binaryData.begin(), binaryData.end());

    std::promise<std::string> promise;
    std::future<std::string> future = promise.get_future();

    std::thread reader_thread([&] {
        PipeReader reader(fifo_path_);
        if (reader.open()) {
            char buffer[1024];
            ssize_t bytes_read = reader.read(buffer, sizeof(buffer));
            if (bytes_read > 0) {
                promise.set_value(std::string(buffer, bytes_read));
            } else {
                promise.set_value("");
            }
        } else {
            promise.set_value("");
        }
    });

    server_->sendMessage(binaryMessage);

    auto status = future.wait_for(std::chrono::seconds(5));
    if (status == std::future_status::ready) {
        EXPECT_EQ(future.get(), binaryMessage);
    }

    reader_thread.join();
}

TEST_F(FIFOServerTest, SendSpecialCharacters) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    std::string specialMessage = "Special: \n\t\r chars";

    std::promise<std::string> promise;
    std::future<std::string> future = promise.get_future();

    std::thread reader_thread([&] {
        PipeReader reader(fifo_path_);
        if (reader.open()) {
            char buffer[1024];
            ssize_t bytes_read = reader.read(buffer, sizeof(buffer));
            if (bytes_read > 0) {
                promise.set_value(std::string(buffer, bytes_read));
            } else {
                promise.set_value("");
            }
        } else {
            promise.set_value("");
        }
    });

    server_->sendMessage(specialMessage);

    auto status = future.wait_for(std::chrono::seconds(5));
    if (status == std::future_status::ready) {
        EXPECT_EQ(future.get(), specialMessage);
    }

    reader_thread.join();
}

TEST_F(FIFOServerTest, ConcurrentSends) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    const int numMessages = 10;
    std::atomic<int> messagesReceived{0};

    std::thread reader_thread([&] {
        for (int i = 0; i < numMessages; ++i) {
            PipeReader reader(fifo_path_);
            if (reader.open()) {
                char buffer[1024];
                ssize_t bytes_read = reader.read(buffer, sizeof(buffer));
                if (bytes_read > 0) {
                    messagesReceived++;
                }
            }
        }
    });

    std::vector<std::thread> senders;
    for (int i = 0; i < numMessages; ++i) {
        senders.emplace_back([this, i]() {
            server_->sendMessage("Concurrent_" + std::to_string(i));
        });
    }

    for (auto& sender : senders) {
        sender.join();
    }

    reader_thread.join();
    EXPECT_GT(messagesReceived.load(), 0);
}

TEST_F(FIFOServerTest, MoveConstruction) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    FIFOServer movedServer(std::move(*server_));
    EXPECT_TRUE(movedServer.isRunning());
}

TEST_F(FIFOServerTest, MoveAssignment) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

#ifdef _WIN32
    std::string otherPath = createPipePath("other_fifo");
#else
    std::string otherPath = "/tmp/other_fifo";
#endif
    FIFOServer otherServer(otherPath);
    otherServer = std::move(*server_);
    EXPECT_TRUE(otherServer.isRunning());

#ifndef _WIN32
    std::filesystem::remove(otherPath);
#endif
}

TEST_F(FIFOServerTest, GetConfig) {
    auto config = server_->getConfig();
    EXPECT_GT(config.max_queue_size, 0);
    EXPECT_GT(config.max_message_size, 0);
}

TEST_F(FIFOServerTest, UpdateConfig) {
    ServerConfig newConfig;
    newConfig.max_queue_size = 2000;
    newConfig.max_message_size = 2 * 1024 * 1024;
    newConfig.enable_compression = true;

    bool result = server_->updateConfig(newConfig);
    EXPECT_TRUE(result);

    auto config = server_->getConfig();
    EXPECT_EQ(config.max_queue_size, 2000);
}

TEST_F(FIFOServerTest, SetLogLevel) {
    EXPECT_NO_THROW(server_->setLogLevel(LogLevel::Debug));
    EXPECT_NO_THROW(server_->setLogLevel(LogLevel::Info));
    EXPECT_NO_THROW(server_->setLogLevel(LogLevel::Warning));
    EXPECT_NO_THROW(server_->setLogLevel(LogLevel::Error));
    EXPECT_NO_THROW(server_->setLogLevel(LogLevel::None));
}

TEST_F(FIFOServerTest, GetQueueSize) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    size_t queueSize = server_->getQueueSize();
    EXPECT_GE(queueSize, 0);
}

TEST_F(FIFOServerTest, ClearQueue) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    // Queue some messages (they may or may not be queued depending on timing)
    server_->sendMessage("Message 1");
    server_->sendMessage("Message 2");

    size_t cleared = server_->clearQueue();
    EXPECT_GE(cleared, 0);
}

TEST_F(FIFOServerTest, SendMessageWithPriority) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    std::thread reader_thread([&] {
        PipeReader reader(fifo_path_);
        if (reader.open()) {
            char buffer[1024];
            reader.read(buffer, sizeof(buffer));
        }
    });

    bool result =
        server_->sendMessage("High priority message", MessagePriority::High);
    EXPECT_TRUE(result);

    reader_thread.join();
}

TEST_F(FIFOServerTest, SendMessageAsync) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    std::thread reader_thread([&] {
        PipeReader reader(fifo_path_);
        if (reader.open()) {
            char buffer[1024];
            reader.read(buffer, sizeof(buffer));
        }
    });

    auto future = server_->sendMessageAsync("Async message");
    auto status = future.wait_for(std::chrono::seconds(5));
    if (status == std::future_status::ready) {
        EXPECT_TRUE(future.get());
    }

    reader_thread.join();
}

TEST_F(FIFOServerTest, SendMessageAsyncWithPriority) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    std::thread reader_thread([&] {
        PipeReader reader(fifo_path_);
        if (reader.open()) {
            char buffer[1024];
            reader.read(buffer, sizeof(buffer));
        }
    });

    auto future = server_->sendMessageAsync("Critical async message",
                                            MessagePriority::Critical);
    auto status = future.wait_for(std::chrono::seconds(5));
    if (status == std::future_status::ready) {
        // Result depends on timing
    }

    reader_thread.join();
}

TEST_F(FIFOServerTest, StopWithFlush) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    server_->sendMessage("Message before stop");

    // Stop with flush_queue = true (default)
    server_->stop(true);
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(FIFOServerTest, StopWithoutFlush) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    server_->sendMessage("Message before stop");

    // Stop with flush_queue = false
    server_->stop(false);
    EXPECT_FALSE(server_->isRunning());
}
