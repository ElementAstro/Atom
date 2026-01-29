/**
 * @file advanced_configuration.cpp
 * @brief Advanced serial port configuration example
 *
 * This example demonstrates comprehensive serial port configuration including:
 * - All supported baud rates and custom rates
 * - Data bits, stop bits, and parity options
 * - Flow control mechanisms (hardware and software)
 * - Timeout configurations and their effects
 * - Configuration validation and error handling
 * - Dynamic configuration changes
 *
 * @author Atom Serial Examples
 * @date 2024
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "atom/serial/core/serial_port.hpp"

using namespace atom::serial;

/**
 * @brief Demonstrates all supported baud rates
 */
void demonstrateBaudRates() {
    std::cout << "\n=== Baud Rate Configuration ===\n";

    // Common baud rates
    std::vector<int> commonBaudRates = {300,   600,    1200,   2400,   4800,
                                        9600,  14400,  19200,  28800,  38400,
                                        57600, 115200, 230400, 460800, 921600};

    std::cout << "Common baud rates:\n";
    for (int baudRate : commonBaudRates) {
        SerialConfig config;
        try {
            config.setBaudRate(baudRate);
            std::cout << "  " << std::setw(8) << baudRate
                      << " bps - Supported\n";
        } catch (const SerialConfigException& e) {
            std::cout << "  " << std::setw(8) << baudRate
                      << " bps - Not supported: " << e.what() << "\n";
        }
    }

    // Custom baud rate example
    std::cout << "\nCustom baud rates:\n";
    std::vector<int> customRates = {1000000, 1500000, 2000000, 3000000};
    for (int baudRate : customRates) {
        SerialConfig config;
        try {
            config.setBaudRate(baudRate);
            std::cout << "  " << std::setw(8) << baudRate
                      << " bps - Supported\n";
        } catch (const SerialConfigException& e) {
            std::cout << "  " << std::setw(8) << baudRate
                      << " bps - Not supported\n";
        }
    }
}

/**
 * @brief Demonstrates data bits configuration
 */
void demonstrateDataBits() {
    std::cout << "\n=== Data Bits Configuration ===\n";

    std::vector<int> dataBitsOptions = {5, 6, 7, 8, 9};

    for (int dataBits : dataBitsOptions) {
        SerialConfig config;
        try {
            config.setDataBits(dataBits);
            std::cout << "  " << dataBits << " data bits - Supported\n";
        } catch (const SerialConfigException& e) {
            std::cout << "  " << dataBits
                      << " data bits - Not supported: " << e.what() << "\n";
        }
    }

    std::cout << "\nNote: Most systems support 7 and 8 data bits. 5, 6, and 9 "
                 "may have limited support.\n";
}

/**
 * @brief Demonstrates parity configuration
 */
void demonstrateParityOptions() {
    std::cout << "\n=== Parity Configuration ===\n";

    struct ParityOption {
        Parity parity;
        std::string name;
        std::string description;
    };

    std::vector<ParityOption> parityOptions = {
        {Parity::None, "None", "No parity checking"},
        {Parity::Even, "Even", "Even parity - ensures even number of 1 bits"},
        {Parity::Odd, "Odd", "Odd parity - ensures odd number of 1 bits"}};

    for (const auto& option : parityOptions) {
        SerialConfig config;
        try {
            config.setParity(option.parity);
            std::cout << "  " << std::setw(6) << option.name << " - "
                      << option.description << "\n";
        } catch (const SerialConfigException& e) {
            std::cout << "  " << std::setw(6) << option.name
                      << " - Not supported: " << e.what() << "\n";
        }
    }
}

/**
 * @brief Demonstrates stop bits configuration
 */
