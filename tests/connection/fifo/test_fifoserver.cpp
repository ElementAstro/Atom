#include "atom/connection/fifoserver.hpp"
#include <fcntl.h>
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <future>
#include <thread>

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
        promise.set_value(bytes_read >= 0);  // Empty message should still trigger read
        close(fd);
    });

    server_->sendMessage(emptyMessage);

    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
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

    ASSERT_EQ(future.wait_for(std::chrono::seconds(10)), std::future_status::ready);
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

    ASSERT_EQ(future.wait_for(std::chrono::seconds(10)), std::future_status::ready);
    auto receivedMessages = future.get();

    EXPECT_EQ(receivedMessages.size(), messages.size());
    for (size_t i = 0; i < messages.size() && i < receivedMessages.size(); ++i) {
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

    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_EQ(future.get(), message);

    reader_thread.join();
}
