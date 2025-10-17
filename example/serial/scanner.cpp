#include "atom/serial/core/scanner.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

using namespace atom::serial;

/**
 * @file scanner.cpp
 * @brief Comprehensive SerialPortScanner demonstration
 *
 * This enhanced example demonstrates:
 * 1. Basic and advanced scanner configuration
 * 2. Asynchronous and synchronous port listing
 * 3. Detailed port information retrieval
 * 4. Custom device detector registration
 * 5. Performance monitoring and statistics
 * 6. Caching and background monitoring
 * 7. Error handling and recovery
 * 8. CH340 device detection and identification
 *
 * @author Atom Serial Examples
 * @date 2024
 */

/**
 * @brief Demonstrates basic scanner configuration options
 */
void demonstrateBasicConfiguration() {
    std::cout << "\n=== Basic Scanner Configuration ===\n";

    // Create scanner with default configuration
    ScannerConfig defaultConfig;
    std::cout << "Default configuration:\n";
    std::cout << "  CH340 detection: "
              << (defaultConfig.detect_ch340 ? "enabled" : "disabled") << "\n";
    std::cout << "  Virtual ports: "
              << (defaultConfig.include_virtual_ports ? "included" : "excluded")
              << "\n";
    std::cout << "  Scan timeout: " << defaultConfig.scan_timeout.count()
              << "ms\n";
    std::cout << "  Cache TTL: " << defaultConfig.cache_ttl.count() << "ms\n";
    std::cout << "  Max retries: " << defaultConfig.max_retry_count << "\n";

    // Create custom configuration
    ScannerConfig customConfig;
    customConfig.detect_ch340 = true;
    customConfig.include_virtual_ports = false;
    customConfig.scan_timeout = std::chrono::milliseconds(3000);
    customConfig.cache_ttl = std::chrono::milliseconds(30000);
    customConfig.max_retry_count = 5;
    customConfig.enable_debug_logging = true;
    customConfig.enable_performance_logging = true;

    std::cout << "\nCustom configuration:\n";
    std::cout << "  CH340 detection: "
              << (customConfig.detect_ch340 ? "enabled" : "disabled") << "\n";
    std::cout << "  Virtual ports: "
              << (customConfig.include_virtual_ports ? "included" : "excluded")
              << "\n";
    std::cout << "  Scan timeout: " << customConfig.scan_timeout.count()
              << "ms\n";
    std::cout << "  Cache TTL: " << customConfig.cache_ttl.count() << "ms\n";
    std::cout << "  Max retries: " << customConfig.max_retry_count << "\n";
    std::cout << "  Debug logging: "
              << (customConfig.enable_debug_logging ? "enabled" : "disabled")
              << "\n";
    std::cout << "  Performance logging: "
              << (customConfig.enable_performance_logging ? "enabled"
                                                          : "disabled")
              << "\n";

    // Validate configuration
    if (customConfig.is_valid()) {
        std::cout << "  Configuration is valid\n";
    } else {
        std::cout << "  Configuration is invalid\n";
    }
}

/**
 * @brief Demonstrates advanced scanner features
 */
