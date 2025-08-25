/**
 * @file ch340_detection.cpp
 * @brief CH340 device detection and identification example
 *
 * This example demonstrates specialized CH340 device handling including:
 * - CH340 device identification and model detection
 * - CH340-specific configuration recommendations
 * - Common CH340 issues and troubleshooting
 * - Performance characteristics of different CH340 models
 * - Driver compatibility and version checking
 * - CH340 vs other USB-to-serial adapter comparison
 *
 * @author Atom Serial Examples
 * @date 2024
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>
#include "atom/serial/core/scanner.hpp"
#include "atom/serial/core/serial_port.hpp"

using namespace atom::serial;

/**
 * @brief CH340 model information database
 */
struct CH340ModelInfo {
    std::string model;
    std::string description;
    std::vector<int> supportedBaudRates;
    std::string commonIssues;
    std::string recommendations;
};

/**
 * @brief Database of known CH340 models and their characteristics
 */
std::map<std::string, CH340ModelInfo> ch340Database = {
    {"CH340G",
     {"CH340G",
      "Most common CH340 variant, USB to UART bridge",
      {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200},
      "May have issues with high baud rates, driver compatibility on some "
      "systems",
      "Use standard baud rates, ensure proper driver installation"}},
    {"CH340C",
     {"CH340C",
      "Compact version of CH340, similar functionality to CH340G",
      {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200},
      "Similar to CH340G, may have power consumption differences",
      "Good for low-power applications, standard baud rates recommended"}},
    {"CH340T",
     {"CH340T",
      "CH340 with built-in crystal oscillator",
      {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200},
      "Generally more stable than external crystal versions",
      "Preferred choice for stable communication"}},
    {"CH340E",
     {"CH340E",
      "Enhanced version with better EMI performance",
      {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400},
      "Better EMI characteristics, may support higher baud rates",
      "Good choice for noisy environments"}},
    {"CH340N",
     {"CH340N",
      "Newer variant with improved features",
      {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400,
       460800},
      "Generally more reliable, better driver support",
      "Recommended for new designs, supports higher baud rates"}}};

/**
 * @brief Demonstrates CH340 device detection and identification
 */
void demonstrateCH340Detection() {
    std::cout << "\n=== CH340 Device Detection ===\n";

    // Configure scanner for CH340 detection
    ScannerConfig config;
    config.detect_ch340 = true;
    config.include_virtual_ports = false;  // Focus on physical devices
    config.scan_timeout = std::chrono::milliseconds(3000);

    SerialPortScanner scanner(config);

    std::cout << "Scanning for CH340 devices...\n";
    auto result = scanner.list_available_ports();

    if (std::holds_alternative<std::vector<SerialPortScanner::PortInfo>>(
            result)) {
        const auto& ports =
            std::get<std::vector<SerialPortScanner::PortInfo>>(result);

        std::vector<SerialPortScanner::PortInfo> ch340Ports;
        for (const auto& port : ports) {
            if (port.is_ch340) {
                ch340Ports.push_back(port);
            }
        }

        if (ch340Ports.empty()) {
            std::cout << "No CH340 devices found.\n";
            std::cout << "Connect a CH340-based USB-to-serial adapter to see "
                         "detection in action.\n";
            return;
        }

        std::cout << "Found " << ch340Ports.size() << " CH340 device(s):\n\n";

        for (size_t i = 0; i < ch340Ports.size(); ++i) {
            const auto& port = ch340Ports[i];
            std::cout << "CH340 Device " << (i + 1) << ":\n";
            std::cout << "  Port: " << port.device << "\n";
            std::cout << "  Description: " << port.description << "\n";
            std::cout << "  Model: " << port.ch340_model << "\n";
            std::cout << "  Hardware ID: " << port.hardware_id << "\n";
            std::cout << "  VID:PID: " << port.vendor_id << ":"
                      << port.product_id << "\n";

            if (!port.manufacturer.empty()) {
                std::cout << "  Manufacturer: " << port.manufacturer << "\n";
            }
            if (!port.serial_number.empty()) {
                std::cout << "  Serial Number: " << port.serial_number << "\n";
            }

            // Get detailed information
            auto detailsResult = scanner.get_port_details(port.device);
            if (std::holds_alternative<
                    std::optional<SerialPortScanner::PortDetails>>(
                    detailsResult)) {
                const auto& maybeDetails =
                    std::get<std::optional<SerialPortScanner::PortDetails>>(
                        detailsResult);
                if (maybeDetails && maybeDetails->is_ch340) {
                    std::cout << "  Recommended Baud Rates: "
                              << maybeDetails->recommended_baud_rates << "\n";
                    if (!maybeDetails->notes.empty()) {
                        std::cout << "  Notes: " << maybeDetails->notes << "\n";
                    }
                }
            }

            std::cout << "\n";
        }

    } else {
        const auto& error = std::get<SerialPortScanner::ErrorInfo>(result);
        std::cerr << "Error scanning for devices: " << error.message << "\n";
    }
}

