#include "atom/connection/async_fifoserver.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <future>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

using namespace atom::async::connection;
using namespace std::chrono_literals;

class AsyncFifoServerTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef _WIN32
        fifo_path_ = "\\\\.\\pipe\\test_async_fifo_" + std::to_string(GetCurrentProcessId());
#else
        fifo_path_ = "/tmp/test_async_fifo_" + std::to_string(getpid());
#endif
        server_ = std::make_unique<FifoServer>(fifo_path_);
    }

    void TearDown() override {
        if (server_) {
            server_->stop();
        }
        server_.reset();

        // Clean up FIFO file
        std::error_code ec;
        std::filesystem::remove(fifo_path_, ec);
    }

    std::string fifo_path_;
    std::unique_ptr<FifoServer> server_;
};

TEST_F(AsyncFifoServerTest, BasicStartStop) {
    EXPECT_FALSE(server_->isRunning());

    server_->start();
    EXPECT_TRUE(server_->isRunning());

    server_->stop();
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(AsyncFifoServerTest, MultipleStartCalls) {
    EXPECT_FALSE(server_->isRunning());

    server_->start();
    EXPECT_TRUE(server_->isRunning());

    // Second start should not cause issues
    EXPECT_NO_THROW(server_->start());
    EXPECT_TRUE(server_->isRunning());

    server_->stop();
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(AsyncFifoServerTest, MultipleStopCalls) {
    server_->start();
    EXPECT_TRUE(server_->isRunning());

    server_->stop();
    EXPECT_FALSE(server_->isRunning());

    // Second stop should not cause issues
    EXPECT_NO_THROW(server_->stop());
    EXPECT_FALSE(server_->isRunning());
}

#ifndef _WIN32  // FIFO operations are more complex on Windows
TEST_F(AsyncFifoServerTest, SendMessage) {
    server_->start();
    EXPECT_TRUE(server_->isRunning());

    std::promise<std::string> messagePromise;
    auto messageFuture = messagePromise.get_future();

    // Create a reader thread to read from the FIFO
    std::thread reader([this, &messagePromise]() {
        std::this_thread::sleep_for(100ms);  // Give server time to create FIFO

        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            char buffer[1024];
            ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                messagePromise.set_value(std::string(buffer, bytes_read));
            }
            close(fd);
        }
    });

    std::this_thread::sleep_for(200ms);  // Give reader time to open FIFO

    std::string testMessage = "Hello Async FIFO Server!";
    server_->sendMessage(testMessage);

    ASSERT_EQ(messageFuture.wait_for(3s), std::future_status::ready);
    auto receivedMessage = messageFuture.get();

    reader.join();

    EXPECT_EQ(receivedMessage, testMessage);
}

TEST_F(AsyncFifoServerTest, SendMultipleMessages) {
    server_->start();
    EXPECT_TRUE(server_->isRunning());

    const int numMessages = 3;
    std::vector<std::string> testMessages = {
        "Message 1",
        "Message 2",
        "Message 3"
    };

    std::promise<std::vector<std::string>> messagesPromise;
    auto messagesFuture = messagesPromise.get_future();

    std::thread reader([this, &messagesPromise, numMessages]() {
        std::this_thread::sleep_for(100ms);

        std::vector<std::string> receivedMessages;
        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            for (int i = 0; i < numMessages; ++i) {
                char buffer[1024];
                ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
                if (bytes_read > 0) {
                    receivedMessages.emplace_back(buffer, bytes_read);
                }
            }
            close(fd);
        }
        messagesPromise.set_value(receivedMessages);
    });

    std::this_thread::sleep_for(200ms);

    // Send multiple messages
    for (const auto& message : testMessages) {
        server_->sendMessage(message);
        std::this_thread::sleep_for(50ms);
    }

    ASSERT_EQ(messagesFuture.wait_for(5s), std::future_status::ready);
    auto receivedMessages = messagesFuture.get();

    reader.join();

    EXPECT_EQ(receivedMessages.size(), testMessages.size());
    for (size_t i = 0; i < testMessages.size() && i < receivedMessages.size(); ++i) {
        EXPECT_EQ(receivedMessages[i], testMessages[i]);
    }
}

TEST_F(AsyncFifoServerTest, SendLargeMessage) {
    server_->start();
    EXPECT_TRUE(server_->isRunning());

    // Create a large message
    std::string largeMessage(4096, 'X');
    largeMessage += "END";

    std::promise<std::string> messagePromise;
    auto messageFuture = messagePromise.get_future();

    std::thread reader([this, &messagePromise]() {
        std::this_thread::sleep_for(100ms);

        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            std::string receivedData;
            char buffer[1024];
            ssize_t bytes_read;

            while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
                receivedData.append(buffer, bytes_read);
                if (receivedData.find("END") != std::string::npos) {
                    break;
                }
            }
            close(fd);
            messagePromise.set_value(receivedData);
        }
    });

    std::this_thread::sleep_for(200ms);

    server_->sendMessage(largeMessage);

    ASSERT_EQ(messageFuture.wait_for(5s), std::future_status::ready);
    auto receivedMessage = messageFuture.get();

    reader.join();

    EXPECT_EQ(receivedMessage, largeMessage);
}

