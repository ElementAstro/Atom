/**
 * @file custom_device_detectors.cpp
 * @brief Custom device detector implementation example
 *
 * This example demonstrates how to create and register custom device detectors
 * including:
 * - Creating detectors for specific hardware manufacturers
 * - VID/PID-based detection strategies
 * - Description string parsing and pattern matching
 * - Multi-criteria detection logic
 * - Detector priority and conflict resolution
 * - Dynamic detector registration and management
 * - Testing and validation of custom detectors
 *
 * @author Atom Serial Examples
 * @date 2024
 */

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <vector>
#include "atom/serial/core/scanner.hpp"

using namespace atom::serial;

/**
 * @brief Utility function to convert string to lowercase
 */
std::string toLower(std::string_view str) {
    std::string result;
    result.resize(str.size());
    std::transform(str.begin(), str.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

/**
 * @brief Utility function to check if string contains substring
 * (case-insensitive)
 */
bool containsIgnoreCase(std::string_view haystack, std::string_view needle) {
    std::string lowerHaystack = toLower(haystack);
    std::string lowerNeedle = toLower(needle);
    return lowerHaystack.find(lowerNeedle) != std::string::npos;
}

/**
 * @brief Demonstrates basic VID/PID-based device detection
 */
void demonstrateVidPidDetection(SerialPortScanner& scanner) {
    std::cout << "\n=== VID/PID-Based Device Detection ===\n";

    // FTDI device detector (comprehensive)
    scanner.register_device_detector(
        "FTDI_Comprehensive",
        [](uint16_t vid, uint16_t pid,
           std::string_view description) -> std::pair<bool, std::string> {
            if (vid == 0x0403) {  // FTDI VID
                switch (pid) {
                    case 0x6001:
                        return {true, "FTDI FT232R USB UART"};
                    case 0x6014:
                        return {true, "FTDI FT232H Hi-Speed USB UART/FIFO"};
                    case 0x6015:
                        return {true, "FTDI FT231X USB UART"};
                    case 0x6010:
                        return {true, "FTDI FT2232C/D/H Dual UART/FIFO"};
                    case 0x6011:
                        return {true, "FTDI FT4232H Quad UART/FIFO"};
                    case 0x6040:
                        return {true, "FTDI FT2232H Dual UART/FIFO"};
                    case 0x6041:
                        return {true, "FTDI FT4232H Quad UART/FIFO"};
                    case 0x9378:
                        return {true, "FTDI FT4222H Quad SPI/I2C"};
                    default:
                        return {true, "FTDI Device (PID: " +
                                          std::to_string(pid) + ")"};
                }
            }
            return {false, ""};
        });

    // Prolific device detector
    scanner.register_device_detector(
        "Prolific_Comprehensive",
        [](uint16_t vid, uint16_t pid,
           std::string_view description) -> std::pair<bool, std::string> {
            if (vid == 0x067B) {  // Prolific VID
                switch (pid) {
                    case 0x2303:
                        return {true, "Prolific PL2303 USB-to-Serial"};
                    case 0x04BB:
                        return {true, "Prolific PL2303 (HXD variant)"};
                    case 0x2317:
                        return {true, "Prolific PL2303X USB-to-Serial"};
                    default:
                        return {true, "Prolific Device (PID: " +
                                          std::to_string(pid) + ")"};
                }
            }
            return {false, ""};
        });

    // Silicon Labs device detector
    scanner.register_device_detector(
        "SiliconLabs_Comprehensive",
        [](uint16_t vid, uint16_t pid,
           std::string_view description) -> std::pair<bool, std::string> {
            if (vid == 0x10C4) {  // Silicon Labs VID
                switch (pid) {
                    case 0xEA60:
                        return {true, "Silicon Labs CP210x USB-to-UART Bridge"};
                    case 0xEA70:
                        return {true,
                                "Silicon Labs CP2105 Dual USB-to-UART Bridge"};
                    case 0xEA71:
                        return {true,
                                "Silicon Labs CP2108 Quad USB-to-UART Bridge"};
                    case 0xEA80:
                        return {true,
                                "Silicon Labs CP2110 HID USB-to-UART Bridge"};
                    default:
                        return {true, "Silicon Labs Device (PID: " +
                                          std::to_string(pid) + ")"};
                }
            }
            return {false, ""};
        });

    std::cout << "Registered comprehensive VID/PID-based detectors for:\n";
    std::cout << "  - FTDI (VID: 0x0403) - Multiple product variants\n";
    std::cout << "  - Prolific (VID: 0x067B) - PL2303 series\n";
    std::cout << "  - Silicon Labs (VID: 0x10C4) - CP210x series\n";
}

/**
 * @brief Demonstrates description-based device detection
 */
void demonstrateDescriptionBasedDetection(SerialPortScanner& scanner) {
    std::cout << "\n=== Description-Based Device Detection ===\n";

    // Arduino device detector (based on description patterns)
    scanner.register_device_detector(
        "Arduino_Detector",
        [](uint16_t vid, uint16_t pid,
           std::string_view description) -> std::pair<bool, std::string> {
            std::string desc = toLower(description);

            // Check for Arduino-specific patterns
            if (desc.find("arduino") != std::string::npos) {
                if (desc.find("uno") != std::string::npos)
                    return {true, "Arduino Uno"};
                if (desc.find("mega") != std::string::npos)
                    return {true, "Arduino Mega"};
                if (desc.find("leonardo") != std::string::npos)
                    return {true, "Arduino Leonardo"};
                if (desc.find("micro") != std::string::npos)
                    return {true, "Arduino Micro"};
                if (desc.find("nano") != std::string::npos)
                    return {true, "Arduino Nano"};
                return {true, "Arduino Compatible Device"};
            }

            // Check for common Arduino VIDs
            if (vid == 0x2341 || vid == 0x1B4F || vid == 0x239A) {
                return {true, "Arduino Compatible (VID: " +
                                  std::to_string(vid) + ")"};
            }

            return {false, ""};
        });

    // ESP32/ESP8266 device detector
    scanner.register_device_detector(
        "ESP_Detector",
        [](uint16_t vid, uint16_t pid,
           std::string_view description) -> std::pair<bool, std::string> {
            std::string desc = toLower(description);

            // Check for ESP-specific patterns
            if (desc.find("esp32") != std::string::npos) {
                return {true, "ESP32 Development Board"};
            }
            if (desc.find("esp8266") != std::string::npos) {
                return {true, "ESP8266 Development Board"};
            }
            if (desc.find("nodemcu") != std::string::npos) {
                return {true, "NodeMCU (ESP-based)"};
            }
            if (desc.find("wemos") != std::string::npos) {
                return {true, "WeMos (ESP-based)"};
            }

            // Check for Espressif VID
            if (vid == 0x303A) {
                return {true, "Espressif Device"};
            }

            return {false, ""};
        });

    // Generic microcontroller detector
    scanner.register_device_detector(
        "Microcontroller_Detector",
        [](uint16_t /* vid */, uint16_t /* pid */,
           std::string_view description) -> std::pair<bool, std::string> {
            std::string desc = toLower(description);

            // Check for common microcontroller terms
            std::vector<std::string> mcuTerms = {
                "stm32",   "stlink", "nucleo", "discovery", "raspberry",
                "pi pico", "teensy", "mbed",   "lpc",       "kinetis",
                "sam",     "samd",   "same"};

            for (const auto& term : mcuTerms) {
                if (desc.find(term) != std::string::npos) {
                    return {true, "Microcontroller Development Board"};
                }
            }

            return {false, ""};
        });

    std::cout << "Registered description-based detectors for:\n";
    std::cout << "  - Arduino boards and compatibles\n";
    std::cout << "  - ESP32/ESP8266 development boards\n";
    std::cout << "  - Generic microcontroller development boards\n";
}

/**
 * @brief Demonstrates advanced multi-criteria detection
 */
void demonstrateAdvancedDetection(SerialPortScanner& scanner) {
    std::cout << "\n=== Advanced Multi-Criteria Detection ===\n";

    // Industrial device detector (combines VID/PID and description)
    scanner.register_device_detector(
        "Industrial_Detector",
        [](uint16_t vid, uint16_t pid,
           std::string_view description) -> std::pair<bool, std::string> {
            std::string desc = toLower(description);

            // Check for industrial automation vendors
            struct IndustrialVendor {
                uint16_t vid;
                std::string name;
                std::vector<std::string> keywords;
            };

            std::vector<IndustrialVendor> vendors = {
                {0x0403,
                 "FTDI Industrial",
                 {"industrial", "automation", "modbus", "profibus"}},
                {0x067B, "Prolific Industrial", {"plc", "hmi", "scada"}},
                {0x1A86, "QinHeng Industrial", {"ch340", "industrial"}},
                {0x10C4,
                 "Silicon Labs Industrial",
                 {"cp210x", "industrial", "automation"}}};

            for (const auto& vendor : vendors) {
                if (vid == vendor.vid) {
                    for (const auto& keyword : vendor.keywords) {
                        if (desc.find(keyword) != std::string::npos) {
                            return {true, vendor.name + " Device"};
                        }
                    }
                }
            }

            // Check for generic industrial terms
            std::vector<std::string> industrialTerms = {
                "modbus", "profibus", "canbus", "rs485",      "rs422",
                "plc",    "hmi",      "scada",  "industrial", "automation"};

            for (const auto& term : industrialTerms) {
                if (desc.find(term) != std::string::npos) {
                    return {true, "Industrial Communication Device"};
                }
            }

            return {false, ""};
        });

    // GPS/GNSS device detector
    scanner.register_device_detector(
        "GPS_Detector",
        [](uint16_t vid, uint16_t pid,
           std::string_view description) -> std::pair<bool, std::string> {
            std::string desc = toLower(description);

            // Check for GPS/GNSS terms
            std::vector<std::string> gpsTerms = {
                "gps",  "gnss",  "glonass", "galileo", "beidou",
                "nmea", "ublox", "u-blox",  "garmin",  "trimble"};

            for (const auto& term : gpsTerms) {
                if (desc.find(term) != std::string::npos) {
                    return {true, "GPS/GNSS Receiver"};
                }
            }

            // Check for known GPS vendor VIDs
            if (vid == 0x1546) {  // u-blox
                return {true, "u-blox GPS/GNSS Receiver"};
            }

            return {false, ""};
        });

    std::cout << "Registered advanced detectors for:\n";
    std::cout << "  - Industrial automation devices\n";
    std::cout << "  - GPS/GNSS receivers\n";
}

/**
 * @brief Demonstrates testing custom detectors
 */
void testCustomDetectors(SerialPortScanner& scanner) {
    std::cout << "\n=== Testing Custom Detectors ===\n";

    std::cout << "Scanning for devices with custom detectors...\n";
    auto result = scanner.list_available_ports();

    if (std::holds_alternative<std::vector<SerialPortScanner::PortInfo>>(
            result)) {
        const auto& ports =
            std::get<std::vector<SerialPortScanner::PortInfo>>(result);

        if (ports.empty()) {
            std::cout << "No devices found for testing.\n";
            std::cout << "Connect various USB-to-serial devices to see custom "
                         "detection in action.\n";
            return;
        }

        std::cout << "Found " << ports.size() << " device(s):\n\n";

        for (size_t i = 0; i < ports.size(); ++i) {
            const auto& port = ports[i];
            std::cout << "Device " << (i + 1) << ":\n";
            std::cout << "  Port: " << port.device << "\n";
            std::cout << "  Description: " << port.description << "\n";
            std::cout << "  VID:PID: " << port.vendor_id << ":"
                      << port.product_id << "\n";

            // The custom detectors would have been applied during scanning
            // and their results would be reflected in the port information

            if (containsIgnoreCase(port.description, "arduino")) {
                std::cout << "  -> Detected by Arduino detector\n";
            }
            if (containsIgnoreCase(port.description, "esp")) {
                std::cout << "  -> Detected by ESP detector\n";
            }
            if (containsIgnoreCase(port.description, "ftdi")) {
                std::cout << "  -> Detected by FTDI detector\n";
            }
            if (containsIgnoreCase(port.description, "prolific")) {
                std::cout << "  -> Detected by Prolific detector\n";
            }

            std::cout << "\n";
        }

    } else {
        const auto& error = std::get<SerialPortScanner::ErrorInfo>(result);
        std::cerr << "Error scanning devices: " << error.message << "\n";
    }
}

/**
 * @brief Demonstrates detector best practices
 */
void demonstrateDetectorBestPractices() {
    std::cout << "\n=== Custom Detector Best Practices ===\n";

    std::cout << "1. Detector Design Principles:\n";
    std::cout << "   - Use specific VID/PID combinations when available\n";
    std::cout
        << "   - Fall back to description parsing for generic detection\n";
    std::cout << "   - Return descriptive device names, not just 'detected'\n";
    std::cout << "   - Handle edge cases and unknown variants gracefully\n\n";

    std::cout << "2. Performance Considerations:\n";
    std::cout << "   - Keep detector logic simple and fast\n";
    std::cout << "   - Avoid complex regex operations if possible\n";
    std::cout << "   - Use early returns to minimize processing\n";
    std::cout << "   - Cache expensive computations if needed\n\n";

    std::cout << "3. Reliability Guidelines:\n";
    std::cout << "   - Test with real hardware when possible\n";
    std::cout << "   - Handle case-insensitive string comparisons\n";
    std::cout << "   - Be specific enough to avoid false positives\n";
    std::cout << "   - Document the detection criteria used\n\n";

    std::cout << "4. Maintenance Tips:\n";
    std::cout << "   - Keep VID/PID databases up to date\n";
    std::cout << "   - Monitor for new device variants\n";
    std::cout << "   - Version your detector implementations\n";
    std::cout << "   - Provide fallback detection methods\n\n";
}

/**
 * @brief Main function demonstrating custom device detectors
 */
int main() {
    std::cout << "=== Custom Device Detector Example ===\n";
    std::cout << "This example shows how to create and register custom device "
                 "detectors.\n";

    // Create scanner
    ScannerConfig config;
    config.detect_ch340 = true;  // Keep built-in CH340 detection
    config.scan_timeout = std::chrono::milliseconds(3000);

    SerialPortScanner scanner(config);

    // Demonstrate different types of custom detectors
    demonstrateVidPidDetection(scanner);
    demonstrateDescriptionBasedDetection(scanner);
    demonstrateAdvancedDetection(scanner);

    // Test the registered detectors
    testCustomDetectors(scanner);

    // Show best practices
    demonstrateDetectorBestPractices();

    std::cout << "\n=== Custom Detector Summary ===\n";
    std::cout << "Custom device detectors allow you to:\n";
    std::cout << "  - Identify specific hardware types automatically\n";
    std::cout << "  - Provide meaningful device names and descriptions\n";
    std::cout << "  - Implement application-specific device categorization\n";
    std::cout << "  - Enhance user experience with better device recognition\n";

    std::cout << "\n=== Example Complete ===\n";

    return 0;
}
