/*
 * ttybase_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-12

Description: Comprehensive example usage of the TTYBase class.
Demonstrates all features including:
- Connection to TTY devices
- Synchronous read/write operations
- Asynchronous read/write operations
- Read until stop byte
- Error handling and messages
- Debug mode
- Connection state management

**************************************************/

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "atom/connection/serial/ttybase.hpp"

using namespace atom::connection;

namespace {

// Utility class for formatted logging
class Logger {
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
                  << "." << std::setfill('0') << std::setw(3) << ms.count()
                  << "] ";

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

// Helper to convert TTYResponse to string
std::string responseToString(TTYBase::TTYResponse response) {
    switch (response) {
        case TTYBase::TTYResponse::OK:
            return "OK";
        case TTYBase::TTYResponse::ReadError:
            return "ReadError";
        case TTYBase::TTYResponse::WriteError:
            return "WriteError";
        case TTYBase::TTYResponse::SelectError:
            return "SelectError";
        case TTYBase::TTYResponse::Timeout:
            return "Timeout";
        case TTYBase::TTYResponse::PortFailure:
            return "PortFailure";
        case TTYBase::TTYResponse::ParamError:
            return "ParamError";
        case TTYBase::TTYResponse::Errno:
            return "Errno";
        case TTYBase::TTYResponse::Overflow:
            return "Overflow";
        default:
            return "Unknown";
    }
}

// Get platform-specific default device
std::string getDefaultDevice() {
#ifdef _WIN32
    return "COM1";
#elif defined(__APPLE__)
    return "/dev/tty.usbserial";
#else
    return "/dev/ttyUSB0";
#endif
}

}  // namespace

// Derived class for demonstration
class SerialDevice : public TTYBase {
public:
    explicit SerialDevice(std::string_view driverName) : TTYBase(driverName) {}

    // High-level connect method
    bool connectToDevice(const std::string& device, uint32_t baudRate = 9600) {
        Logger::log(Logger::LOG_INFO, "SerialDevice",
                    "Connecting to " + device + " at " +
                        std::to_string(baudRate) + " baud");

        auto response = connect(device, baudRate, 8, 0, 1);
        if (response == TTYResponse::OK) {
            Logger::log(Logger::LOG_SUCCESS, "SerialDevice",
                        "Connected successfully");
            return true;
        } else {
            Logger::log(Logger::LOG_ERR, "SerialDevice",
                        "Connection failed: " + getErrorMessage(response));
            return false;
        }
    }

    // High-level send method
    bool sendData(const std::string& data) {
        uint32_t bytesWritten = 0;
        auto response = writeString(data, bytesWritten);

        if (response == TTYResponse::OK) {
            Logger::log(Logger::LOG_SUCCESS, "SerialDevice",
                        "Sent " + std::to_string(bytesWritten) + " bytes");
            return true;
        } else {
            Logger::log(Logger::LOG_ERR, "SerialDevice",
                        "Send failed: " + getErrorMessage(response));
            return false;
        }
    }