/**
 * @brief Demonstrates CH340 model-specific information
 */
void demonstrateCH340ModelInfo() {
    std::cout << "\n=== CH340 Model Information Database ===\n";

    std::cout << "Known CH340 models and their characteristics:\n\n";

    for (const auto& [model, info] : ch340Database) {
        std::cout << "Model: " << info.model << "\n";
        std::cout << "  Description: " << info.description << "\n";

        std::cout << "  Supported Baud Rates: ";
        for (size_t i = 0; i < info.supportedBaudRates.size(); ++i) {
            std::cout << info.supportedBaudRates[i];
            if (i < info.supportedBaudRates.size() - 1)
                std::cout << ", ";
        }
        std::cout << "\n";

        std::cout << "  Common Issues: " << info.commonIssues << "\n";
        std::cout << "  Recommendations: " << info.recommendations << "\n\n";
    }
}

/**
 * @brief Demonstrates CH340 configuration testing
 */
void demonstrateCH340Configuration() {
    std::cout << "\n=== CH340 Configuration Testing ===\n";

    // Find CH340 devices first
    ScannerConfig config;
    config.detect_ch340 = true;
    SerialPortScanner scanner(config);

    auto result = scanner.list_available_ports();
    if (!std::holds_alternative<std::vector<SerialPortScanner::PortInfo>>(
            result)) {
        std::cout << "Could not scan for devices\n";
        return;
    }

    const auto& ports =
        std::get<std::vector<SerialPortScanner::PortInfo>>(result);
    std::string ch340Port;

    for (const auto& port : ports) {
        if (port.is_ch340) {
            ch340Port = port.device;
            break;
        }
    }

    if (ch340Port.empty()) {
        std::cout << "No CH340 devices available for configuration testing\n";
        std::cout
            << "The following would be tested with a connected CH340 device:\n";
        std::cout << "  - Baud rate compatibility\n";
        std::cout << "  - Data bits configuration\n";
        std::cout << "  - Flow control support\n";
        std::cout << "  - Timeout behavior\n";
        return;
    }

    std::cout << "Testing CH340 configuration on port: " << ch340Port << "\n";

    SerialPort port;

    // Test different baud rates
    std::cout << "\nTesting baud rate compatibility:\n";
    std::vector<int> testBaudRates = {9600,  19200,  38400,
                                      57600, 115200, 230400};

    for (int baudRate : testBaudRates) {
        SerialConfig testConfig;
        testConfig.setBaudRate(baudRate);
        testConfig.setReadTimeout(std::chrono::milliseconds(100));
        testConfig.setWriteTimeout(std::chrono::milliseconds(100));

        auto error = port.tryOpen(ch340Port, testConfig);
        if (error) {
            std::cout << "  " << std::setw(8) << baudRate << " bps: Failed - "
                      << *error << "\n";
        } else {
            std::cout << "  " << std::setw(8) << baudRate << " bps: OK\n";
            port.close();
        }
    }

    // Test flow control
    std::cout << "\nTesting flow control options:\n";

    struct FlowControlTest {
        FlowControl flowControl;
        std::string name;
    };

    std::vector<FlowControlTest> flowTests = {
        {FlowControl::None, "None"},
        {FlowControl::Hardware, "Hardware (RTS/CTS)"},
        {FlowControl::Software, "Software (XON/XOFF)"}};

    for (const auto& test : flowTests) {
        SerialConfig testConfig;
        testConfig.setBaudRate(9600);
        testConfig.setFlowControl(test.flowControl);
        testConfig.setReadTimeout(std::chrono::milliseconds(100));

        auto error = port.tryOpen(ch340Port, testConfig);
        if (error) {
            std::cout << "  " << std::setw(20) << test.name << ": Failed - "
                      << *error << "\n";
        } else {
            std::cout << "  " << std::setw(20) << test.name << ": OK\n";
            port.close();
        }
    }
}

