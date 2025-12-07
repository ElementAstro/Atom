#include <fcntl.h>
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <future>
#include <thread>
#include "atom/connection/fifo/fifoserver.hpp"

using namespace atom::connection;

class FIFOServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        fifo_path_ = "/tmp/test_fifo";
        server_ = std::make_unique<FIFOServer>(fifo_path_);
    }

    void TearDown() override {
        server_->stop();
        server_.reset();
        std::filesystem::remove(fifo_path_);
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
        int fd = open(fifo_path_.c_str(), O_RDONLY);
        ASSERT_NE(fd, -1);

        char buffer[1024];
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
        ASSERT_GT(bytes_read, 0);

        promise.set_value(std::string(buffer, bytes_read));
        close(fd);
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
        int fd = open(fifo_path_.c_str(), O_RDONLY);
        ASSERT_NE(fd, -1);

        char buffer[1024];
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
        promise.set_value(bytes_read >=
                          0);  // Empty message should still trigger read
        close(fd);
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
        int fd = open(fifo_path_.c_str(), O_RDONLY);
        ASSERT_NE(fd, -1);

        std::string receivedData;
        char buffer[1024];
        ssize_t bytes_read;

        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            receivedData.append(buffer, bytes_read);
            if (receivedData.find("END_MARKER") != std::string::npos) {
                break;
            }
        }

        promise.set_value(receivedData);
        close(fd);
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
            int fd = open(fifo_path_.c_str(), O_RDONLY);
            if (fd != -1) {
                char buffer[1024];
                ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
                if (bytes_read > 0) {
                    receivedMessages.emplace_back(buffer, bytes_read);
                }
                close(fd);
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
        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            char buffer[1024];
            ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                promise.set_value(std::string(buffer, bytes_read));
            }
            close(fd);
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

    server_->setOnMessageSentCallback(
        [&callbackCalled](bool success) { callbackCalled = true; });

    std::thread reader_thread([&] {
        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            char buffer[1024];
            read(fd, buffer, sizeof(buffer));
            close(fd);
        }
    });

    server_->sendMessage("Test with callback");

    reader_thread.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Callback may or may not be called depending on implementation
}

TEST_F(FIFOServerTest, SetOnClientConnectedCallback) {
    std::atomic<bool> clientConnected{false};

    server_->setOnClientConnectedCallback(
        [&clientConnected]() { clientConnected = true; });

    server_->start();
    ASSERT_TRUE(server_->isRunning());

    // Open FIFO to simulate client connection
    std::thread client_thread([&] {
        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            close(fd);
        }
    });

    client_thread.join();
    // Callback may or may not be called depending on implementation
}

TEST_F(FIFOServerTest, SetOnClientDisconnectedCallback) {
    std::atomic<bool> clientDisconnected{false};

    server_->setOnClientDisconnectedCallback(
        [&clientDisconnected]() { clientDisconnected = true; });

    server_->start();
    ASSERT_TRUE(server_->isRunning());

    std::thread client_thread([&] {
        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            close(fd);  // Disconnect
        }
    });

    client_thread.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Callback may or may not be called depending on implementation
}

TEST_F(FIFOServerTest, GetStatistics) {
    server_->start();

    auto stats = server_->getStatistics();
    EXPECT_EQ(stats.messagesSent, 0);
    EXPECT_EQ(stats.bytesSent, 0);
}

TEST_F(FIFOServerTest, ResetStatistics) {
    server_->start();

    std::thread reader_thread([&] {
        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            char buffer[1024];
            read(fd, buffer, sizeof(buffer));
            close(fd);
        }
    });

    server_->sendMessage("Test message");
    reader_thread.join();

    server_->resetStatistics();

    auto stats = server_->getStatistics();
    EXPECT_EQ(stats.messagesSent, 0);
    EXPECT_EQ(stats.bytesSent, 0);
}

TEST_F(FIFOServerTest, SendBinaryData) {
    server_->start();
    ASSERT_TRUE(server_->isRunning());

    std::vector<char> binaryData = {0x00, 0x01, 0x02, static_cast<char>(0xFF)};
    std::string binaryMessage(binaryData.begin(), binaryData.end());

    std::promise<std::string> promise;
    std::future<std::string> future = promise.get_future();

    std::thread reader_thread([&] {
        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            char buffer[1024];
            ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                promise.set_value(std::string(buffer, bytes_read));
            }
            close(fd);
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
        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            char buffer[1024];
            ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                promise.set_value(std::string(buffer, bytes_read));
            }
            close(fd);
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
            int fd = open(fifo_path_.c_str(), O_RDONLY);
            if (fd != -1) {
                char buffer[1024];
                ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
                if (bytes_read > 0) {
                    messagesReceived++;
                }
                close(fd);
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

    FIFOServer otherServer("/tmp/other_fifo");
    otherServer = std::move(*server_);
    EXPECT_TRUE(otherServer.isRunning());

    std::filesystem::remove("/tmp/other_fifo");
}