void demonstrateAdvancedFeatures() {
    std::cout << "\n=== Advanced Scanner Features ===\n";

    // Create scanner with advanced configuration
    ScannerConfig config;
    config.detect_ch340 = true;
    config.include_virtual_ports = true;
    config.scan_timeout = std::chrono::milliseconds(2000);
    config.enable_caching = true;
    config.enable_background_monitoring = true;
    config.monitor_interval = std::chrono::milliseconds(5000);
    config.enable_performance_logging = true;
    config.log_stats_interval = std::chrono::milliseconds(10000);

    SerialPortScanner scanner(config);

    // Register multiple custom device detectors
    std::cout << "Registering custom device detectors...\n";

    // FTDI device detector
    scanner.register_device_detector(
        "FTDI",
        [](uint16_t vid, uint16_t pid,
           std::string_view description) -> std::pair<bool, std::string> {
            // Check VID for FTDI devices
            if (vid == 0x0403) {
                std::string model = "FTDI Device";
                if (pid == 0x6001)
                    model = "FTDI FT232R";
                else if (pid == 0x6014)
                    model = "FTDI FT232H";
                else if (pid == 0x6015)
                    model = "FTDI FT231X";
                return {true, model};
            }

            // Check description for FTDI
            std::string lower_desc;
            lower_desc.resize(description.size());
            std::transform(description.begin(), description.end(),
                           lower_desc.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (lower_desc.find("ftdi") != std::string::npos) {
                return {true, "FTDI (Detected by Description)"};
            }

            return {false, ""};
        });

    // Prolific device detector
    scanner.register_device_detector(
        "Prolific",
        [](uint16_t vid, uint16_t pid,
           std::string_view description) -> std::pair<bool, std::string> {
            if (vid == 0x067B) {
                if (pid == 0x2303)
                    return {true, "Prolific PL2303"};
                return {true, "Prolific Device"};
            }

            std::string lower_desc;
            lower_desc.resize(description.size());
            std::transform(description.begin(), description.end(),
                           lower_desc.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (lower_desc.find("prolific") != std::string::npos) {
                return {true, "Prolific (Detected by Description)"};
            }

            return {false, ""};
        });

    // Silicon Labs device detector
    scanner.register_device_detector(
        "SiLabs",
        [](uint16_t vid, uint16_t pid,
           std::string_view /* description */) -> std::pair<bool, std::string> {
            if (vid == 0x10C4) {
                if (pid == 0xEA60)
                    return {true, "Silicon Labs CP210x"};
                return {true, "Silicon Labs Device"};
            }
            return {false, ""};
        });

    std::cout << "Custom device detectors registered successfully\n";
}

/**
 * @brief Demonstrates asynchronous port scanning
 */
void demonstrateAsyncScanning(SerialPortScanner& scanner) {
    std::cout << "\n=== Asynchronous Port Scanning ===\n";

    std::atomic<bool> scanComplete = false;
    std::vector<SerialPortScanner::PortInfo> foundPorts;

    std::cout << "Starting asynchronous port scan...\n";

    scanner.list_available_ports_async(
        [&scanComplete, &foundPorts](
            SerialPortScanner::Result<std::vector<SerialPortScanner::PortInfo>>
                result) {
            if (std::holds_alternative<
                    std::vector<SerialPortScanner::PortInfo>>(result)) {
                foundPorts =
                    std::get<std::vector<SerialPortScanner::PortInfo>>(result);
                std::cout << "  Async scan found " << foundPorts.size()
                          << " ports\n";

                for (const auto& port : foundPorts) {
                    std::cout << "    " << std::setw(12) << port.device << " - "
                              << port.description;
                    if (port.is_ch340) {
                        std::cout << " [CH340: " << port.ch340_model << "]";
                    }
                    std::cout << "\n";
                }
            } else {
                const auto& error =
                    std::get<SerialPortScanner::ErrorInfo>(result);
                std::cerr << "  Async scan error: " << error.message
                          << " (code: " << error.error_code << ")\n";
            }
            scanComplete = true;
        });

    // Wait for completion with progress indication
    int dots = 0;
    while (!scanComplete) {
        std::cout << "  Scanning" << std::string(dots % 4, '.') << "\r"
                  << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        dots++;
    }
    std::cout << "  Scanning complete!        \n";
}

/**
 * @brief Demonstrates synchronous port scanning with detailed information
 */
void demonstrateSyncScanning(SerialPortScanner& scanner) {
    std::cout << "\n=== Synchronous Port Scanning ===\n";

    std::cout << "Performing synchronous port scan...\n";
    auto result = scanner.list_available_ports();

    if (std::holds_alternative<std::vector<SerialPortScanner::PortInfo>>(
            result)) {
        const auto& ports =
            std::get<std::vector<SerialPortScanner::PortInfo>>(result);

        std::cout << "Found " << ports.size() << " serial ports:\n";

        for (size_t i = 0; i < ports.size(); ++i) {
            const auto& port = ports[i];
            std::cout << "\n  Port " << (i + 1) << ": " << port.device << "\n";
            std::cout << "    Description: " << port.description << "\n";
            std::cout << "    Hardware ID: " << port.hardware_id << "\n";
            std::cout << "    VID:PID: " << port.vendor_id << ":"
                      << port.product_id << "\n";
            std::cout << "    Available: " << (port.is_available ? "Yes" : "No")
                      << "\n";
            std::cout << "    Virtual: " << (port.is_virtual ? "Yes" : "No")
                      << "\n";
            std::cout << "    Bluetooth: " << (port.is_bluetooth ? "Yes" : "No")
                      << "\n";

            if (!port.manufacturer.empty()) {
                std::cout << "    Manufacturer: " << port.manufacturer << "\n";
            }
            if (!port.serial_number.empty()) {
                std::cout << "    Serial Number: " << port.serial_number
                          << "\n";
            }
            if (!port.location.empty()) {
                std::cout << "    Location: " << port.location << "\n";
            }

            if (port.is_ch340) {
                std::cout << "    CH340 Device: " << port.ch340_model << "\n";
            }

            // Get detailed information for this port
            auto detailsResult = scanner.get_port_details(port.device);
            if (std::holds_alternative<
                    std::optional<SerialPortScanner::PortDetails>>(
                    detailsResult)) {
                const auto& maybeDetails =
                    std::get<std::optional<SerialPortScanner::PortDetails>>(
                        detailsResult);
                if (maybeDetails && maybeDetails->is_ch340) {
                    std::cout << "    Recommended Baud Rates: "
                              << maybeDetails->recommended_baud_rates << "\n";
                    if (!maybeDetails->notes.empty()) {
                        std::cout << "    Notes: " << maybeDetails->notes
                                  << "\n";
                    }
                }
            }
        }
    } else {
        const auto& error = std::get<SerialPortScanner::ErrorInfo>(result);
        std::cerr << "Sync scan error: " << error.message
                  << " (code: " << error.error_code << ")\n";
    }
}

/**
 * @brief Demonstrates performance monitoring and statistics
 */
void demonstratePerformanceMonitoring(SerialPortScanner& scanner) {
    std::cout << "\n=== Performance Monitoring ===\n";

    // Reset statistics for clean measurement
    scanner.reset_statistics();

    std::cout << "Performing multiple scans to collect statistics...\n";

    // Perform several scans
    for (int i = 0; i < 3; ++i) {
        std::cout << "  Scan " << (i + 1) << "...\n";
        auto result = scanner.list_available_ports();

        if (std::holds_alternative<std::vector<SerialPortScanner::PortInfo>>(
                result)) {
            const auto& ports =
                std::get<std::vector<SerialPortScanner::PortInfo>>(result);
            std::cout << "    Found " << ports.size() << " ports\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Get and display statistics
    auto stats = scanner.get_statistics();
    std::cout << "\nPerformance Statistics:\n";
    std::cout << "  Total scans: " << stats.total_scans << "\n";
    std::cout << "  Successful scans: " << stats.successful_scans << "\n";
    std::cout << "  Failed scans: " << stats.failed_scans << "\n";
    std::cout << "  Ports found: " << stats.ports_found << "\n";
    std::cout << "  CH340 devices found: " << stats.ch340_devices_found << "\n";
    std::cout << "  Average scan time: " << std::fixed << std::setprecision(2)
              << stats.get_average_scan_time() << "μs\n";
    std::cout << "  Min scan time: " << stats.min_scan_time.load() << "μs\n";
    std::cout << "  Max scan time: " << stats.max_scan_time.load() << "μs\n";
    std::cout << "  Cache hits: " << stats.cache_hits << "\n";
    std::cout << "  Cache misses: " << stats.cache_misses << "\n";
}

/**
 * @brief Demonstrates caching functionality
 */
void demonstrateCaching(SerialPortScanner& scanner) {
    std::cout << "\n=== Caching Functionality ===\n";

    std::cout << "Cache information:\n";
    std::cout << scanner.get_cache_info() << "\n";

    std::cout << "Performing scan to populate cache...\n";
    auto result1 = scanner.list_available_ports();

    std::cout << "Performing second scan (should use cache)...\n";
    auto result2 = scanner.list_available_ports();

    std::cout << "Cache information after scans:\n";
    std::cout << scanner.get_cache_info() << "\n";

    std::cout << "Refreshing cache...\n";
    scanner.refresh_cache();

    std::cout << "Cache information after refresh:\n";
    std::cout << scanner.get_cache_info() << "\n";
}

/**
 * @brief Demonstrates port availability checking
 */
void demonstratePortAvailability(SerialPortScanner& scanner) {
    std::cout << "\n=== Port Availability Checking ===\n";

    // Get list of ports first
    auto result = scanner.list_available_ports();
    if (std::holds_alternative<std::vector<SerialPortScanner::PortInfo>>(
            result)) {
        const auto& ports =
            std::get<std::vector<SerialPortScanner::PortInfo>>(result);

        if (!ports.empty()) {
            // Check availability of first port
            std::string testPort = ports[0].device;
            std::cout << "Checking availability of port: " << testPort << "\n";

            auto availResult = scanner.is_port_available(testPort);
            if (std::holds_alternative<bool>(availResult)) {
                bool available = std::get<bool>(availResult);
                std::cout << "  Port is "
                          << (available ? "available" : "not available")
                          << "\n";
            } else {
                const auto& error =
                    std::get<SerialPortScanner::ErrorInfo>(availResult);
                std::cout << "  Error checking availability: " << error.message
                          << "\n";
            }
        }
    }

    // Test with non-existent port
    std::cout << "Checking availability of non-existent port...\n";
    auto availResult = scanner.is_port_available("NONEXISTENT_PORT");
    if (std::holds_alternative<bool>(availResult)) {
        bool available = std::get<bool>(availResult);
        std::cout << "  Non-existent port is "
                  << (available ? "available" : "not available") << "\n";
    } else {
        const auto& error = std::get<SerialPortScanner::ErrorInfo>(availResult);
        std::cout << "  Expected error: " << error.message << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive scanner functionality
 */
int main() {
    try {
        std::cout << "=== Comprehensive SerialPortScanner Example ===\n";
        std::cout << "This example demonstrates all scanner capabilities.\n";

        // Demonstrate basic configuration
        demonstrateBasicConfiguration();

        // Create scanner with advanced configuration
        demonstrateAdvancedFeatures();

        // Create scanner for demonstrations
        ScannerConfig config;
        config.detect_ch340 = true;
        config.include_virtual_ports = true;
        config.scan_timeout = std::chrono::milliseconds(2000);
        config.cache_ttl = std::chrono::milliseconds(30000);
        config.enable_performance_logging = true;
        config.enable_debug_logging = false;  // Reduce noise
        config.enable_background_monitoring = false;

        SerialPortScanner scanner(config);

        // Demonstrate various scanner features
        demonstrateAsyncScanning(scanner);
        demonstrateSyncScanning(scanner);
        demonstratePerformanceMonitoring(scanner);
        demonstrateCaching(scanner);
        demonstratePortAvailability(scanner);

        std::cout << "\n=== Scanner Best Practices ===\n";
        std::cout
            << "1. Enable caching for better performance in repeated scans\n";
        std::cout << "2. Use async scanning for non-blocking operations\n";
        std::cout
            << "3. Register custom device detectors for specific hardware\n";
        std::cout
            << "4. Monitor performance statistics to optimize scan intervals\n";
        std::cout
            << "5. Handle errors gracefully and provide fallback options\n";
        std::cout << "6. Use appropriate timeouts based on your requirements\n";
        std::cout
            << "7. Consider background monitoring for real-time port changes\n";

        std::cout << "\n=== Example Complete ===\n";

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