    // High-level receive method
    std::string receiveData(size_t maxSize, uint8_t timeout) {
        std::vector<uint8_t> buffer(maxSize);
        uint32_t bytesRead = 0;

        auto response = read(std::span<uint8_t>(buffer), timeout, bytesRead);

        if (response == TTYResponse::OK) {
            Logger::log(Logger::LOG_SUCCESS, "SerialDevice",
                        "Received " + std::to_string(bytesRead) + " bytes");
            return std::string(buffer.begin(), buffer.begin() + bytesRead);
        } else {
            Logger::log(Logger::LOG_WARNING, "SerialDevice",
                        "Receive: " + getErrorMessage(response));
            return "";
        }
    }
};

// Example 1: Basic TTYBase usage
void basicUsageExample() {
    Logger::log(Logger::LOG_INFO, "Example1", "=== Basic TTYBase Usage ===");

    try {
        // Create TTY instance
        SerialDevice device("ExampleDriver");

        Logger::log(Logger::LOG_INFO, "Example1",
                    "Created TTYBase instance, isConnected: " +
                        std::string(device.isConnected() ? "yes" : "no"));

        // Get port file descriptor (will be -1 if not connected)
        Logger::log(Logger::LOG_INFO, "Example1",
                    "Port FD: " + std::to_string(device.getPortFD()));

        // Try to connect (will likely fail without actual device)
        std::string devicePath = getDefaultDevice();
        Logger::log(Logger::LOG_INFO, "Example1",
                    "Attempting connection to: " + devicePath);

        auto response = device.connect(devicePath, 9600, 8, 0, 1);
        Logger::log(Logger::LOG_INFO, "Example1",
                    "Connect result: " + responseToString(response));

        if (device.isConnected()) {
            Logger::log(Logger::LOG_SUCCESS, "Example1", "Device connected!");

            // Disconnect
            auto disconnectResponse = device.disconnect();
            Logger::log(
                Logger::LOG_INFO, "Example1",
                "Disconnect result: " + responseToString(disconnectResponse));
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example1",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example1",
                "Basic usage example completed\n");
}

// Example 2: Connection parameters
void connectionParametersExample() {
    Logger::log(Logger::LOG_INFO, "Example2", "=== Connection Parameters ===");

    try {
        SerialDevice device("ParamTestDriver");

        // Common baud rates
        std::vector<uint32_t> baudRates = {9600, 19200, 38400, 57600, 115200};

        Logger::log(Logger::LOG_INFO, "Example2", "Common baud rates:");
        for (auto rate : baudRates) {
            Logger::log(Logger::LOG_INFO, "Example2",
                        "  - " + std::to_string(rate) + " baud");
        }

        // Word sizes
        Logger::log(Logger::LOG_INFO, "Example2",
                    "Word sizes: 5, 6, 7, 8 bits");

        // Parity options
        Logger::log(Logger::LOG_INFO, "Example2",
                    "Parity: 0=None, 1=Odd, 2=Even");

        // Stop bits
        Logger::log(Logger::LOG_INFO, "Example2", "Stop bits: 1 or 2");

        // Example connection attempt with different parameters
        std::string devicePath = getDefaultDevice();

        struct ConnectionParams {
            uint32_t baudRate;
            uint8_t wordSize;
            uint8_t parity;
            uint8_t stopBits;
            std::string description;
        };

        std::vector<ConnectionParams> configs = {
            {9600, 8, 0, 1, "Standard 9600-8-N-1"},
            {115200, 8, 0, 1, "Fast 115200-8-N-1"},
            {9600, 7, 2, 1, "7-bit even parity"},
            {19200, 8, 1, 2, "Odd parity, 2 stop bits"}};

        for (const auto& config : configs) {
            Logger::log(Logger::LOG_INFO, "Example2",
                        "Config: " + config.description);
            Logger::log(Logger::LOG_DEBUG, "Example2",
                        "  Baud=" + std::to_string(config.baudRate) +
                            ", Word=" + std::to_string(config.wordSize) +
                            ", Parity=" + std::to_string(config.parity) +
                            ", Stop=" + std::to_string(config.stopBits));

            auto response =
                device.connect(devicePath, config.baudRate, config.wordSize,
                               config.parity, config.stopBits);

            Logger::log(Logger::LOG_INFO, "Example2",
                        "  Result: " + responseToString(response));

            if (device.isConnected()) {
                device.disconnect();
            }
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example2",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example2",
                "Connection parameters example completed\n");
}

// Example 3: Synchronous read/write operations
void syncReadWriteExample() {
    Logger::log(Logger::LOG_INFO, "Example3", "=== Synchronous Read/Write ===");

    try {
        SerialDevice device("SyncIODriver");

        std::string devicePath = getDefaultDevice();
        auto connectResponse = device.connect(devicePath, 9600, 8, 0, 1);

        if (connectResponse != TTYBase::TTYResponse::OK) {
            Logger::log(Logger::LOG_WARNING, "Example3",
                        "Could not connect to device (simulating operations)");
        }

        // Demonstrate write operation
        Logger::log(Logger::LOG_INFO, "Example3", "--- Write Operations ---");

        // Write using span
        std::vector<uint8_t> binaryData = {0x01, 0x02, 0x03, 0x04, 0x05};
        uint32_t bytesWritten = 0;

        auto writeResponse =
            device.write(std::span<const uint8_t>(binaryData), bytesWritten);
        Logger::log(Logger::LOG_INFO, "Example3",
                    "Binary write result: " + responseToString(writeResponse) +
                        " (" + std::to_string(bytesWritten) + " bytes)");

        // Write string
        std::string textData = "Hello Serial Port!";
        auto stringWriteResponse = device.writeString(textData, bytesWritten);
        Logger::log(
            Logger::LOG_INFO, "Example3",
            "String write result: " + responseToString(stringWriteResponse) +
                " (" + std::to_string(bytesWritten) + " bytes)");

        // Demonstrate read operation
        Logger::log(Logger::LOG_INFO, "Example3", "--- Read Operations ---");

        std::vector<uint8_t> readBuffer(256);
        uint32_t bytesRead = 0;

        auto readResponse =
            device.read(std::span<uint8_t>(readBuffer), 2, bytesRead);
        Logger::log(Logger::LOG_INFO, "Example3",
                    "Read result: " + responseToString(readResponse) + " (" +
                        std::to_string(bytesRead) + " bytes)");

        if (device.isConnected()) {
            device.disconnect();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example3",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example3",
                "Sync read/write example completed\n");
}

// Example 4: Asynchronous operations
void asyncOperationsExample() {
    Logger::log(Logger::LOG_INFO, "Example4",
                "=== Asynchronous Operations ===");

    try {
        SerialDevice device("AsyncIODriver");

        std::string devicePath = getDefaultDevice();
        device.connect(devicePath, 9600, 8, 0, 1);

        // Async write
        Logger::log(Logger::LOG_INFO, "Example4", "--- Async Write ---");

        std::vector<uint8_t> writeData = {0xAA, 0xBB, 0xCC, 0xDD};
        auto writeFuture =
            device.writeAsync(std::span<const uint8_t>(writeData));

        Logger::log(Logger::LOG_INFO, "Example4",
                    "Async write initiated, waiting...");

        if (writeFuture.wait_for(std::chrono::seconds(5)) ==
            std::future_status::ready) {
            auto [response, bytesWritten] = writeFuture.get();
            Logger::log(Logger::LOG_INFO, "Example4",
                        "Async write completed: " + responseToString(response) +
                            " (" + std::to_string(bytesWritten) + " bytes)");
        } else {
            Logger::log(Logger::LOG_WARNING, "Example4",
                        "Async write timed out");
        }

        // Async read
        Logger::log(Logger::LOG_INFO, "Example4", "--- Async Read ---");

        std::vector<uint8_t> readBuffer(128);
        auto readFuture = device.readAsync(std::span<uint8_t>(readBuffer), 2);

        Logger::log(Logger::LOG_INFO, "Example4",
                    "Async read initiated, waiting...");

        if (readFuture.wait_for(std::chrono::seconds(5)) ==
            std::future_status::ready) {
            auto [response, bytesRead] = readFuture.get();
            Logger::log(Logger::LOG_INFO, "Example4",
                        "Async read completed: " + responseToString(response) +
                            " (" + std::to_string(bytesRead) + " bytes)");
        } else {
            Logger::log(Logger::LOG_WARNING, "Example4",
                        "Async read timed out");
        }

        if (device.isConnected()) {
            device.disconnect();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example4",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example4",
                "Async operations example completed\n");
}

// Example 5: Read until stop byte
void readSectionExample() {
    Logger::log(Logger::LOG_INFO, "Example5",
                "=== Read Section (Until Stop Byte) ===");

    try {
        SerialDevice device("SectionReadDriver");

        std::string devicePath = getDefaultDevice();
        device.connect(devicePath, 9600, 8, 0, 1);

        // Read until newline character
        Logger::log(Logger::LOG_INFO, "Example5",
                    "Reading until newline (0x0A)...");

        std::vector<uint8_t> buffer(256);
        uint32_t bytesRead = 0;
        uint8_t stopByte = '\n';  // 0x0A

        auto response = device.readSection(std::span<uint8_t>(buffer), stopByte,
                                           5, bytesRead);

        Logger::log(Logger::LOG_INFO, "Example5",
                    "Read section result: " + responseToString(response) +
                        " (" + std::to_string(bytesRead) + " bytes)");

        if (bytesRead > 0) {
            std::string data(buffer.begin(), buffer.begin() + bytesRead);
            Logger::log(Logger::LOG_INFO, "Example5", "Data received: " + data);
        }

        // Read until carriage return
        Logger::log(Logger::LOG_INFO, "Example5", "Reading until CR (0x0D)...");

        stopByte = '\r';  // 0x0D
        response = device.readSection(std::span<uint8_t>(buffer), stopByte, 5,
                                      bytesRead);

        Logger::log(Logger::LOG_INFO, "Example5",
                    "Read section result: " + responseToString(response));

        // Read until custom delimiter
        Logger::log(Logger::LOG_INFO, "Example5",
                    "Reading until ETX (0x03)...");

        stopByte = 0x03;  // ETX (End of Text)
        response = device.readSection(std::span<uint8_t>(buffer), stopByte, 5,
                                      bytesRead);

        Logger::log(Logger::LOG_INFO, "Example5",
                    "Read section result: " + responseToString(response));

        if (device.isConnected()) {
            device.disconnect();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example5",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example5",
                "Read section example completed\n");
}

// Example 6: Error handling
void errorHandlingExample() {
    Logger::log(Logger::LOG_INFO, "Example6", "=== Error Handling ===");

    try {
        SerialDevice device("ErrorTestDriver");

        // Get error messages for all response types
        Logger::log(Logger::LOG_INFO, "Example6",
                    "Error messages for all response types:");

        std::vector<TTYBase::TTYResponse> responses = {
            TTYBase::TTYResponse::OK,         TTYBase::TTYResponse::ReadError,
            TTYBase::TTYResponse::WriteError, TTYBase::TTYResponse::SelectError,
            TTYBase::TTYResponse::Timeout,    TTYBase::TTYResponse::PortFailure,
            TTYBase::TTYResponse::ParamError, TTYBase::TTYResponse::Errno,
            TTYBase::TTYResponse::Overflow};

        for (auto response : responses) {
            std::string errorMsg = device.getErrorMessage(response);
            Logger::log(Logger::LOG_INFO, "Example6",
                        "  " + responseToString(response) + ": " + errorMsg);
        }

        // Demonstrate error scenarios
        Logger::log(Logger::LOG_INFO, "Example6", "\n--- Error Scenarios ---");

        // Invalid device path
        auto response =
            device.connect("/dev/nonexistent_device", 9600, 8, 0, 1);
        Logger::log(Logger::LOG_INFO, "Example6",
                    "Invalid device: " + responseToString(response) + " - " +
                        device.getErrorMessage(response));

        // Operations on disconnected device
        std::vector<uint8_t> buffer(64);
        uint32_t bytes = 0;

        response = device.read(std::span<uint8_t>(buffer), 1, bytes);
        Logger::log(Logger::LOG_INFO, "Example6",
                    "Read when disconnected: " + responseToString(response));

        response = device.write(std::span<const uint8_t>(buffer), bytes);
        Logger::log(Logger::LOG_INFO, "Example6",
                    "Write when disconnected: " + responseToString(response));

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example6",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example6",
                "Error handling example completed\n");
}

// Example 7: Debug mode
void debugModeExample() {
    Logger::log(Logger::LOG_INFO, "Example7", "=== Debug Mode ===");

    try {
        SerialDevice device("DebugDriver");

        // Enable debug mode
        Logger::log(Logger::LOG_INFO, "Example7", "Enabling debug mode...");
        device.setDebug(true);

        // Perform operations with debug enabled
        std::string devicePath = getDefaultDevice();
        device.connect(devicePath, 9600, 8, 0, 1);

        std::vector<uint8_t> buffer(64);
        uint32_t bytes = 0;
        device.read(std::span<uint8_t>(buffer), 1, bytes);

        // Disable debug mode
        Logger::log(Logger::LOG_INFO, "Example7", "Disabling debug mode...");
        device.setDebug(false);

        // Operations without debug
        device.read(std::span<uint8_t>(buffer), 1, bytes);

        if (device.isConnected()) {
            device.disconnect();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example7",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example7", "Debug mode example completed\n");
}

// Example 8: Using makeByteSpan helper
void byteSpanHelperExample() {
    Logger::log(Logger::LOG_INFO, "Example8", "=== makeByteSpan Helper ===");

    try {
        SerialDevice device("ByteSpanDriver");

        // Using makeByteSpan with different container types
        Logger::log(Logger::LOG_INFO, "Example8",
                    "Creating byte spans from containers:");

        // From vector<char>
        std::vector<char> charVec = {'H', 'e', 'l', 'l', 'o'};
        auto charSpan = makeByteSpan(charVec);
        Logger::log(
            Logger::LOG_INFO, "Example8",
            "vector<char> span size: " + std::to_string(charSpan.size()));

        // From vector<uint8_t>
        std::vector<uint8_t> byteVec = {0x01, 0x02, 0x03, 0x04};
        auto byteSpan = makeByteSpan(byteVec);
        Logger::log(
            Logger::LOG_INFO, "Example8",
            "vector<uint8_t> span size: " + std::to_string(byteSpan.size()));

        // From array
        std::array<uint8_t, 8> byteArray = {0xAA, 0xBB, 0xCC, 0xDD,
                                            0xEE, 0xFF, 0x00, 0x11};
        auto arraySpan = makeByteSpan(byteArray);
        Logger::log(
            Logger::LOG_INFO, "Example8",
            "array<uint8_t, 8> span size: " + std::to_string(arraySpan.size()));

        // Use spans for read/write operations
        std::string devicePath = getDefaultDevice();
        if (device.connect(devicePath, 9600, 8, 0, 1) ==
            TTYBase::TTYResponse::OK) {
            uint32_t bytesWritten = 0;
            device.write(byteSpan, bytesWritten);
            Logger::log(Logger::LOG_INFO, "Example8",
                        "Wrote using makeByteSpan: " +
                            std::to_string(bytesWritten) + " bytes");

            device.disconnect();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example8",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example8",
                "Byte span helper example completed\n");
}

// Example 9: Move semantics
void moveSemanticsExample() {
    Logger::log(Logger::LOG_INFO, "Example9", "=== Move Semantics ===");

    try {
        // Create original device
        SerialDevice device1("MoveTestDriver1");

        std::string devicePath = getDefaultDevice();
        device1.connect(devicePath, 9600, 8, 0, 1);

        Logger::log(Logger::LOG_INFO, "Example9",
                    "device1 connected: " +
                        std::string(device1.isConnected() ? "yes" : "no"));
        Logger::log(Logger::LOG_INFO, "Example9",
                    "device1 FD: " + std::to_string(device1.getPortFD()));

        // Move to new device
        SerialDevice device2 = std::move(device1);

        Logger::log(Logger::LOG_INFO, "Example9", "After move:");
        Logger::log(Logger::LOG_INFO, "Example9",
                    "device2 connected: " +
                        std::string(device2.isConnected() ? "yes" : "no"));
        Logger::log(Logger::LOG_INFO, "Example9",
                    "device2 FD: " + std::to_string(device2.getPortFD()));

        // Move assignment
        SerialDevice device3("MoveTestDriver3");
        device3 = std::move(device2);

        Logger::log(Logger::LOG_INFO, "Example9", "After move assignment:");
        Logger::log(Logger::LOG_INFO, "Example9",
                    "device3 connected: " +
                        std::string(device3.isConnected() ? "yes" : "no"));

        if (device3.isConnected()) {
            device3.disconnect();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example9",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example9",
                "Move semantics example completed\n");
}

// Example 10: Complete serial communication workflow
void completeWorkflowExample() {
    Logger::log(Logger::LOG_INFO, "Example10",
                "=== Complete Serial Communication Workflow ===");

    try {
        SerialDevice device("WorkflowDriver");

        // Step 1: Configure and connect
        Logger::log(Logger::LOG_INFO, "Example10", "Step 1: Connecting...");
        std::string devicePath = getDefaultDevice();

        if (!device.connectToDevice(devicePath, 115200)) {
            Logger::log(Logger::LOG_WARNING, "Example10",
                        "Could not connect (simulating workflow)");
        }

        // Step 2: Enable debug for troubleshooting
        Logger::log(Logger::LOG_INFO, "Example10",
                    "Step 2: Enabling debug mode...");
        device.setDebug(true);

        // Step 3: Send initialization command
        Logger::log(Logger::LOG_INFO, "Example10",
                    "Step 3: Sending init command...");
        device.sendData("AT\r\n");

        // Step 4: Wait for response
        Logger::log(Logger::LOG_INFO, "Example10",
                    "Step 4: Waiting for response...");
        std::string response = device.receiveData(256, 5);

        if (!response.empty()) {
            Logger::log(Logger::LOG_SUCCESS, "Example10",
                        "Received: " + response);
        }

        // Step 5: Send data command
        Logger::log(Logger::LOG_INFO, "Example10",
                    "Step 5: Sending data command...");
        device.sendData("ATI\r\n");

        // Step 6: Read response
        Logger::log(Logger::LOG_INFO, "Example10",
                    "Step 6: Reading response...");
        response = device.receiveData(1024, 5);

        // Step 7: Disable debug
        Logger::log(Logger::LOG_INFO, "Example10",
                    "Step 7: Disabling debug mode...");
        device.setDebug(false);

        // Step 8: Disconnect
        Logger::log(Logger::LOG_INFO, "Example10", "Step 8: Disconnecting...");
        if (device.isConnected()) {
            device.disconnect();
        }

        Logger::log(Logger::LOG_SUCCESS, "Example10",
                    "Workflow completed successfully");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example10",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example10",
                "Complete workflow example completed\n");
}

int main() {
    Logger::log(Logger::LOG_INFO, "Main",
                "========================================");
    Logger::log(Logger::LOG_INFO, "Main", "  TTYBase Comprehensive Examples");
    Logger::log(Logger::LOG_INFO, "Main",
                "========================================");
    Logger::log(Logger::LOG_WARNING, "Main",
                "Note: Most examples require actual serial hardware");
    Logger::log(Logger::LOG_INFO, "Main",
                "Default device: " + getDefaultDevice());
    Logger::log(Logger::LOG_INFO, "Main", "");

    // Run all examples
    basicUsageExample();
    connectionParametersExample();
    syncReadWriteExample();
    asyncOperationsExample();
    readSectionExample();
    errorHandlingExample();
    debugModeExample();
    byteSpanHelperExample();
    moveSemanticsExample();
    completeWorkflowExample();

    Logger::log(Logger::LOG_SUCCESS, "Main",
                "========================================");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "  All TTYBase examples completed!");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "========================================");

    return 0;
}
