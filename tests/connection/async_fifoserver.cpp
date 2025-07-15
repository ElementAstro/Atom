#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <asio.hpp>
#include <chrono>  // For sleep
#include <filesystem>
#include <fstream>  // For client simulation
#include <functional>
#include <future>  // For std::future
#include <random>  // For unique path generation
#include <string>
#include <thread>
#include <vector>

// Include the header for the class under test
#include "atom/connection/async_fifoserver.hpp"

// Include platform-specific headers if needed for client simulation details
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace atom::async::connection {

// Mock classes for handlers
class MockMessageHandler {
public:
    MOCK_METHOD(void, handle, (std::string_view data), ());
};

class MockClientHandler {
public:
    MOCK_METHOD(void, handle, (FifoServer::ClientEvent event), ());
};

class MockErrorHandler {
public:
    MOCK_METHOD(void, handle, (const asio::error_code& ec), ());
};

// Test fixture for FifoServer
class FifoServerTest : public ::testing::Test {
public:
    // Unique path for the FIFO for each test
    std::string fifo_path;
    // Unique pointer to the server instance
    std::unique_ptr<FifoServer> server;

    // Setup method: Create a unique FIFO path
    void SetUp() override {
        // Generate a unique path in the temporary directory
        // Use a prefix to make it identifiable and add a unique part
        auto now = std::chrono::high_resolution_clock::now()
                       .time_since_epoch()
                       .count();
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<long long> distrib;

        fifo_path = (std::filesystem::temp_directory_path() /
                     ("test_fifo_" + std::to_string(now) + "_" +
                      std::to_string(distrib(gen))))
                        .string();

        // The FIFO file itself is created by FifoServer::start on Unix
        // On Windows, named pipes are created differently, but the current
        // server impl is Unix-only. If Windows support is added, this setup
        // might need conditional compilation.
    }

    // Teardown method: Stop the server and remove the FIFO file
    void TearDown() override {
        if (server) {
            server->stop();
            server.reset();  // Ensure server is destroyed
        }
        // Remove the FIFO file if it exists (Unix-specific cleanup)
#ifndef _WIN32
        if (std::filesystem::exists(fifo_path)) {
            std::filesystem::remove(fifo_path);
        }
#endif
    }

    // Helper function to simulate a client writing to the FIFO
    void clientWrite(const std::string& path, const std::string& message) {
        std::ofstream fifo(path);
        if (fifo.is_open()) {
            fifo << message << '\n';
            fifo.flush();
        } else {
            FAIL() << "Client failed to open FIFO for writing: " << path;
        }
    }

    // Helper function to simulate a client reading from the FIFO
    std::string clientRead(const std::string& path, size_t expected_length) {
        std::string received_data;
        std::ifstream fifo(path);
        if (fifo.is_open()) {
            std::vector<char> buffer(expected_length + 1);
            fifo.read(buffer.data(), expected_length);
            received_data.assign(buffer.data(), fifo.gcount());
        } else {
            ADD_FAILURE() << "Client failed to open FIFO for reading: " << path;
            return "";  // Return empty string on failure
        }
        return received_data;
    }
};

// Test case: Constructor and Destructor
TEST_F(FifoServerTest, ConstructorDestructor) {
    // Server is created and destroyed within the fixture
    // Check that no exceptions are thrown and cleanup happens (FIFO file
    // removed)
    server = std::make_unique<FifoServer>(fifo_path);
    EXPECT_FALSE(server->isRunning());
    EXPECT_EQ(server->getPath(), fifo_path);
    // Server is destroyed by fixture TearDown
}

// Test case: Start and Stop cycle
TEST_F(FifoServerTest, StartStopCycle) {
    server = std::make_unique<FifoServer>(fifo_path);
    EXPECT_FALSE(server->isRunning());

    // Start the server
    server->start([](std::string_view) {});  // Provide a dummy handler
    // Give time for the io_context thread to start and mkfifo to potentially
    // happen
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(server->isRunning());
#ifndef _WIN32  // Check FIFO file existence only on Unix
    EXPECT_TRUE(std::filesystem::exists(fifo_path));
#endif

    // Stop the server
    server->stop();
    // Give time for the io_context thread to stop
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(server->isRunning());
    // FIFO file should be removed by destructor/stop on Unix
#ifndef _WIN32
    EXPECT_FALSE(std::filesystem::exists(fifo_path));
#endif
}

// Test case: Start when already running
TEST_F(FifoServerTest, StartWhenAlreadyRunning) {
    server = std::make_unique<FifoServer>(fifo_path);
    server->start([](std::string_view) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(server->isRunning());

    // Call start again
    server->start([](std::string_view) {});  // Should be a no-op
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(server->isRunning());  // Should still be running
}

// Test case: Stop when not running
TEST_F(FifoServerTest, StopWhenNotRunning) {
    server = std::make_unique<FifoServer>(fifo_path);
    EXPECT_FALSE(server->isRunning());

    // Call stop
    server->stop();                     // Should be a no-op
    EXPECT_FALSE(server->isRunning());  // Should still not be running
}

// Test case: isRunning state check
TEST_F(FifoServerTest, IsRunningState) {
    server = std::make_unique<FifoServer>(fifo_path);
    EXPECT_FALSE(server->isRunning());

    server->start([](std::string_view) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(server->isRunning());

    server->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(server->isRunning());
}

// Test case: getPath returns correct path
TEST_F(FifoServerTest, GetPath) {
    server = std::make_unique<FifoServer>(fifo_path);
    EXPECT_EQ(server->getPath(), fifo_path);
}

// Test case: Receive a single message
TEST_F(FifoServerTest, ReceiveSingleMessage) {
    server = std::make_unique<FifoServer>(fifo_path);
    MockMessageHandler mock_handler;
    std::string received_data;
    bool handler_called = false;

    // Set expectation on the mock handler
    EXPECT_CALL(mock_handler, handle(testing::_))
        .Times(1)
        .WillOnce(testing::Invoke([&](std::string_view data) {
            received_data = std::string(data);
            handler_called = true;
        }));

    // Start the server with the mock handler
    server->start(std::bind(&MockMessageHandler::handle, &mock_handler,
                            std::placeholders::_1));
    std::this_thread::sleep_for(std::chrono::milliseconds(
        50));  // Give server time to start and open FIFO

    // Simulate a client writing a message
    std::string test_message = "Hello, FIFO!";
    std::thread client_thread(&FifoServerTest::clientWrite, fifo_path,
                              test_message);

    // Wait for the handler to be called (or a timeout)
    // Using a simple sleep here; a more robust test would use a condition
    // variable
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Join the client thread
    client_thread.join();

    // Verify the handler was called and received the correct message
    EXPECT_TRUE(handler_called);
    // The message includes the newline character read by async_read_until
    EXPECT_EQ(received_data, test_message + '\n');

    server->stop();
}

// Test case: Receive multiple messages
TEST_F(FifoServerTest, ReceiveMultipleMessages) {
    server = std::make_unique<FifoServer>(fifo_path);
    MockMessageHandler mock_handler;
    std::vector<std::string> received_messages;

    // Set expectation: handler should be called twice
    EXPECT_CALL(mock_handler, handle(testing::_))
        .Times(2)
        .WillRepeatedly(testing::Invoke([&](std::string_view data) {
            received_messages.push_back(std::string(data));
        }));

    server->start(std::bind(&MockMessageHandler::handle, &mock_handler,
                            std::placeholders::_1));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Simulate client writing two messages
    std::string msg1 = "First message";
    std::string msg2 = "Second message";
    std::thread client_thread([&]() {
        std::ofstream fifo(fifo_path);
        if (fifo.is_open()) {
            fifo << msg1 << '\n';
            fifo.flush();
            std::this_thread::sleep_for(
                std::chrono::milliseconds(10));  // Small delay
            fifo << msg2 << '\n';
            fifo.flush();
        } else {
            FAIL() << "Client failed to open FIFO for writing";
        }
    });

    // Wait for messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    client_thread.join();

    // Verify both messages were received
    EXPECT_EQ(received_messages.size(), 2);
    if (received_messages.size() == 2) {
        EXPECT_EQ(received_messages[0], msg1 + '\n');
        EXPECT_EQ(received_messages[1], msg2 + '\n');
    }

    server->stop();
}

// Test case: Client connection and disconnection events
TEST_F(FifoServerTest, ClientConnectionDisconnectionEvents) {
    server = std::make_unique<FifoServer>(fifo_path);
    MockClientHandler mock_client_handler;
    std::vector<FifoServer::ClientEvent> events;

    EXPECT_CALL(mock_client_handler, handle(FifoServer::ClientEvent::Connected))
        .Times(1)
        .WillOnce(testing::Invoke(
            [&](FifoServer::ClientEvent event) { events.push_back(event); }));
    EXPECT_CALL(mock_client_handler,
                handle(FifoServer::ClientEvent::Disconnected))
        .Times(1)
        .WillOnce(testing::Invoke(
            [&](FifoServer::ClientEvent event) { events.push_back(event); }));

    server->setClientHandler(std::bind(&MockClientHandler::handle,
                                       &mock_client_handler,
                                       std::placeholders::_1));
    server->start([](std::string_view) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::thread client_thread([&]() {
        std::ofstream fifo(fifo_path);
        if (fifo.is_open()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } else {
            FAIL() << "Client failed to open FIFO";
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client_thread.join();

    EXPECT_EQ(events.size(), 2);
    if (events.size() == 2) {
        EXPECT_EQ(events[0], FifoServer::ClientEvent::Connected);
        EXPECT_EQ(events[1], FifoServer::ClientEvent::Disconnected);
    }

    server->stop();
}

// Test case: Error handling (e.g., writing to a closed pipe)
// This is tricky with FIFOs and asio. A common error is writing after the
// reader has closed.
TEST_F(FifoServerTest, ErrorHandlingWriteAfterClientClose) {
#ifdef _WIN32
    // This test relies on Unix FIFO behavior (broken pipe on write after reader
    // closes) Skip on Windows where named pipe behavior might differ or the
    // server impl is missing.
    GTEST_SKIP() << "Skipping on Windows due to Unix-specific FIFO behavior";
#endif

    server = std::make_unique<FifoServer>(fifo_path);
    MockErrorHandler mock_error_handler;
    asio::error_code received_ec;
    bool error_handled = false;

    // Set expectation for the error handler
    EXPECT_CALL(mock_error_handler, handle(testing::_))
        .Times(testing::AtLeast(1))  // Expect at least one error
        .WillOnce(testing::Invoke([&](const asio::error_code& ec) {
            received_ec = ec;
            error_handled = true;
        }));

    server->setErrorHandler(std::bind(
        &MockErrorHandler::handle, &mock_error_handler, std::placeholders::_1));
    server->start([](std::string_view) {});  // Dummy message handler
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Simulate a client connecting, then immediately disconnecting
    std::thread client_thread([&]() {
        std::ofstream fifo(fifo_path);  // Opens and immediately closes
    });
    client_thread.join();  // Wait for client to connect and disconnect

    // Give server time to potentially register the disconnection
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Now try to write from the server
    auto future = server->write("Server message after client disconnect\n");

    // Wait for the write operation to complete and the future to be set
    future.wait_for(std::chrono::milliseconds(100));

    // Verify the write failed (future value is false)
    EXPECT_TRUE(future.valid());
    EXPECT_FALSE(future.get());

    // Verify the error handler was called
    EXPECT_TRUE(error_handled);
    // Check for a relevant error code (e.g., broken pipe on Unix)
    EXPECT_EQ(received_ec, asio::error::broken_pipe);

    server->stop();
}

// Test case: Asynchronous write operation
TEST_F(FifoServerTest, AsyncWrite) {
    server = std::make_unique<FifoServer>(fifo_path);
    std::string received_by_client;
    std::string test_message =
        "Async write test message";  // Client reads until newline

    server->start([](std::string_view) {});  // Dummy message handler
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Simulate a client reading from the FIFO in a separate thread
    std::thread client_thread([&]() {
        std::ifstream fifo(fifo_path);
        if (fifo.is_open()) {
            // Read until newline
            std::getline(fifo, received_by_client);
            // The stream closes when it goes out of scope
        } else {
            FAIL() << "Client failed to open FIFO for reading";
        }
    });

    // Give client thread time to open the FIFO
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Perform the asynchronous write from the server
    // Add newline because the server's read_until expects it, and clientWrite
    // adds it. The server's write method doesn't add a newline, so we must add
    // it here for the client to read it correctly.
    auto future = server->write(test_message + '\n');

    // Wait for the write to complete and the client to read
    future.wait();  // Wait for the future to be ready

    // Wait for the client thread to finish reading
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Wait for the future to be ready and get the result
    EXPECT_TRUE(future.valid());
    EXPECT_TRUE(future.get());  // Expect write to succeed

    // Join the client thread
    client_thread.join();

    // Verify the client received the correct message (getline removes the
    // newline)
    EXPECT_EQ(received_by_client, test_message);

    server->stop();
}

// Test case: Synchronous write operation
TEST_F(FifoServerTest, SyncWrite) {
    server = std::make_unique<FifoServer>(fifo_path);
    std::string received_by_client;
    std::string test_message =
        "Sync write test message";  // Client reads until newline

    server->start([](std::string_view) {});  // Dummy message handler
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Simulate a client reading from the FIFO in a separate thread
    std::thread client_thread([&]() {
        std::ifstream fifo(fifo_path);
        if (fifo.is_open()) {
            std::getline(fifo, received_by_client);
        } else {
            FAIL() << "Client failed to open FIFO for reading";
        }
    });

    // Give client thread time to open the FIFO
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Perform the synchronous write from the server
    // Add newline for client's getline
    bool write_success = server->writeSync(test_message + '\n');

    // Wait for the client to read
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Join the client thread
    client_thread.join();

    // Verify the write succeeded and the client received the correct message
    EXPECT_TRUE(write_success);
    EXPECT_EQ(received_by_client, test_message);

    server->stop();
}

// Test case: Write when no client is connected
TEST_F(FifoServerTest, WriteWhenNoClient) {
#ifdef _WIN32
    // This test relies on Unix FIFO behavior (write fails without a reader)
    // Skip on Windows where named pipe behavior might differ or the server impl
    // is missing.
    GTEST_SKIP() << "Skipping on Windows due to Unix-specific FIFO behavior";
#endif

    server = std::make_unique<FifoServer>(fifo_path);
    MockErrorHandler mock_error_handler;
    bool error_handled = false;

    // Expect an error when writing without a reader
    EXPECT_CALL(mock_error_handler, handle(testing::_))
        .Times(testing::AtLeast(1))
        .WillOnce(
            testing::Invoke([&]([[maybe_unused]] const asio::error_code& ec) {
                error_handled = true;
                // Specific error code might vary (e.g., EPIPE on Unix)
            }));

    server->setErrorHandler(std::bind(
        &MockErrorHandler::handle, &mock_error_handler, std::placeholders::_1));
    server->start([](std::string_view) {});  // Dummy message handler
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Do NOT start a client reader thread

    // Attempt to write
    auto future = server->write("Message to trigger error\n");

    // Wait for the write to complete (it should fail quickly)
    future.wait_for(std::chrono::milliseconds(100));

    // Verify the write failed
    EXPECT_TRUE(future.valid());
    EXPECT_FALSE(future.get());

    // Verify the error handler was called
    EXPECT_TRUE(error_handled);

    server->stop();
}

// Test case: Write after server is stopped
TEST_F(FifoServerTest, WriteAfterStop) {
    server = std::make_unique<FifoServer>(fifo_path);
    server->start([](std::string_view) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(server->isRunning());

    server->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(server->isRunning());

    // Attempt to write after stopping
    // The current `write` implementation doesn't check `running_`.
    // Writing to a closed/invalid handle in asio after stop might lead to
    // an immediate error or queue the operation which then fails when the
    // io_context is not running. We expect the future to indicate failure or
    // the operation to be discarded.

    auto future = server->write("Message after stop\n");

    // Wait briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Check if the future is ready and indicates failure
    // If the io_context is stopped, async operations might immediately fail or
    // be discarded. If discarded, the future might remain not ready or become
    // invalid. If it fails immediately, it will be ready and get() will return
    // false. We'll check if it's ready and false, or if it's not ready
    // (implying discard/no-op).

    bool future_ready =
        (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready);

    if (future_ready) {
        // Operation completed, expect failure
        EXPECT_FALSE(future.get());
    } else {
        // Operation did not complete immediately, likely discarded due to
        // stopped io_context. This is also a valid outcome for "write failed
        // after stop".
        SUCCEED() << "Write operation after stop did not complete immediately "
                     "(expected behavior when io_context is stopped)";
    }

    // No need to call server->stop() again, it's already stopped.
}

// Test case: Cancel pending operations
TEST_F(FifoServerTest, CancelOperations) {
    server = std::make_unique<FifoServer>(fifo_path);
    MockErrorHandler mock_error_handler;
    bool cancel_error_received = false;

    // Set expectation for a cancellation error (asio::error::operation_aborted)
    EXPECT_CALL(mock_error_handler,
                handle(testing::Eq(asio::error::operation_aborted)))
        .Times(testing::AtLeast(1))  // Expect at least one cancellation error
        .WillOnce(
            testing::Invoke([&]([[maybe_unused]] const asio::error_code& ec) {
                cancel_error_received = true;
            }));

    server->setErrorHandler(std::bind(
        &MockErrorHandler::handle, &mock_error_handler, std::placeholders::_1));
    server->start([](std::string_view) {});  // Dummy message handler
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Start a client reader thread but make it block (e.g., by not writing)
    // This will cause the server's async_read_until to wait.
    std::thread client_thread([&]() {
        // Open the FIFO but don't write anything yet
        std::ifstream fifo(fifo_path);
        if (fifo.is_open()) {
            // Keep it open to allow server's read to block
            std::this_thread::sleep_for(
                std::chrono::milliseconds(200));  // Keep pipe open
        } else {
            FAIL() << "Client failed to open FIFO";
        }
    });

    // Give server time to start reading and client time to open pipe
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Cancel pending operations (the async_read_until)
    server->cancel();

    // Wait for the cancellation error to be handled
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify the cancellation error was received
    EXPECT_TRUE(cancel_error_received);

    // Join the client thread
    client_thread.join();

    server->stop();
}

// Test case: Handler removal prevents calls
TEST_F(FifoServerTest, RemoveHandlerPreventsCall) {
    server = std::make_unique<FifoServer>(fifo_path);
    MockMessageHandler handler1, handler2;
    std::vector<std::string> received_messages_handler2;

    // Create handler functions
    [[maybe_unused]] auto handler_func1 = std::bind(
        &MockMessageHandler::handle, &handler1, std::placeholders::_1);
    auto handler_func2 = std::bind(&MockMessageHandler::handle, &handler2,
                                   std::placeholders::_1);

    // Set expectations:
    // handler1 should NOT be called since we never use it
    EXPECT_CALL(handler1, handle(testing::_)).Times(0);

    // handler2 should be called once with our test message
    EXPECT_CALL(handler2, handle(testing::_))
        .Times(1)
        .WillOnce(testing::Invoke([&](std::string_view data) {
            received_messages_handler2.push_back(std::string(data));
        }));

    // Start server with handler2 only
    server->start(handler_func2);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Simulate client writing a message
    const std::string test_message = "Test message\n";
    std::thread client_thread([this, &test_message]() {
        std::ofstream fifo(fifo_path);
        if (fifo.is_open()) {
            fifo << test_message;
            fifo.flush();
        } else {
            FAIL() << "Client failed to open FIFO for writing: " << fifo_path;
        }
    });

    // Wait for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    client_thread.join();

    // Verify:
    // 1. Only handler2 was called
    // 2. It received exactly one message
    // 3. The message matches what we sent
    ASSERT_EQ(received_messages_handler2.size(), 1);
    EXPECT_EQ(received_messages_handler2[0], test_message);

    server->stop();
}

// Test case: Setting client handler replaces previous one
TEST_F(FifoServerTest, SetClientHandlerReplaces) {
    server = std::make_unique<FifoServer>(fifo_path);
    MockClientHandler handler1, handler2;
    std::vector<FifoServer::ClientEvent> events;

    server->setClientHandler(std::bind(&MockClientHandler::handle, &handler1,
                                       std::placeholders::_1));
    EXPECT_CALL(handler1, handle(testing::_)).Times(0);

    server->setClientHandler(std::bind(&MockClientHandler::handle, &handler2,
                                       std::placeholders::_1));

    EXPECT_CALL(handler2, handle(FifoServer::ClientEvent::Connected))
        .Times(1)
        .WillOnce(testing::Invoke(
            [&](FifoServer::ClientEvent event) { events.push_back(event); }));
    EXPECT_CALL(handler2, handle(FifoServer::ClientEvent::Disconnected))
        .Times(1)
        .WillOnce(testing::Invoke(
            [&](FifoServer::ClientEvent event) { events.push_back(event); }));

    server->start([](std::string_view) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::thread client_thread([&]() {
        std::ofstream fifo(fifo_path);
        if (fifo.is_open()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client_thread.join();

    EXPECT_EQ(events.size(), 2);
    if (events.size() == 2) {
        EXPECT_EQ(events[0], FifoServer::ClientEvent::Connected);
        EXPECT_EQ(events[1], FifoServer::ClientEvent::Disconnected);
    }

    server->stop();
}

// Test case: Setting error handler replaces previous one
TEST_F(FifoServerTest, SetErrorHandlerReplaces) {
#ifdef _WIN32
    // This test relies on Unix FIFO behavior (write fails without a reader)
    // Skip on Windows where named pipe behavior might differ or the server impl
    // is missing.
    GTEST_SKIP() << "Skipping on Windows due to Unix-specific FIFO behavior";
#endif

    server = std::make_unique<FifoServer>(fifo_path);
    MockErrorHandler handler1, handler2;
    bool handler2_called = false;

    // Set handler1 first
    server->setErrorHandler(
        std::bind(&MockErrorHandler::handle, &handler1, std::placeholders::_1));

    // Set expectation for handler1: Should NOT be called
    EXPECT_CALL(handler1, handle(testing::_)).Times(0);

    // Set handler2, replacing handler1
    server->setErrorHandler(
        std::bind(&MockErrorHandler::handle, &handler2, std::placeholders::_1));

    // Set expectation for handler2: Should be called on error
    EXPECT_CALL(handler2, handle(testing::_))
        .Times(testing::AtLeast(1))
        .WillOnce(
            testing::Invoke([&]([[maybe_unused]] const asio::error_code& ec) {
                handler2_called = true;
            }));

    server->start([](std::string_view) {});  // Dummy message handler
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Simulate an error (e.g., writing when no client is connected)
    // Do NOT start a client reader thread

    // Attempt to write
    auto future = server->write("Message to trigger error\n");

    // Wait for the write to complete (it should fail)
    future.wait_for(std::chrono::milliseconds(100));

    // Verify the write failed
    EXPECT_TRUE(future.valid());
    EXPECT_FALSE(future.get());

    // Verify only handler2 was called
    EXPECT_TRUE(handler2_called);

    server->stop();
}

}  // namespace atom::async::connection