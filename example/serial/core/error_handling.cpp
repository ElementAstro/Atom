/**
 * @file error_handling.cpp
 * @brief Comprehensive error handling and recovery example
 *
 * This example demonstrates robust error handling including:
 * - All serial exception types and their handling
 * - Error recovery strategies and retry mechanisms
 * - Graceful degradation and fallback options
 * - Connection monitoring and automatic reconnection
 * - Logging and error reporting best practices
 * - Resource cleanup in error scenarios
 *
 * @author Atom Serial Examples
 * @date 2024
 */

#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "atom/serial/core/serial_port.hpp"

using namespace atom::serial;

/**
 * @brief Error statistics tracker
 */
struct ErrorStats {
    int totalErrors = 0;
    int openErrors = 0;
    int readErrors = 0;
    int writeErrors = 0;
    int timeoutErrors = 0;
    int configErrors = 0;
    int ioErrors = 0;
    int recoveryAttempts = 0;
    int successfulRecoveries = 0;

    void reset() {
        totalErrors = openErrors = readErrors = writeErrors = 0;
        timeoutErrors = configErrors = ioErrors = 0;
        recoveryAttempts = successfulRecoveries = 0;
    }

    void print() const {
        std::cout << "Error Statistics:\n";
        std::cout << "  Total Errors: " << totalErrors << "\n";
        std::cout << "  Open Errors: " << openErrors << "\n";
        std::cout << "  Read Errors: " << readErrors << "\n";
        std::cout << "  Write Errors: " << writeErrors << "\n";
        std::cout << "  Timeout Errors: " << timeoutErrors << "\n";
        std::cout << "  Config Errors: " << configErrors << "\n";
        std::cout << "  I/O Errors: " << ioErrors << "\n";
        std::cout << "  Recovery Attempts: " << recoveryAttempts << "\n";
        std::cout << "  Successful Recoveries: " << successfulRecoveries
                  << "\n";
    }
};

static ErrorStats g_errorStats;

/**
 * @brief Demonstrates handling of different exception types
 */
void demonstrateExceptionTypes() {
    std::cout << "\n=== Exception Types Demonstration ===\n";

    SerialPort port;

    // 1. SerialPortNotOpenException
    std::cout << "1. Testing SerialPortNotOpenException:\n";
    try {
        port.write("This should fail");
    } catch (const SerialPortNotOpenException& e) {
        std::cout << "  Caught SerialPortNotOpenException: " << e.what()
                  << "\n";
        g_errorStats.totalErrors++;
    } catch (const SerialException& e) {
        std::cout << "  Caught base SerialException: " << e.what() << "\n";
        g_errorStats.totalErrors++;
    }

    // 2. SerialConfigException
    std::cout << "2. Testing SerialConfigException:\n";
    try {
        SerialConfig config;
        config.setBaudRate(-1);  // Invalid baud rate
    } catch (const SerialConfigException& e) {
        std::cout << "  Caught SerialConfigException: " << e.what() << "\n";
        g_errorStats.configErrors++;
        g_errorStats.totalErrors++;
    } catch (const SerialException& e) {
        std::cout << "  Caught base SerialException: " << e.what() << "\n";
        g_errorStats.totalErrors++;
    }

    // 3. Attempt to open invalid port
    std::cout << "3. Testing port opening errors:\n";
    try {
        SerialConfig config;
        auto error = port.tryOpen("INVALID_PORT_NAME", config);
        if (error) {
            std::cout << "  Port opening failed (expected): " << *error << "\n";
            g_errorStats.openErrors++;
            g_errorStats.totalErrors++;
        }
    } catch (const SerialException& e) {
        std::cout << "  Caught SerialException during open: " << e.what()
                  << "\n";
        g_errorStats.openErrors++;
        g_errorStats.totalErrors++;
    }
}

/**
 * @brief Retry mechanism with exponential backoff
 */
template <typename Func>
bool retryWithBackoff(
    Func func, int maxRetries = 3,
    std::chrono::milliseconds initialDelay = std::chrono::milliseconds(100)) {
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        try {
            func();
            if (attempt > 0) {
                g_errorStats.successfulRecoveries++;
                std::cout << "  Operation succeeded on attempt "
                          << (attempt + 1) << "\n";
            }
            return true;
        } catch (const SerialException& e) {
            g_errorStats.recoveryAttempts++;
            std::cout << "  Attempt " << (attempt + 1)
                      << " failed: " << e.what() << "\n";

            if (attempt < maxRetries - 1) {
                auto delay =
                    initialDelay * (1 << attempt);  // Exponential backoff
                std::cout << "  Retrying in " << delay.count() << "ms...\n";
                std::this_thread::sleep_for(delay);
            }
        }
    }
    return false;
}