TEST_F(AsyncFifoServerTest, ConcurrentReaders) {
    server_->start();
    EXPECT_TRUE(server_->isRunning());

    const int numReaders = 3;
    const int messagesPerReader = 2;

    std::vector<std::promise<std::vector<std::string>>> promises(numReaders);
    std::vector<std::future<std::vector<std::string>>> futures;
    std::vector<std::thread> readers;

    for (int i = 0; i < numReaders; ++i) {
        futures.push_back(promises[i].get_future());
    }

    // Create multiple reader threads
    for (int i = 0; i < numReaders; ++i) {
        readers.emplace_back([this, &promises, i, messagesPerReader]() {
            std::this_thread::sleep_for(100ms + std::chrono::milliseconds(i * 10));

            std::vector<std::string> receivedMessages;
            int fd = open(fifo_path_.c_str(), O_RDONLY);
            if (fd != -1) {
                for (int j = 0; j < messagesPerReader; ++j) {
                    char buffer[1024];
                    ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
                    if (bytes_read > 0) {
                        receivedMessages.emplace_back(buffer, bytes_read);
                    }
                }
                close(fd);
            }
            promises[i].set_value(receivedMessages);
        });
    }

    std::this_thread::sleep_for(300ms);

    // Send messages
    for (int i = 0; i < numReaders * messagesPerReader; ++i) {
        std::string message = "Message_" + std::to_string(i);
        server_->sendMessage(message);
        std::this_thread::sleep_for(50ms);
    }

    // Wait for all readers to complete
    int totalMessagesReceived = 0;
    for (int i = 0; i < numReaders; ++i) {
        ASSERT_EQ(futures[i].wait_for(5s), std::future_status::ready);
        auto messages = futures[i].get();
        totalMessagesReceived += messages.size();
    }

    for (auto& reader : readers) {
        reader.join();
    }

    // At least some messages should have been received
    EXPECT_GT(totalMessagesReceived, 0);
}

TEST_F(AsyncFifoServerTest, SendEmptyMessage) {
    server_->start();
    EXPECT_TRUE(server_->isRunning());

    std::promise<bool> completionPromise;
    auto completionFuture = completionPromise.get_future();

    std::thread reader([this, &completionPromise]() {
        std::this_thread::sleep_for(100ms);

        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            char buffer[1024];
            ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
            close(fd);
            completionPromise.set_value(bytes_read >= 0);
        } else {
            completionPromise.set_value(false);
        }
    });

    std::this_thread::sleep_for(200ms);

    // Send empty message
    EXPECT_NO_THROW(server_->sendMessage(""));

    ASSERT_EQ(completionFuture.wait_for(3s), std::future_status::ready);
    bool completed = completionFuture.get();

    reader.join();

    EXPECT_TRUE(completed);
}

TEST_F(AsyncFifoServerTest, SendAfterStop) {
    server_->start();
    EXPECT_TRUE(server_->isRunning());

    server_->stop();
    EXPECT_FALSE(server_->isRunning());

    // Sending after stop should not crash
    EXPECT_NO_THROW(server_->sendMessage("Test message after stop"));
}

TEST_F(AsyncFifoServerTest, RestartServer) {
    // First run
    server_->start();
    EXPECT_TRUE(server_->isRunning());

    server_->stop();
    EXPECT_FALSE(server_->isRunning());

    // Restart
    server_->start();
    EXPECT_TRUE(server_->isRunning());

    std::promise<std::string> messagePromise;
    auto messageFuture = messagePromise.get_future();

    std::thread reader([this, &messagePromise]() {
        std::this_thread::sleep_for(100ms);

        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            char buffer[1024];
            ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                messagePromise.set_value(std::string(buffer, bytes_read));
            }
            close(fd);
        }
    });

    std::this_thread::sleep_for(200ms);

    std::string testMessage = "Restart test message";
    server_->sendMessage(testMessage);

    ASSERT_EQ(messageFuture.wait_for(3s), std::future_status::ready);
    auto receivedMessage = messageFuture.get();

    reader.join();

    EXPECT_EQ(receivedMessage, testMessage);
}
#endif  // !_WIN32

TEST_F(AsyncFifoServerTest, ServerStateConsistency) {
    // Test state consistency across multiple operations
    EXPECT_FALSE(server_->isRunning());

    for (int i = 0; i < 5; ++i) {
        server_->start();
        EXPECT_TRUE(server_->isRunning());

        server_->stop();
        EXPECT_FALSE(server_->isRunning());
    }
}

TEST_F(AsyncFifoServerTest, ThreadSafety) {
    const int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    server_->start();
    EXPECT_TRUE(server_->isRunning());

    // Multiple threads trying to send messages concurrently
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, &successCount]() {
            try {
                std::string message = "Thread_" + std::to_string(i) + "_message";
                server_->sendMessage(message);
                successCount++;
            } catch (...) {
                // Ignore exceptions for this test
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), numThreads);
}