/**
 * @brief Demonstrates CH340 troubleshooting tips
 */
void demonstrateCH340Troubleshooting() {
    std::cout << "\n=== CH340 Troubleshooting Guide ===\n";

    std::cout << "Common CH340 Issues and Solutions:\n\n";

    std::cout << "1. Device Not Recognized:\n";
    std::cout << "   - Install proper CH340 drivers from manufacturer\n";
    std::cout << "   - Check USB cable quality and connection\n";
    std::cout << "   - Try different USB ports\n";
    std::cout << "   - Verify device power requirements\n\n";

    std::cout << "2. Communication Errors:\n";
    std::cout
        << "   - Use standard baud rates (9600, 19200, 38400, 57600, 115200)\n";
    std::cout << "   - Avoid very high baud rates (>115200) unless necessary\n";
    std::cout << "   - Check wiring and connections\n";
    std::cout << "   - Ensure proper ground connection\n\n";

    std::cout << "3. Data Corruption:\n";
    std::cout << "   - Reduce baud rate\n";
    std::cout << "   - Check for electromagnetic interference\n";
    std::cout << "   - Use shorter cables\n";
    std::cout << "   - Add appropriate pull-up/pull-down resistors\n\n";

    std::cout << "4. Driver Issues:\n";
    std::cout << "   - Windows: Install official CH340 drivers\n";
    std::cout << "   - Linux: Usually works with built-in drivers (ch341)\n";
    std::cout << "   - macOS: May require driver installation\n";
    std::cout
        << "   - Check Device Manager/System Information for conflicts\n\n";

    std::cout << "5. Performance Optimization:\n";
    std::cout << "   - Use hardware flow control when available\n";
    std::cout << "   - Set appropriate timeouts\n";
    std::cout
        << "   - Consider buffer sizes for high-throughput applications\n";
    std::cout << "   - Test with different CH340 models if issues persist\n\n";

    std::cout << "Best Practices for CH340 Usage:\n";
    std::cout << "  - Always verify driver installation\n";
    std::cout << "  - Start with conservative settings (9600 baud, no flow "
                 "control)\n";
    std::cout << "  - Test thoroughly before deploying in production\n";
    std::cout << "  - Keep spare devices for troubleshooting\n";
    std::cout << "  - Document working configurations for future reference\n";
}

/**
 * @brief Main function demonstrating CH340 detection and handling
 */
int main() {
    std::cout << "=== CH340 Device Detection and Identification Example ===\n";
    std::cout << "This example focuses on CH340 USB-to-serial adapters.\n";

    // Demonstrate various CH340-related features
    demonstrateCH340Detection();
    demonstrateCH340ModelInfo();
    demonstrateCH340Configuration();
    demonstrateCH340Troubleshooting();

    std::cout << "\n=== CH340 Summary ===\n";
    std::cout
        << "CH340 devices are popular, low-cost USB-to-serial adapters.\n";
    std::cout
        << "While generally reliable, they may require specific drivers\n";
    std::cout << "and work best with standard baud rates and configurations.\n";
    std::cout
        << "Always test your specific CH340 model with your application\n";
    std::cout << "before deploying in production environments.\n";

    std::cout << "\n=== Example Complete ===\n";

    return 0;
}
