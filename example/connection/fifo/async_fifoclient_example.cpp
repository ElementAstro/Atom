/*
 * async_fifoclient_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-12

Description: Comprehensive example usage of the async FifoClient class.
Demonstrates all features including:
- Basic async read/write operations
- Timeout handling
- Connection state management

**************************************************/

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

#include "atom/connection/fifo/async_fifoclient.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

// Utility class for formatted loggingclass Logger {
public:
enum Level { LOG_INFO, LOG_SUCCESS, LOG_WARNING, LOG_ERR, LOG_DEBUG };

static void log(Level level, const std::string& component,
                const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::cout << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S")
              << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";

    switch (level) {
        case LOG_INFO:
            std::cout << "[INFO]    ";
            break;
        case LOG_SUCCESS:
            std::cout << "[SUCCESS] ";
            break;
        case LOG_WARNING:
            std::cout << "[WARN]    ";
            break;
        case LOG_ERR:
            std::cout << "[ERROR]   ";
            break;
        case LOG_DEBUG:
            std::cout << "[DEBUG]   ";
            break;
    }

    std::cout << "[" << component << "] " << message << std::endl;
}
};

// Helper function to create platform-specific pipe pathstd::string
// createPipePath(const std::string& name) {
#ifdef _WIN32
return "\\\\.\\pipe\\" + name;
#else
return "/tmp/" + name;
#endif
}

// Helper to ensure FIFO exists on POSIX systemsvoid ensureFifoExists(const
// std::string& path) {
#ifndef _WIN32
unlink(path.c_str());
if (mkfifo(path.c_str(), 0666) != 0) {
    Logger::log(Logger::LOG_WARNING, "Setup", "Could not create FIFO: " + path);
}
#endif
}

}  // namespace

// Example 1: Basic async client usagevoid basicAsyncClientExample() {
Logger::log(Logger::LOG_INFO, "Example1",
            "=== Basic Async FifoClient Usage ===");

const std::string fifoPath = createPipePath("async_basic_fifo");
ensureFifoExists(fifoPath);

try {
    // Create async client
    atom::async::connection::FifoClient client(fifoPath);

    Logger::log(Logger::LOG_INFO, "Example1",
                "Created async FifoClient, isOpen: " +
                    std::string(client.isOpen() ? "yes" : "no"));

    // Write data
    std::string message = "Hello from async FifoClient!";
    bool writeSuccess = client.write(message, std::chrono::milliseconds(1000));

    if (writeSuccess) {
        Logger::log(Logger::LOG_SUCCESS, "Example1",
                    "Write successful: " + message);
    } else {
        Logger::log(Logger::LOG_WARNING, "Example1",
                    "Write failed or timed out");
    }

    // Read data
    auto readResult = client.read(std::chrono::milliseconds(1000));

    if (readResult.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example1",
                    "Read successful: " + readResult.value());
    } else {
        Logger::log(Logger::LOG_WARNING, "Example1",
                    "Read returned no data (timeout expected in demo)");
    }

    // Close client
    client.close();
    Logger::log(Logger::LOG_INFO, "Example1",
                "Client closed, isOpen: " +
                    std::string(client.isOpen() ? "yes" : "no"));

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example1",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example1",
            "Basic async client example completed\n");
}

// Example 2: Timeout handlingvoid timeoutHandlingExample() {
Logger::log(Logger::LOG_INFO, "Example2", "=== Timeout Handling ===");

const std::string fifoPath = createPipePath("async_timeout_fifo");
ensureFifoExists(fifoPath);

try {
    atom::async::connection::FifoClient client(fifoPath);

    // Test different timeout values
    std::vector<std::chrono::milliseconds> timeouts = {
        std::chrono::milliseconds(100), std::chrono::milliseconds(500),
        std::chrono::milliseconds(1000), std::chrono::milliseconds(2000)};

    for (const auto& timeout : timeouts) {
        Logger::log(Logger::LOG_INFO, "Example2",
                    "Testing write with timeout: " +
                        std::to_string(timeout.count()) + "ms");

        auto start = std::chrono::steady_clock::now();
        bool success = client.write("Test message", timeout);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        Logger::log(
            success ? Logger::LOG_SUCCESS : Logger::LOG_WARNING, "Example2",
            "Result: " + std::string(success ? "success" : "timeout/fail") +
                " (elapsed: " + std::to_string(elapsed.count()) + "ms)");
    }

    // Test read with no timeout (nullopt)
    Logger::log(Logger::LOG_INFO, "Example2",
                "Testing read with no timeout...");
    auto readResult = client.read(std::nullopt);
    Logger::log(
        Logger::LOG_INFO, "Example2",
        "Read result: " +
            std::string(readResult.has_value() ? "data received" : "no data"));

    client.close();

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example2",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example2",
            "Timeout handling example completed\n");
}

