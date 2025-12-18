/*
 * async_fifoserver_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-12

Description: Comprehensive example usage of the async FifoServer class.
Demonstrates all features including:
- Basic async server operations
- Start/stop lifecycle
- Running state management

**************************************************/

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

#include "atom/connection/fifo/async_fifoserver.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

// Utility class for formatted logging
class Logger {
public:
    enum Level { INFO, SUCCESS, WARNING, ERR, DEBUG };

    static void log(Level level, const std::string& component,
                    const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::cout << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count()
                  << "] ";

        switch (level) {
            case INFO:
                std::cout << "[INFO]    ";
                break;
            case SUCCESS:
                std::cout << "[SUCCESS] ";
                break;
            case WARNING:
                std::cout << "[WARN]    ";
                break;
            case ERR:
                std::cout << "[ERROR]   ";
                break;
            case DEBUG:
                std::cout << "[DEBUG]   ";
                break;
        }

        std::cout << "[" << component << "] " << message << std::endl;
    }
};

// Helper function to create platform-specific pipe path
std::string createPipePath(const std::string& name) {
#ifdef _WIN32
    return "\\\\.\\pipe\\" + name;
#else
    return "/tmp/" + name;
#endif
}

// Helper to ensure FIFO exists on POSIX systems
void ensureFifoExists(const std::string& path) {
#ifndef _WIN32
    unlink(path.c_str());
    if (mkfifo(path.c_str(), 0666) != 0) {
        Logger::log(Logger::WARNING, "Setup", "Could not create FIFO: " + path);
    }
#endif
}

}  // namespace

