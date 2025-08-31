#include "atom/connection/async_fifoclient.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <future>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace atom::async::connection;
using namespace std::chrono_literals;

class AsyncFifoClientTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef _WIN32
        fifo_path_ = "\\\\.\\pipe\\test_async_fifoclient_" + std::to_string(GetCurrentProcessId());
#else
        fifo_path_ = "/tmp/test_async_fifoclient_" + std::to_string(getpid());
        // Create FIFO for testing
        mkfifo(fifo_path_.c_str(), 0666);
#endif
        client_ = std::make_unique<FifoClient>(fifo_path_);
    }

    void TearDown() override {
        client_.reset();
        
        // Clean up FIFO file
        std::error_code ec;
        std::filesystem::remove(fifo_path_, ec);
    }

    std::string fifo_path_;
    std::unique_ptr<FifoClient> client_;
};

#ifndef _WIN32  // FIFO operations are more complex on Windows

TEST_F(AsyncFifoClientTest, BasicWrite) {
    std::promise<std::string> messagePromise;
    auto messageFuture = messagePromise.get_future();
    
    // Create a reader thread
    std::thread reader([this, &messagePromise]() {
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
    
    std::this_thread::sleep_for(100ms);  // Give reader time to open FIFO
    
    std::string testMessage = "Hello Async FIFO Client!";
    EXPECT_TRUE(client_->write(testMessage));
    
    ASSERT_EQ(messageFuture.wait_for(3s), std::future_status::ready);
    auto receivedMessage = messageFuture.get();
    
    reader.join();
    
    EXPECT_EQ(receivedMessage, testMessage);
}

TEST_F(AsyncFifoClientTest, WriteWithTimeout) {
    std::promise<std::string> messagePromise;
    auto messageFuture = messagePromise.get_future();
    
    std::thread reader([this, &messagePromise]() {
        std::this_thread::sleep_for(200ms);  // Delayed reader
        
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
    
    std::string testMessage = "Timeout test message";
    EXPECT_TRUE(client_->write(testMessage, 1s));
    
    ASSERT_EQ(messageFuture.wait_for(3s), std::future_status::ready);
    auto receivedMessage = messageFuture.get();
    
    reader.join();
    
    EXPECT_EQ(receivedMessage, testMessage);
}

TEST_F(AsyncFifoClientTest, WriteTimeout) {
    // No reader - should timeout
    std::string testMessage = "This should timeout";
    
    auto start = std::chrono::steady_clock::now();
    bool result = client_->write(testMessage, 500ms);
    auto duration = std::chrono::steady_clock::now() - start;
    
    // Should timeout and return false
    EXPECT_FALSE(result);
    EXPECT_GE(duration, 400ms);  // Allow some tolerance
    EXPECT_LE(duration, 1s);     // But not too long
}

TEST_F(AsyncFifoClientTest, BasicRead) {
    std::string testMessage = "Read test message";
    
    // Create a writer thread
    std::thread writer([this, testMessage]() {
        std::this_thread::sleep_for(100ms);
        
        int fd = open(fifo_path_.c_str(), O_WRONLY);
        if (fd != -1) {
            write(fd, testMessage.c_str(), testMessage.length());
            close(fd);
        }
    });
    
    auto result = client_->read(1024, 3s);
    
    writer.join();
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), testMessage);
}

TEST_F(AsyncFifoClientTest, ReadWithSpecificSize) {
    std::string testMessage = "Partial read test message";
    size_t readSize = 7;  // Read only "Partial"
    
    std::thread writer([this, testMessage]() {
        std::this_thread::sleep_for(100ms);
        
        int fd = open(fifo_path_.c_str(), O_WRONLY);
        if (fd != -1) {
            write(fd, testMessage.c_str(), testMessage.length());
            close(fd);
        }
    });
    
    auto result = client_->read(readSize, 3s);
    
    writer.join();
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), testMessage.substr(0, readSize));
}

TEST_F(AsyncFifoClientTest, ReadTimeout) {
    // No writer - should timeout
    auto start = std::chrono::steady_clock::now();
    auto result = client_->read(1024, 500ms);
    auto duration = std::chrono::steady_clock::now() - start;
    
    EXPECT_FALSE(result.has_value());
    EXPECT_GE(duration, 400ms);
    EXPECT_LE(duration, 1s);
}

TEST_F(AsyncFifoClientTest, MultipleWrites) {
    const int numMessages = 5;
    std::vector<std::string> testMessages;
    
    for (int i = 0; i < numMessages; ++i) {
        testMessages.push_back("Message_" + std::to_string(i));
    }
    
    std::promise<std::vector<std::string>> messagesPromise;
    auto messagesFuture = messagesPromise.get_future();
    
    std::thread reader([this, &messagesPromise, numMessages]() {
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
        messagesPromise.set_value(receivedMessages);
    });
    
    std::this_thread::sleep_for(100ms);
    
    // Write multiple messages
    for (const auto& message : testMessages) {
        EXPECT_TRUE(client_->write(message));
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

TEST_F(AsyncFifoClientTest, LargeDataWrite) {
    // Create a large message
    std::string largeMessage(8192, 'X');
    largeMessage += "END_MARKER";
    
    std::promise<std::string> messagePromise;
    auto messageFuture = messagePromise.get_future();
    
    std::thread reader([this, &messagePromise]() {
        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            std::string receivedData;
            char buffer[1024];
            ssize_t bytes_read;
            
            while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
                receivedData.append(buffer, bytes_read);
                if (receivedData.find("END_MARKER") != std::string::npos) {
                    break;
                }
            }
            close(fd);
            messagePromise.set_value(receivedData);
        }
    });
    
    std::this_thread::sleep_for(100ms);
    
    EXPECT_TRUE(client_->write(largeMessage));
    
    ASSERT_EQ(messageFuture.wait_for(5s), std::future_status::ready);
    auto receivedMessage = messageFuture.get();
    
    reader.join();
    
    EXPECT_EQ(receivedMessage, largeMessage);
}

