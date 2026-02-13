/**
 * @file basic_serial_communication.cpp
 * @brief Basic serial port communication example
 *
 * This example demonstrates fundamental serial port operations including:
 * - Opening and configuring serial ports
 * - Basic read and write operations
 * - Proper error handling and resource management
 * - Port enumeration and selection
 *
 * @author Atom Serial Examples
 * @date 2024
 */

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "atom/serial/core/serial_port.hpp"

using namespace atom::serial;

/**
 * @brief Demonstrates basic serial port enumeration
 */
void demonstratePortEnumeration() {
    std::cout << "\n=== Serial Port Enumeration ===\n";

    try {
        // Get list of available serial ports
        auto ports = SerialPort::getAvailablePorts();

        if (ports.empty()) {
            std::cout << "No serial ports found on this system.\n";
            return;
        }

        std::cout << "Available serial ports:\n";
        for (size_t i = 0; i < ports.size(); ++i) {
            std::cout << "  [" << i << "] " << ports[i] << "\n";
        }

    } catch (const SerialException& e) {
        std::cerr << "Error enumerating ports: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates basic serial port configuration
 */
void demonstrateBasicConfiguration() {
    std::cout << "\n=== Basic Serial Configuration ===\n";

    // Create a serial configuration with common settings
    SerialConfig config;
    config.setBaudRate(9600);                  // Standard baud rate
    config.setDataBits(8);                     // 8 data bits
    config.setParity(Parity::None);            // No parity
    config.setStopBits(StopBits::One);         // 1 stop bit
    config.setFlowControl(FlowControl::None);  // No flow control
    config.setReadTimeout(
        std::chrono::milliseconds(1000));  // 1 second read timeout
    config.setWriteTimeout(
        std::chrono::milliseconds(1000));  // 1 second write timeout

    std::cout << "Configuration created:\n";
    std::cout << "  Baud Rate: " << config.getBaudRate() << "\n";
    std::cout << "  Data Bits: " << config.getDataBits() << "\n";
    std::cout << "  Parity: "
              << (config.getParity() == Parity::None   ? "None"
                  : config.getParity() == Parity::Even ? "Even"
                                                       : "Odd")
              << "\n";
    std::cout << "  Stop Bits: "
              << (config.getStopBits() == StopBits::One ? "1" : "2") << "\n";
    std::cout << "  Flow Control: "
              << (config.getFlowControl() == FlowControl::None ? "None"
                  : config.getFlowControl() == FlowControl::Hardware
                      ? "Hardware"
                      : "Software")
              << "\n";
    std::cout << "  Read Timeout: " << config.getReadTimeout().count()
              << "ms\n";
    std::cout << "  Write Timeout: " << config.getWriteTimeout().count()
              << "ms\n";
}

/**
 * @brief Demonstrates safe port opening with error handling
 */
bool demonstratePortOpening(const std::string& portName, SerialPort& port) {
    std::cout << "\n=== Opening Serial Port ===\n";
    std::cout << "Attempting to open port: " << portName << "\n";

    try {
        // Create configuration
        SerialConfig config;
        config.setBaudRate(9600);
        config.setDataBits(8);
        config.setParity(Parity::None);
        config.setStopBits(StopBits::One);
        config.setFlowControl(FlowControl::None);
        config.setReadTimeout(std::chrono::milliseconds(500));
        config.setWriteTimeout(std::chrono::milliseconds(500));

        // Try to open the port
        auto error = port.tryOpen(portName, config);
        if (error) {
            std::cerr << "Failed to open port: " << *error << "\n";
            return false;
        }

        std::cout << "Port opened successfully!\n";
        std::cout << "Port is open: " << (port.isOpen() ? "Yes" : "No") << "\n";

        return true;

    } catch (const SerialException& e) {
        std::cerr << "Exception opening port: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Demonstrates basic write operations
 */
void demonstrateWriteOperations(SerialPort& port) {
    std::cout << "\n=== Write Operations ===\n";

    if (!port.isOpen()) {
        std::cout << "Port is not open, skipping write operations.\n";
        return;
    }

    try {
        // Write a simple string
        std::string message = "Hello, Serial World!\n";
        size_t bytesWritten = port.write(message);
        std::cout << "Wrote " << bytesWritten << " bytes: \""
                  << message.substr(0, message.length() - 1) << "\"\n";

        // Write binary data
        std::vector<uint8_t> binaryData = {0x01, 0x02, 0x03, 0x04, 0x05};
        bytesWritten = port.write(std::span<const uint8_t>(binaryData));
        std::cout << "Wrote " << bytesWritten << " bytes of binary data\n";

        // Write a serializable object (example with int)
        int value = 42;
        bytesWritten = port.writeObject(value);
        std::cout << "Wrote " << bytesWritten
                  << " bytes (integer value: " << value << ")\n";

        // Flush the output to ensure data is sent
        port.flush();
        std::cout << "Output flushed\n";

    } catch (const SerialException& e) {
        std::cerr << "Error during write operations: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates basic read operations
 */
void demonstrateReadOperations(SerialPort& port) {
    std::cout << "\n=== Read Operations ===\n";

    if (!port.isOpen()) {
        std::cout << "Port is not open, skipping read operations.\n";
        return;
    }

    try {
        // Check how many bytes are available
        size_t available = port.available();
        std::cout << "Bytes available to read: " << available << "\n";

        if (available > 0) {
            // Read available data
            auto data = port.readAvailable();
            std::cout << "Read " << data.size() << " bytes: ";
            for (uint8_t byte : data) {
                if (byte >= 32 && byte <= 126) {
                    std::cout << static_cast<char>(byte);
                } else {
                    std::cout << "[0x" << std::hex << static_cast<int>(byte)
                              << std::dec << "]";
                }
            }
            std::cout << "\n";
        }

        // Try to read a specific number of bytes (with timeout)
        std::cout << "Attempting to read up to 10 bytes...\n";
        auto readData = port.read(10);
        if (!readData.empty()) {
            std::cout << "Read " << readData.size() << " bytes\n";
        } else {
            std::cout << "No data received (timeout or no data available)\n";
        }

    } catch (const SerialTimeoutException& e) {
        std::cout << "Read operation timed out: " << e.what() << "\n";
    } catch (const SerialException& e) {
        std::cerr << "Error during read operations: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates proper port closing and cleanup
 */
void demonstratePortClosing(SerialPort& port) {
    std::cout << "\n=== Closing Serial Port ===\n";

    if (port.isOpen()) {
        try {
            port.close();
            std::cout << "Port closed successfully\n";
            std::cout << "Port is open: " << (port.isOpen() ? "Yes" : "No")
                      << "\n";
        } catch (const SerialException& e) {
            std::cerr << "Error closing port: " << e.what() << "\n";
        }
    } else {
        std::cout << "Port was already closed\n";
    }
}

/**
 * @brief Main function demonstrating complete serial communication workflow
 */
int main() {
    std::cout << "=== Basic Serial Port Communication Example ===\n";
    std::cout
        << "This example demonstrates fundamental serial port operations.\n";

    // Step 1: Enumerate available ports
    demonstratePortEnumeration();

    // Step 2: Show basic configuration
    demonstrateBasicConfiguration();

    // Step 3: Get available ports for testing
    auto ports = SerialPort::getAvailablePorts();
    if (ports.empty()) {
        std::cout << "\nNo serial ports available for testing.\n";
        std::cout << "Connect a serial device or use a virtual serial port for "
                     "full demonstration.\n";
        return 0;
    }

    // Step 4: Try to open the first available port
    SerialPort port;
    std::string selectedPort = ports[0];

    std::cout << "\nUsing port: " << selectedPort << " for demonstration\n";

    if (demonstratePortOpening(selectedPort, port)) {
        // Step 5: Demonstrate write operations
        demonstrateWriteOperations(port);

        // Small delay to allow data processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Step 6: Demonstrate read operations
        demonstrateReadOperations(port);

        // Step 7: Properly close the port
        demonstratePortClosing(port);
    }

    std::cout << "\n=== Example Complete ===\n";
    std::cout << "This example covered:\n";
    std::cout << "- Port enumeration and selection\n";
    std::cout << "- Basic configuration setup\n";
    std::cout << "- Safe port opening with error handling\n";
    std::cout << "- Write operations (string, binary, objects)\n";
    std::cout << "- Read operations (available data, specific amounts)\n";
    std::cout << "- Proper resource cleanup\n";

    return 0;
}
