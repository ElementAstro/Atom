#include "atom/connection/fifoclient.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <future>
#include <thread>
#include "atom/connection/fifoserver.hpp"

using namespace atom::connection;

class FifoClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        fifo_path_ = "/tmp/test_fifo";
        server_ = std::make_unique<FIFOServer>(fifo_path_);
        ClientConfig config{};
        client_ = std::make_unique<FifoClient>(fifo_path_, config);
        server_->start();
    }

    void TearDown() override {
        server_->stop();
        client_.reset();
        server_.reset();
        std::filesystem::remove(fifo_path_);
    }

    std::string fifo_path_;
    std::unique_ptr<FIFOServer> server_;
    std::unique_ptr<FifoClient> client_;
};

TEST_F(FifoClientTest, ConnectToFifo) { ASSERT_TRUE(client_->isOpen()); }

TEST_F(FifoClientTest, WriteToFifo) {
    ASSERT_TRUE(client_->isOpen());

    std::string message = "Hello, FIFO!";
    ASSERT_TRUE(client_->write(message));
}

TEST_F(FifoClientTest, ReadFromFifo) {
    ASSERT_TRUE(client_->isOpen());

    std::string message = "Hello, FIFO!";
    server_->sendMessage(message);

    auto future = std::async(std::launch::async, [&]() -> atom::type::expected<std::string, std::error_code> {
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
    ASSERT_TRUE(client_->write(message, std::chrono::seconds(1)));

    auto future = std::async(std::launch::async, [&]() -> atom::type::expected<std::string, std::error_code> {
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

    auto future = std::async(std::launch::async, [&]() -> atom::type::expected<std::string, std::error_code> {
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
    EXPECT_TRUE(client_->write(emptyMessage));
}

TEST_F(FifoClientTest, WriteLargeData) {
    ASSERT_TRUE(client_->isOpen());

    // Create a large message (but within reasonable limits)
    std::string largeMessage(8192, 'X');
    largeMessage += "END_MARKER";

    EXPECT_TRUE(client_->write(largeMessage));
}

TEST_F(FifoClientTest, ReadWithZeroSize) {
    ASSERT_TRUE(client_->isOpen());

    std::string message = "Test message";
    server_->sendMessage(message);

    auto future = std::async(std::launch::async, [&]() -> atom::type::expected<std::string, std::error_code> {
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
            if (client_->write(message)) {
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

    auto future = std::async(std::launch::async, [&]() -> atom::type::expected<std::string, std::error_code> {
        return client_->read(100, std::chrono::milliseconds(1000));
    });

    auto status = future.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(status, std::future_status::ready);

    auto result = future.get();
    // Should timeout or return error
    EXPECT_FALSE(result.has_value());
}