TEST_F(AsyncFifoClientTest, EmptyWrite) {
    std::promise<bool> completionPromise;
    auto completionFuture = completionPromise.get_future();
    
    std::thread reader([this, &completionPromise]() {
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
    
    std::this_thread::sleep_for(100ms);
    
    EXPECT_TRUE(client_->write(""));
    
    ASSERT_EQ(completionFuture.wait_for(3s), std::future_status::ready);
    bool completed = completionFuture.get();
    
    reader.join();
    
    EXPECT_TRUE(completed);
}

TEST_F(AsyncFifoClientTest, ConcurrentOperations) {
    const int numThreads = 3;
    std::vector<std::thread> writers;
    std::atomic<int> successCount{0};
    
    std::promise<std::vector<std::string>> messagesPromise;
    auto messagesFuture = messagesPromise.get_future();
    
    // Reader thread
    std::thread reader([this, &messagesPromise, numThreads]() {
        std::vector<std::string> receivedMessages;
        
        for (int i = 0; i < numThreads; ++i) {
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
        messagesPromise.set_value(receivedMessages);
    });
    
    std::this_thread::sleep_for(100ms);
    
    // Multiple writer threads
    for (int i = 0; i < numThreads; ++i) {
        writers.emplace_back([this, i, &successCount]() {
            std::string message = "Thread_" + std::to_string(i) + "_message";
            if (client_->write(message)) {
                successCount++;
            }
            std::this_thread::sleep_for(10ms);
        });
    }
    
    for (auto& writer : writers) {
        writer.join();
    }
    
    ASSERT_EQ(messagesFuture.wait_for(5s), std::future_status::ready);
    auto receivedMessages = messagesFuture.get();
    
    reader.join();
    
    EXPECT_EQ(successCount.load(), numThreads);
    EXPECT_EQ(receivedMessages.size(), numThreads);
}

TEST_F(AsyncFifoClientTest, IsOpenStatus) {
    // Client should be open after construction
    EXPECT_TRUE(client_->isOpen());
    
    // Test write to verify it's actually functional
    std::promise<bool> writePromise;
    auto writeFuture = writePromise.get_future();
    
    std::thread reader([this, &writePromise]() {
        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            char buffer[1024];
            read(fd, buffer, sizeof(buffer));
            close(fd);
            writePromise.set_value(true);
        } else {
            writePromise.set_value(false);
        }
    });
    
    std::this_thread::sleep_for(100ms);
    
    EXPECT_TRUE(client_->write("Status test"));
    
    ASSERT_EQ(writeFuture.wait_for(3s), std::future_status::ready);
    bool writeSuccessful = writeFuture.get();
    
    reader.join();
    
    EXPECT_TRUE(writeSuccessful);
}

TEST_F(AsyncFifoClientTest, WriteReadCycle) {
    std::string testMessage = "Write-Read cycle test";
    
    // First write
    std::promise<bool> writePromise;
    auto writeFuture = writePromise.get_future();
    
    std::thread reader([this, &writePromise]() {
        int fd = open(fifo_path_.c_str(), O_RDONLY);
        if (fd != -1) {
            char buffer[1024];
            ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
            close(fd);
            writePromise.set_value(bytes_read > 0);
        } else {
            writePromise.set_value(false);
        }
    });
    
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(client_->write(testMessage));
    
    ASSERT_EQ(writeFuture.wait_for(3s), std::future_status::ready);
    EXPECT_TRUE(writeFuture.get());
    
    reader.join();
    
    // Then read
    std::thread writer([this, testMessage]() {
        std::this_thread::sleep_for(100ms);
        
        int fd = open(fifo_path_.c_str(), O_WRONLY);
        if (fd != -1) {
            write(fd, testMessage.c_str(), testMessage.length());
            close(fd);
        }
    });
    
    auto result = client_->read(testMessage.length(), 3s);
    
    writer.join();
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), testMessage);
}

#endif  // !_WIN32

TEST_F(AsyncFifoClientTest, ConstructorWithPath) {
    // Test that constructor accepts the path correctly
    EXPECT_NO_THROW(FifoClient testClient(fifo_path_));
}

TEST_F(AsyncFifoClientTest, ThreadSafety) {
    const int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> operationCount{0};
    
    // Multiple threads performing operations
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, &operationCount]() {
            try {
                // Test isOpen() call from multiple threads
                if (client_->isOpen()) {
                    operationCount++;
                }
            } catch (...) {
                // Ignore exceptions for this test
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(operationCount.load(), numThreads);
}