// Example 3: Multiple operationsvoid multipleOperationsExample() {
Logger::log(Logger::LOG_INFO, "Example3", "=== Multiple Operations ===");

const std::string fifoPath = createPipePath("async_multi_fifo");
ensureFifoExists(fifoPath);

try {
    atom::async::connection::FifoClient client(fifoPath);

    // Perform multiple write operations
    int successCount = 0;
    int totalOperations = 10;

    Logger::log(Logger::LOG_INFO, "Example3",
                "Performing " + std::to_string(totalOperations) +
                    " write operations...");

    for (int i = 0; i < totalOperations; ++i) {
        std::string msg = "Message " + std::to_string(i + 1);
        if (client.write(msg, std::chrono::milliseconds(500))) {
            successCount++;
        }
    }

    Logger::log(Logger::LOG_SUCCESS, "Example3",
                "Completed " + std::to_string(successCount) + "/" +
                    std::to_string(totalOperations) + " write operations");

    client.close();

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example3",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example3",
            "Multiple operations example completed\n");
}

// Example 4: Connection state managementvoid connectionStateExample() {
Logger::log(Logger::LOG_INFO, "Example4",
            "=== Connection State Management ===");

const std::string fifoPath = createPipePath("async_state_fifo");
ensureFifoExists(fifoPath);

try {
    atom::async::connection::FifoClient client(fifoPath);

    // Check initial state
    Logger::log(Logger::LOG_INFO, "Example4",
                "Initial state - isOpen: " +
                    std::string(client.isOpen() ? "yes" : "no"));

    // Perform operation
    client.write("Test", std::chrono::milliseconds(500));
    Logger::log(
        Logger::LOG_INFO, "Example4",
        "After write - isOpen: " + std::string(client.isOpen() ? "yes" : "no"));

    // Close
    client.close();
    Logger::log(
        Logger::LOG_INFO, "Example4",
        "After close - isOpen: " + std::string(client.isOpen() ? "yes" : "no"));

    // Try operation after close
    bool writeAfterClose =
        client.write("After close", std::chrono::milliseconds(500));
    Logger::log(
        Logger::LOG_INFO, "Example4",
        "Write after close: " +
            std::string(writeAfterClose ? "succeeded" : "failed (expected)"));

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example4",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example4",
            "Connection state example completed\n");
}

// Example 5: Concurrent client simulationvoid concurrentClientsExample() {
Logger::log(Logger::LOG_INFO, "Example5",
            "=== Concurrent Clients Simulation ===");

const std::string fifoPath = createPipePath("async_concurrent_fifo");
ensureFifoExists(fifoPath);

try {
    std::vector<std::thread> threads;
    std::atomic<int> successfulWrites{0};

    // Create multiple client threads
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&, clientId = i]() {
            try {
                atom::async::connection::FifoClient client(fifoPath);

                for (int j = 0; j < 5; ++j) {
                    std::string msg = "Client" + std::to_string(clientId) +
                                      "_Msg" + std::to_string(j);
                    if (client.write(msg, std::chrono::milliseconds(500))) {
                        successfulWrites++;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                client.close();
            } catch (const std::exception& e) {
                Logger::log(Logger::LOG_ERR, "Example5",
                            "Client " + std::to_string(clientId) +
                                " error: " + e.what());
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    Logger::log(
        Logger::LOG_SUCCESS, "Example5",
        "Total successful writes: " + std::to_string(successfulWrites.load()));

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example5",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example5",
            "Concurrent clients example completed\n");
}

int main() {
    Logger::log(Logger::LOG_INFO, "Main",
                "==========================================");
    Logger::log(Logger::LOG_INFO, "Main",
                "  Async FifoClient Comprehensive Examples");
    Logger::log(Logger::LOG_INFO, "Main",
                "==========================================\n");

    // Run all examples
    basicAsyncClientExample();
    timeoutHandlingExample();
    multipleOperationsExample();
    connectionStateExample();
    concurrentClientsExample();

    Logger::log(Logger::LOG_SUCCESS, "Main",
                "==========================================");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "  All async FifoClient examples completed!");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "==========================================");

    return 0;
}