/**
 * @brief Demonstrates retry mechanisms
 */
void demonstrateRetryMechanisms() {
    std::cout << "\n=== Retry Mechanisms ===\n";

    SerialPort port;

    std::cout << "Testing retry mechanism with invalid port:\n";
    bool success = retryWithBackoff(
        [&]() {
            SerialConfig config;
            auto error = port.tryOpen("STILL_INVALID_PORT", config);
            if (error) {
                throw SerialException(*error);
            }
        },
        3, std::chrono::milliseconds(50));

    if (!success) {
        std::cout << "  All retry attempts failed (expected)\n";
    }

    // Test with valid operation
    std::cout << "\nTesting retry mechanism with valid operation:\n";
    success = retryWithBackoff(
        [&]() {
            SerialConfig config;
            // This should succeed immediately
            std::cout << "    Executing valid configuration...\n";
        },
        3);

    if (success) {
        std::cout << "  Operation succeeded\n";
    }
}

/**
 * @brief Connection monitor class
 */
class ConnectionMonitor {
private:
    SerialPort* port_;
    std::string portName_;
    SerialConfig config_;
    std::atomic<bool> monitoring_{false};
    std::thread monitorThread_;
    std::function<void(bool)> connectionCallback_;

public:
    ConnectionMonitor(SerialPort* port, const std::string& portName,
                      const SerialConfig& config)
        : port_(port), portName_(portName), config_(config) {}

    ~ConnectionMonitor() { stopMonitoring(); }

    void setConnectionCallback(std::function<void(bool)> callback) {
        connectionCallback_ = callback;
    }

    void startMonitoring(
        std::chrono::milliseconds interval = std::chrono::milliseconds(1000)) {
        if (monitoring_.load()) {
            return;
        }

        monitoring_ = true;
        monitorThread_ = std::thread([this, interval]() {
            while (monitoring_.load()) {
                bool wasConnected = port_->isOpen();
                bool isConnected = checkConnection();

                if (wasConnected != isConnected && connectionCallback_) {
                    connectionCallback_(isConnected);
                }

                if (!isConnected && monitoring_.load()) {
                    attemptReconnection();
                }

                std::this_thread::sleep_for(interval);
            }
        });
    }

    void stopMonitoring() {
        monitoring_ = false;
        if (monitorThread_.joinable()) {
            monitorThread_.join();
        }
    }

private:
    bool checkConnection() {
        if (!port_->isOpen()) {
            return false;
        }

        try {
            // Try a simple operation to verify connection
            port_->available();
            return true;
        } catch (const SerialException&) {
            return false;
        }
    }

    void attemptReconnection() {
        std::cout << "  [Monitor] Attempting reconnection...\n";
        g_errorStats.recoveryAttempts++;

        try {
            if (port_->isOpen()) {
                port_->close();
            }

            auto error = port_->tryOpen(portName_, config_);
            if (!error) {
                std::cout << "  [Monitor] Reconnection successful\n";
                g_errorStats.successfulRecoveries++;
            } else {
                std::cout << "  [Monitor] Reconnection failed: " << *error
                          << "\n";
            }
        } catch (const SerialException& e) {
            std::cout << "  [Monitor] Reconnection exception: " << e.what()
                      << "\n";
        }
    }
};

/**
 * @brief Demonstrates connection monitoring and recovery
 */