void demonstrateStopBits() {
    std::cout << "\n=== Stop Bits Configuration ===\n";

    struct StopBitsOption {
        StopBits stopBits;
        std::string name;
        std::string description;
    };

    std::vector<StopBitsOption> stopBitsOptions = {
        {StopBits::One, "1", "One stop bit (most common)"},
        {StopBits::Two, "2", "Two stop bits (used for slower devices)"}};

    for (const auto& option : stopBitsOptions) {
        SerialConfig config;
        try {
            config.setStopBits(option.stopBits);
            std::cout << "  " << option.name << " stop bit(s) - "
                      << option.description << "\n";
        } catch (const SerialConfigException& e) {
            std::cout << "  " << option.name
                      << " stop bit(s) - Not supported: " << e.what() << "\n";
        }
    }
}

/**
 * @brief Demonstrates flow control options
 */
void demonstrateFlowControl() {
    std::cout << "\n=== Flow Control Configuration ===\n";

    struct FlowControlOption {
        FlowControl flowControl;
        std::string name;
        std::string description;
    };

    std::vector<FlowControlOption> flowControlOptions = {
        {FlowControl::None, "None", "No flow control"},
        {FlowControl::Hardware, "Hardware", "RTS/CTS hardware flow control"},
        {FlowControl::Software, "Software", "XON/XOFF software flow control"}};

    for (const auto& option : flowControlOptions) {
        SerialConfig config;
        try {
            config.setFlowControl(option.flowControl);
            std::cout << "  " << std::setw(10) << option.name << " - "
                      << option.description << "\n";
        } catch (const SerialConfigException& e) {
            std::cout << "  " << std::setw(10) << option.name
                      << " - Not supported: " << e.what() << "\n";
        }
    }

    std::cout << "\nFlow Control Usage Guidelines:\n";
    std::cout << "  - None: Use when devices can handle data at the configured "
                 "rate\n";
    std::cout << "  - Hardware: Preferred for high-speed communication\n";
    std::cout << "  - Software: Use when hardware lines are not available\n";
}

/**
 * @brief Demonstrates timeout configurations
 */
void demonstrateTimeoutConfiguration() {
    std::cout << "\n=== Timeout Configuration ===\n";

    SerialConfig config;

    // Different timeout scenarios
    std::vector<std::chrono::milliseconds> timeouts = {
        std::chrono::milliseconds(100),   // Fast response
        std::chrono::milliseconds(500),   // Standard
        std::chrono::milliseconds(1000),  // Conservative
        std::chrono::milliseconds(5000),  // Very patient
        std::chrono::milliseconds(0)      // Non-blocking
    };

    std::cout << "Read timeout options:\n";
    for (const auto& timeout : timeouts) {
        config.setReadTimeout(timeout);
        std::cout << "  " << std::setw(6) << timeout.count() << "ms - ";
        if (timeout.count() == 0) {
            std::cout << "Non-blocking reads\n";
        } else if (timeout.count() <= 100) {
            std::cout << "Fast response required\n";
        } else if (timeout.count() <= 1000) {
            std::cout << "Standard timeout\n";
        } else {
            std::cout << "Patient waiting\n";
        }
    }

    std::cout << "\nWrite timeout options:\n";
    for (const auto& timeout : timeouts) {
        config.setWriteTimeout(timeout);
        std::cout << "  " << std::setw(6) << timeout.count() << "ms - ";
        if (timeout.count() == 0) {
            std::cout << "Non-blocking writes\n";
        } else if (timeout.count() <= 100) {
            std::cout << "Fast transmission required\n";
        } else if (timeout.count() <= 1000) {
            std::cout << "Standard timeout\n";
        } else {
            std::cout << "Allow for slow transmission\n";
        }
    }
}

/**
 * @brief Creates and validates different configuration presets
 */
