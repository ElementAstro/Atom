/**
 * @file async_operations.cpp
 * @brief Asynchronous serial port operations example
 *
 * This example demonstrates asynchronous serial communication including:
 * - Callback-based asynchronous read operations
 * - Future-based asynchronous read and write operations
 * - Concurrent read/write operations
 * - Proper async operation management and cancellation
 * - Error handling in asynchronous contexts
 * - Performance considerations for async operations
 *
 * @author Atom Serial Examples
 * @date 2024
 */

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include "atom/serial/core/serial_port.hpp"

using namespace atom::serial;

/**
 * @brief Thread-safe message queue for demonstration
 */
class MessageQueue {
private:
    std::queue<std::string> queue_;
    std::mutex mutex_;

public:
    void push(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(message);
    }

    bool pop(std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        message = queue_.front();
        queue_.pop();
        return true;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
};

/**
 * @brief Demonstrates callback-based asynchronous reading
 */
void demonstrateCallbackAsyncRead(SerialPort& port) {
    std::cout << "\n=== Callback-Based Async Read ===\n";

    if (!port.isOpen()) {
        std::cout << "Port is not open, skipping async read demonstration.\n";
        return;
    }

    std::atomic<bool> reading{true};
    std::atomic<int> messagesReceived{0};
    MessageQueue receivedMessages;

    std::cout << "Starting callback-based async read...\n";
    std::cout << "This will read data for 5 seconds.\n";

    // Start asynchronous reading with callback
    port.asyncRead(256, [&](std::vector<uint8_t> data) {
        if (!data.empty()) {
            messagesReceived++;

            // Convert to string for display
            std::string message(data.begin(), data.end());
            receivedMessages.push("Callback received " +
                                  std::to_string(data.size()) +
                                  " bytes: " + message);

            std::cout << "  [Callback] Received " << data.size() << " bytes\n";
        }
    });

    // Let it run for a few seconds
    auto startTime = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - startTime <
           std::chrono::seconds(5)) {
        // Send some test data to potentially receive
        try {
            port.write("Test message " +
                       std::to_string(messagesReceived.load()) + "\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } catch (const SerialException& e) {
            std::cout << "  Error sending test data: " << e.what() << "\n";
        }

        // Display any queued messages
        std::string message;
        while (receivedMessages.pop(message)) {
            std::cout << "  " << message << "\n";
        }
    }

    reading = false;
    std::cout << "Callback async read completed. Messages received: "
              << messagesReceived.load() << "\n";
}

/**
 * @brief Demonstrates future-based asynchronous reading
 */
void demonstrateFutureAsyncRead(SerialPort& port) {
    std::cout << "\n=== Future-Based Async Read ===\n";

    if (!port.isOpen()) {
        std::cout
            << "Port is not open, skipping future async read demonstration.\n";
        return;
    }

    std::cout << "Starting future-based async read operations...\n";

    // Launch multiple async read operations
    std::vector<std::future<std::vector<uint8_t>>> readFutures;

    for (int i = 0; i < 3; ++i) {
        std::cout << "  Launching async read operation " << (i + 1) << "\n";
        readFutures.push_back(port.asyncReadFuture(128));
    }

    // Send some test data
    std::cout << "  Sending test data...\n";
    for (int i = 0; i < 3; ++i) {
        try {
            std::string testData =
                "Future test " + std::to_string(i + 1) + "\n";
            port.write(testData);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (const SerialException& e) {
            std::cout << "  Error sending test data: " << e.what() << "\n";
        }
    }

    // Wait for and process results
    for (size_t i = 0; i < readFutures.size(); ++i) {
        try {
            // Wait for result with timeout
            auto status = readFutures[i].wait_for(std::chrono::seconds(2));

            if (status == std::future_status::ready) {
                auto data = readFutures[i].get();
                std::cout << "  Future " << (i + 1) << " completed: received "
                          << data.size() << " bytes\n";

                if (!data.empty()) {
                    std::string message(data.begin(), data.end());
                    std::cout << "    Data: " << message;
                }
            } else if (status == std::future_status::timeout) {
                std::cout << "  Future " << (i + 1) << " timed out\n";
            } else {
                std::cout << "  Future " << (i + 1) << " deferred\n";
            }

        } catch (const SerialException& e) {
            std::cout << "  Future " << (i + 1)
                      << " threw exception: " << e.what() << "\n";
        }
    }

    std::cout << "Future-based async read completed.\n";
}

/**
 * @brief Demonstrates asynchronous write operations
 */
void demonstrateAsyncWrite(SerialPort& port) {
    std::cout << "\n=== Asynchronous Write Operations ===\n";

    if (!port.isOpen()) {
        std::cout << "Port is not open, skipping async write demonstration.\n";
        return;
    }

    std::cout << "Starting asynchronous write operations...\n";

    // Launch multiple async write operations
    std::vector<std::future<size_t>> writeFutures;
    std::vector<std::string> messages = {
        "Async message 1\n", "Async message 2\n", "Async message 3\n",
        "Final async message\n"};

    for (size_t i = 0; i < messages.size(); ++i) {
        std::cout << "  Launching async write " << (i + 1) << ": \""
                  << messages[i].substr(0, messages[i].length() - 1) << "\"\n";
        writeFutures.push_back(port.asyncWrite(messages[i]));
    }

    // Wait for all writes to complete
    size_t totalBytesWritten = 0;
    for (size_t i = 0; i < writeFutures.size(); ++i) {
        try {
            auto status = writeFutures[i].wait_for(std::chrono::seconds(2));

            if (status == std::future_status::ready) {
                size_t bytesWritten = writeFutures[i].get();
                totalBytesWritten += bytesWritten;
                std::cout << "  Write " << (i + 1)
                          << " completed: " << bytesWritten
                          << " bytes written\n";
            } else if (status == std::future_status::timeout) {
                std::cout << "  Write " << (i + 1) << " timed out\n";
            } else {
                std::cout << "  Write " << (i + 1) << " deferred\n";
            }

        } catch (const SerialException& e) {
            std::cout << "  Write " << (i + 1)
                      << " threw exception: " << e.what() << "\n";
        }
    }

    std::cout << "Async write operations completed. Total bytes written: "
              << totalBytesWritten << "\n";
}

/**
 * @brief Demonstrates concurrent read and write operations
 */
void demonstrateConcurrentOperations(SerialPort& port) {
    std::cout << "\n=== Concurrent Read/Write Operations ===\n";

    if (!port.isOpen()) {
        std::cout << "Port is not open, skipping concurrent operations "
                     "demonstration.\n";
        return;
    }

    std::cout << "Starting concurrent read and write operations...\n";

    std::atomic<bool> operationsActive{true};
    std::atomic<int> messagesWritten{0};
    std::atomic<int> messagesRead{0};

    // Start concurrent async read
    port.asyncRead(256, [&](std::vector<uint8_t> data) {
        if (!data.empty() && operationsActive.load()) {
            messagesRead++;
            std::cout << "  [Concurrent Read] Received " << data.size()
                      << " bytes\n";
        }
    });

    // Launch concurrent async writes
    std::vector<std::future<size_t>> concurrentWrites;
    for (int i = 0; i < 5; ++i) {
        std::string message =
            "Concurrent message " + std::to_string(i + 1) + "\n";
        concurrentWrites.push_back(port.asyncWrite(message));
        messagesWritten++;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Wait for writes to complete
    for (auto& future : concurrentWrites) {
        try {
            future.wait_for(std::chrono::seconds(1));
        } catch (const SerialException& e) {
            std::cout << "  Concurrent write error: " << e.what() << "\n";
        }
    }

    // Allow some time for reads to complete
    std::this_thread::sleep_for(std::chrono::seconds(1));
    operationsActive = false;

    std::cout << "Concurrent operations completed.\n";
    std::cout << "  Messages written: " << messagesWritten.load() << "\n";
    std::cout << "  Messages read: " << messagesRead.load() << "\n";
}

/**
 * @brief Demonstrates error handling in async operations
 */
void demonstrateAsyncErrorHandling(SerialPort& port) {
    std::cout << "\n=== Async Error Handling ===\n";

    std::cout << "Demonstrating error handling in async operations...\n";

    // Try async operations on closed port
    if (port.isOpen()) {
        port.close();
    }

    std::cout << "  Testing async read on closed port...\n";
    try {
        auto future = port.asyncReadFuture(100);
        auto status = future.wait_for(std::chrono::milliseconds(500));

        if (status == std::future_status::ready) {
            auto data = future.get();
            std::cout << "  Unexpected success: received " << data.size()
                      << " bytes\n";
        } else {
            std::cout << "  Async read timed out (expected for closed port)\n";
        }
    } catch (const SerialException& e) {
        std::cout << "  Expected exception caught: " << e.what() << "\n";
    }

    std::cout << "  Testing async write on closed port...\n";
    try {
        auto future = port.asyncWrite("Test message");
        auto status = future.wait_for(std::chrono::milliseconds(500));

        if (status == std::future_status::ready) {
            size_t bytesWritten = future.get();
            std::cout << "  Unexpected success: wrote " << bytesWritten
                      << " bytes\n";
        } else {
            std::cout << "  Async write timed out (expected for closed port)\n";
        }
    } catch (const SerialException& e) {
        std::cout << "  Expected exception caught: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating asynchronous operations
 */
int main() {
    std::cout << "=== Asynchronous Serial Port Operations Example ===\n";
    std::cout << "This example demonstrates various asynchronous communication "
                 "patterns.\n";

    // Get available ports
    auto ports = SerialPort::getAvailablePorts();
    if (ports.empty()) {
        std::cout << "\nNo serial ports available for testing.\n";
        std::cout << "Connect a serial device or use a virtual serial port for "
                     "full demonstration.\n";

        // Still demonstrate error handling
        SerialPort port;
        demonstrateAsyncErrorHandling(port);
        return 0;
    }

    // Open a port for testing
    SerialPort port;
    SerialConfig config;
    config.setBaudRate(9600);
    config.setReadTimeout(std::chrono::milliseconds(500));
    config.setWriteTimeout(std::chrono::milliseconds(500));

    std::string selectedPort = ports[0];
    std::cout << "\nUsing port: " << selectedPort << " for demonstration\n";

    auto error = port.tryOpen(selectedPort, config);
    if (error) {
        std::cout << "Failed to open port: " << *error << "\n";
        std::cout << "Demonstrating error handling only...\n";
        demonstrateAsyncErrorHandling(port);
        return 1;
    }

    std::cout << "Port opened successfully!\n";

    // Demonstrate various async operations
    demonstrateCallbackAsyncRead(port);
    demonstrateFutureAsyncRead(port);
    demonstrateAsyncWrite(port);
    demonstrateConcurrentOperations(port);
    demonstrateAsyncErrorHandling(port);

    std::cout << "\n=== Async Operations Best Practices ===\n";
    std::cout << "1. Always handle exceptions in async operations\n";
    std::cout << "2. Use appropriate timeouts to avoid indefinite blocking\n";
    std::cout << "3. Consider thread safety when using callbacks\n";
    std::cout << "4. Clean up async operations before closing ports\n";
    std::cout << "5. Monitor async operation status and handle timeouts\n";
    std::cout << "6. Use futures for operations that need return values\n";
    std::cout << "7. Use callbacks for continuous data streaming\n";

    std::cout << "\n=== Example Complete ===\n";

    return 0;
}