void demonstrateConnectionMonitoring() {
    std::cout << "\n=== Connection Monitoring and Recovery ===\n";

    auto ports = SerialPort::getAvailablePorts();
    if (ports.empty()) {
        std::cout << "No ports available for monitoring demonstration.\n";
        return;
    }

    SerialPort port;
    SerialConfig config;
    config.setBaudRate(9600);
    config.setReadTimeout(std::chrono::milliseconds(500));

    std::string selectedPort = ports[0];
    std::cout << "Setting up monitoring for port: " << selectedPort << "\n";

    ConnectionMonitor monitor(&port, selectedPort, config);

    // Set up connection callback
    monitor.setConnectionCallback([](bool connected) {
        if (connected) {
            std::cout << "  [Callback] Connection established\n";
        } else {
            std::cout << "  [Callback] Connection lost\n";
        }
    });

    // Try to open the port initially
    auto error = port.tryOpen(selectedPort, config);
    if (error) {
        std::cout << "Initial connection failed: " << *error << "\n";
    } else {
        std::cout << "Initial connection successful\n";
    }

    // Start monitoring
    std::cout << "Starting connection monitoring...\n";
    monitor.startMonitoring(std::chrono::milliseconds(500));

    // Simulate some operations and potential disconnections
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        try {
            if (port.isOpen()) {
                port.write("Test message " + std::to_string(i) + "\n");
                std::cout << "  Sent test message " << i << "\n";
            }
        } catch (const SerialException& e) {
            std::cout << "  Write failed: " << e.what() << "\n";
            g_errorStats.writeErrors++;
            g_errorStats.totalErrors++;
        }
    }

    std::cout << "Stopping monitoring...\n";
    monitor.stopMonitoring();
}

/**
 * @brief Demonstrates graceful error handling in operations
 */
void demonstrateGracefulErrorHandling() {
    std::cout << "\n=== Graceful Error Handling ===\n";

    SerialPort port;

    // Function to safely perform operations with comprehensive error handling
    auto safeOperation = [&](const std::string& operation,
                             std::function<void()> func) {
        std::cout << "Performing " << operation << "...\n";
        try {
            func();
            std::cout << "  " << operation << " completed successfully\n";
        } catch (const SerialTimeoutException& e) {
            std::cout << "  " << operation << " timed out: " << e.what()
                      << "\n";
            g_errorStats.timeoutErrors++;
            g_errorStats.totalErrors++;
        } catch (const SerialIOException& e) {
            std::cout << "  " << operation << " I/O error: " << e.what()
                      << "\n";
            g_errorStats.ioErrors++;
            g_errorStats.totalErrors++;
        } catch (const SerialPortNotOpenException& e) {
            std::cout << "  " << operation
                      << " failed - port not open: " << e.what() << "\n";
            g_errorStats.openErrors++;
            g_errorStats.totalErrors++;
        } catch (const SerialConfigException& e) {
            std::cout << "  " << operation
                      << " configuration error: " << e.what() << "\n";
            g_errorStats.configErrors++;
            g_errorStats.totalErrors++;
        } catch (const SerialException& e) {
            std::cout << "  " << operation << " general error: " << e.what()
                      << "\n";
            g_errorStats.totalErrors++;
        } catch (const std::exception& e) {
            std::cout << "  " << operation << " unexpected error: " << e.what()
                      << "\n";
            g_errorStats.totalErrors++;
        }
    };

    // Test various operations with error handling
    safeOperation("read operation", [&]() { port.read(100); });

    safeOperation("write operation", [&]() { port.write("test data"); });

    safeOperation("configuration change", [&]() {
        SerialConfig config;
        config.setBaudRate(999999999);  // Likely invalid
        // Note: This might not throw if validation is not strict
    });
}

/**
 * @brief Main function demonstrating error handling and recovery
 */
int main() {
    std::cout << "=== Serial Port Error Handling and Recovery Example ===\n";
    std::cout << "This example demonstrates comprehensive error handling "
                 "strategies.\n";

    // Reset error statistics
    g_errorStats.reset();

    // Demonstrate different aspects of error handling
    demonstrateExceptionTypes();
    demonstrateRetryMechanisms();
    demonstrateConnectionMonitoring();
    demonstrateGracefulErrorHandling();

    // Print final statistics
    std::cout << "\n=== Final Error Statistics ===\n";
    g_errorStats.print();

    std::cout << "\n=== Error Handling Best Practices ===\n";
    std::cout
        << "1. Always use specific exception types for targeted handling\n";
    std::cout << "2. Implement retry mechanisms with exponential backoff\n";
    std::cout
        << "3. Monitor connection status and implement auto-reconnection\n";
    std::cout << "4. Log errors with sufficient detail for debugging\n";
    std::cout << "5. Clean up resources properly in error scenarios\n";
    std::cout << "6. Provide graceful degradation when possible\n";
    std::cout << "7. Use RAII principles for automatic resource management\n";
    std::cout << "8. Validate configurations before applying them\n";
    std::cout << "9. Handle timeouts appropriately for your use case\n";
    std::cout << "10. Test error scenarios during development\n";

    std::cout << "\n=== Example Complete ===\n";

    return 0;
}