// Example 1: Basic async server usage
void basicAsyncServerExample() {
    Logger::log(Logger::INFO, "Example1",
                "=== Basic Async FifoServer Usage ===");

    const std::string fifoPath = createPipePath("async_server_basic");
    ensureFifoExists(fifoPath);

    try {
        // Create async server
        atom::async::connection::FifoServer server(fifoPath);

        Logger::log(Logger::INFO, "Example1",
                    "Created async FifoServer, isRunning: " +
                        std::string(server.isRunning() ? "yes" : "no"));

        // Start the server
        server.start();
        Logger::log(Logger::SUCCESS, "Example1",
                    "Server started, isRunning: " +
                        std::string(server.isRunning() ? "yes" : "no"));

        // Let server run for a while
        Logger::log(Logger::INFO, "Example1",
                    "Server running for 2 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Stop the server
        server.stop();
        Logger::log(Logger::INFO, "Example1",
                    "Server stopped, isRunning: " +
                        std::string(server.isRunning() ? "yes" : "no"));

    } catch (const std::exception& e) {
        Logger::log(Logger::ERR, "Example1",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::INFO, "Example1",
                "Basic async server example completed\n");
}

// Example 2: Server lifecycle management
void lifecycleManagementExample() {
    Logger::log(Logger::INFO, "Example2",
                "=== Server Lifecycle Management ===");

    const std::string fifoPath = createPipePath("async_server_lifecycle");
    ensureFifoExists(fifoPath);

    try {
        atom::async::connection::FifoServer server(fifoPath);

        // Multiple start/stop cycles
        for (int cycle = 1; cycle <= 3; ++cycle) {
            Logger::log(Logger::INFO, "Example2",
                        "=== Cycle " + std::to_string(cycle) + " ===");

            // Start
            server.start();
            Logger::log(Logger::SUCCESS, "Example2",
                        "Started - isRunning: " +
                            std::string(server.isRunning() ? "yes" : "no"));

            // Run for a short time
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Stop
            server.stop();
            Logger::log(Logger::INFO, "Example2",
                        "Stopped - isRunning: " +
                            std::string(server.isRunning() ? "yes" : "no"));

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::ERR, "Example2",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::INFO, "Example2",
                "Lifecycle management example completed\n");
}

// Example 3: Running state checks
void runningStateExample() {
    Logger::log(Logger::INFO, "Example3", "=== Running State Checks ===");

    const std::string fifoPath = createPipePath("async_server_state");
    ensureFifoExists(fifoPath);

    try {
        atom::async::connection::FifoServer server(fifoPath);

        // Check state before start
        Logger::log(Logger::INFO, "Example3",
                    "Before start - isRunning: " +
                        std::string(server.isRunning() ? "yes" : "no"));

        server.start();

        // Periodic state checks while running
        for (int i = 0; i < 5; ++i) {
            Logger::log(Logger::INFO, "Example3",
                        "Check " + std::to_string(i + 1) + " - isRunning: " +
                            std::string(server.isRunning() ? "yes" : "no"));
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        server.stop();

        // Check state after stop
        Logger::log(Logger::INFO, "Example3",
                    "After stop - isRunning: " +
                        std::string(server.isRunning() ? "yes" : "no"));

    } catch (const std::exception& e) {
        Logger::log(Logger::ERR, "Example3",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::INFO, "Example3", "Running state example completed\n");
}

// Example 4: Multiple servers
void multipleServersExample() {
    Logger::log(Logger::INFO, "Example4", "=== Multiple Servers ===");

    try {
        std::vector<std::unique_ptr<atom::async::connection::FifoServer>>
            servers;
        std::vector<std::string> paths;

        // Create multiple servers
        for (int i = 0; i < 3; ++i) {
            std::string path =
                createPipePath("async_server_multi_" + std::to_string(i));
            ensureFifoExists(path);
            paths.push_back(path);
            servers.push_back(
                std::make_unique<atom::async::connection::FifoServer>(path));
        }

        // Start all servers
        Logger::log(Logger::INFO, "Example4", "Starting all servers...");
        for (size_t i = 0; i < servers.size(); ++i) {
            servers[i]->start();
            Logger::log(Logger::SUCCESS, "Example4",
                        "Server " + std::to_string(i) + " started");
        }

        // Check all running
        Logger::log(Logger::INFO, "Example4", "Checking server states...");
        for (size_t i = 0; i < servers.size(); ++i) {
            Logger::log(
                Logger::INFO, "Example4",
                "Server " + std::to_string(i) + " isRunning: " +
                    std::string(servers[i]->isRunning() ? "yes" : "no"));
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Stop all servers
        Logger::log(Logger::INFO, "Example4", "Stopping all servers...");
        for (size_t i = 0; i < servers.size(); ++i) {
            servers[i]->stop();
            Logger::log(Logger::INFO, "Example4",
                        "Server " + std::to_string(i) + " stopped");
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::ERR, "Example4",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::INFO, "Example4",
                "Multiple servers example completed\n");
}

// Example 5: Long-running server simulation
void longRunningServerExample() {
    Logger::log(Logger::INFO, "Example5",
                "=== Long-Running Server Simulation ===");

    const std::string fifoPath = createPipePath("async_server_longrun");
    ensureFifoExists(fifoPath);

    try {
        atom::async::connection::FifoServer server(fifoPath);

        server.start();
        Logger::log(Logger::SUCCESS, "Example5", "Server started");

        // Simulate long-running operation with periodic checks
        auto startTime = std::chrono::steady_clock::now();
        int checkCount = 0;

        while (std::chrono::steady_clock::now() - startTime <
               std::chrono::seconds(3)) {
            if (server.isRunning()) {
                checkCount++;
                if (checkCount % 5 == 0) {
                    auto elapsed =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - startTime);
                    Logger::log(Logger::DEBUG, "Example5",
                                "Server healthy at " +
                                    std::to_string(elapsed.count()) + "ms");
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        server.stop();
        Logger::log(Logger::INFO, "Example5",
                    "Server stopped after " + std::to_string(checkCount) +
                        " health checks");

    } catch (const std::exception& e) {
        Logger::log(Logger::ERR, "Example5",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::INFO, "Example5",
                "Long-running server example completed\n");
}

int main() {
    Logger::log(Logger::INFO, "Main",
                "==========================================");
    Logger::log(Logger::INFO, "Main",
                "  Async FifoServer Comprehensive Examples");
    Logger::log(Logger::INFO, "Main",
                "==========================================\n");

    // Run all examples
    basicAsyncServerExample();
    lifecycleManagementExample();
    runningStateExample();
    multipleServersExample();
    longRunningServerExample();

    Logger::log(Logger::SUCCESS, "Main",
                "==========================================");
    Logger::log(Logger::SUCCESS, "Main",
                "  All async FifoServer examples completed!");
    Logger::log(Logger::SUCCESS, "Main",
                "==========================================");

    return 0;
}