void demonstrateConfigurationPresets() {
    std::cout << "\n=== Configuration Presets ===\n";

    struct ConfigPreset {
        std::string name;
        std::function<SerialConfig()> createConfig;
        std::string description;
    };

    std::vector<ConfigPreset> presets = {
        {"Standard RS232",
         []() {
             SerialConfig config;
             config.setBaudRate(9600);
             config.setDataBits(8);
             config.setParity(Parity::None);
             config.setStopBits(StopBits::One);
             config.setFlowControl(FlowControl::None);
             config.setReadTimeout(std::chrono::milliseconds(1000));
             config.setWriteTimeout(std::chrono::milliseconds(1000));
             return config;
         },
         "Most common configuration for general purpose communication"},
        {"High Speed",
         []() {
             SerialConfig config;
             config.setBaudRate(115200);
             config.setDataBits(8);
             config.setParity(Parity::None);
             config.setStopBits(StopBits::One);
             config.setFlowControl(FlowControl::Hardware);
             config.setReadTimeout(std::chrono::milliseconds(100));
             config.setWriteTimeout(std::chrono::milliseconds(100));
             return config;
         },
         "High-speed communication with hardware flow control"},
        {"Reliable Legacy",
         []() {
             SerialConfig config;
             config.setBaudRate(2400);
             config.setDataBits(7);
             config.setParity(Parity::Even);
             config.setStopBits(StopBits::Two);
             config.setFlowControl(FlowControl::Software);
             config.setReadTimeout(std::chrono::milliseconds(2000));
             config.setWriteTimeout(std::chrono::milliseconds(2000));
             return config;
         },
         "Conservative settings for older or unreliable connections"}};

    for (const auto& preset : presets) {
        std::cout << "\n" << preset.name << ":\n";
        std::cout << "  Description: " << preset.description << "\n";

        try {
            auto config = preset.createConfig();
            std::cout << "  Configuration:\n";
            std::cout << "    Baud Rate: " << config.getBaudRate() << "\n";
            std::cout << "    Data Bits: " << config.getDataBits() << "\n";
            std::cout << "    Parity: "
                      << (config.getParity() == Parity::None   ? "None"
                          : config.getParity() == Parity::Even ? "Even"
                                                               : "Odd")
                      << "\n";
            std::cout << "    Stop Bits: "
                      << (config.getStopBits() == StopBits::One ? "1" : "2")
                      << "\n";
            std::cout << "    Flow Control: "
                      << (config.getFlowControl() == FlowControl::None ? "None"
                          : config.getFlowControl() == FlowControl::Hardware
                              ? "Hardware"
                              : "Software")
                      << "\n";
            std::cout << "    Read Timeout: " << config.getReadTimeout().count()
                      << "ms\n";
            std::cout << "    Write Timeout: "
                      << config.getWriteTimeout().count() << "ms\n";
            std::cout << "  Status: Valid configuration\n";

        } catch (const SerialConfigException& e) {
            std::cout << "  Status: Invalid configuration - " << e.what()
                      << "\n";
        }
    }
}

/**
 * @brief Main function demonstrating advanced configuration options
 */
int main() {
    std::cout << "=== Advanced Serial Port Configuration Example ===\n";
    std::cout << "This example demonstrates comprehensive serial port "
                 "configuration options.\n";

    // Demonstrate all configuration aspects
    demonstrateBaudRates();
    demonstrateDataBits();
    demonstrateParityOptions();
    demonstrateStopBits();
    demonstrateFlowControl();
    demonstrateTimeoutConfiguration();
    demonstrateConfigurationPresets();

    std::cout << "\n=== Configuration Guidelines ===\n";
    std::cout
        << "1. Start with standard settings (9600-8-N-1) for initial testing\n";
    std::cout << "2. Match the configuration to your device's specifications\n";
    std::cout << "3. Use hardware flow control for high-speed communication\n";
    std::cout << "4. Adjust timeouts based on expected response times\n";
    std::cout << "5. Test configuration changes incrementally\n";
    std::cout << "6. Consider the physical connection quality when choosing "
                 "settings\n";

    std::cout << "\n=== Example Complete ===\n";

    return 0;
}
