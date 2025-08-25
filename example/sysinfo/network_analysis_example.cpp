/**
 * @file network_analysis_example.cpp
 * @brief Comprehensive example demonstrating network and WiFi analysis
 *
 * This example provides a complete demonstration of network and WiFi
 * information gathering and analysis capabilities available in the Atom Sysinfo
 * module. It showcases advanced network monitoring, connectivity analysis, and
 * performance metrics.
 *
 * Features demonstrated:
 * - Network statistics and performance monitoring
 * - WiFi network scanning and analysis
 * - Network connectivity testing and validation
 * - Bandwidth measurement and analysis
 * - Network security information gathering
 * - Connected device enumeration
 * - Network quality assessment
 * - Signal strength monitoring
 * - Latency and packet loss analysis
 * - Cross-platform network information gathering
 *
 * Platform support:
 * - Windows (with WMI and Win32 networking APIs)
 * - Linux (with /proc/net, iwconfig, and NetworkManager)
 * - macOS (with System Configuration and CoreWLAN)
 * - FreeBSD (with ifconfig and wireless tools)
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 1.0 - Comprehensive network analysis demonstration
 */

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Atom Sysinfo module headers
#include "atom/sysinfo/wifi.hpp"

using namespace atom::system;

namespace {
constexpr int DISPLAY_WIDTH = 80;
constexpr double GOOD_SIGNAL_THRESHOLD = -50.0;   // dBm
constexpr double FAIR_SIGNAL_THRESHOLD = -70.0;   // dBm
constexpr double POOR_SIGNAL_THRESHOLD = -80.0;   // dBm
constexpr double HIGH_LATENCY_THRESHOLD = 100.0;  // ms
constexpr double POOR_LATENCY_THRESHOLD = 200.0;  // ms
}  // namespace

/**
 * @brief Utility function to create formatted section headers
 */
std::string createSectionHeader(const std::string& title) {
    std::string header = "\n" + std::string(DISPLAY_WIDTH, '=') + "\n";
    header += "=== " + title + " ===\n";
    header += std::string(DISPLAY_WIDTH, '=') + "\n";
    return header;
}

/**
 * @brief Get signal strength quality description
 */
std::string getSignalQuality(double signalStrength) {
    if (signalStrength >= GOOD_SIGNAL_THRESHOLD) {
        return "🟢 EXCELLENT";
    } else if (signalStrength >= FAIR_SIGNAL_THRESHOLD) {
        return "🟡 GOOD";
    } else if (signalStrength >= POOR_SIGNAL_THRESHOLD) {
        return "🟠 FAIR";
    } else {
        return "🔴 POOR";
    }
}

/**
 * @brief Get latency quality description
 */
std::string getLatencyQuality(double latency) {
    if (latency < 20.0) {
        return "🟢 EXCELLENT";
    } else if (latency < 50.0) {
        return "🟡 GOOD";
    } else if (latency < HIGH_LATENCY_THRESHOLD) {
        return "🟠 FAIR";
    } else if (latency < POOR_LATENCY_THRESHOLD) {
        return "🔴 POOR";
    } else {
        return "🔴 VERY POOR";
    }
}

/**
 * @brief Create a signal strength bar
 */
std::string createSignalBar(double signalStrength, int width = 20) {
    // Convert dBm to percentage (rough approximation)
    double percentage =
        std::max(0.0, std::min(100.0, (signalStrength + 100.0) * 2.0));
    int filledWidth = static_cast<int>((percentage / 100.0) * width);

    std::string bar = "[";
    for (int i = 0; i < width; ++i) {
        if (i < filledWidth) {
            if (signalStrength >= GOOD_SIGNAL_THRESHOLD)
                bar += "█";
            else if (signalStrength >= FAIR_SIGNAL_THRESHOLD)
                bar += "▓";
            else
                bar += "▒";
        } else {
            bar += "░";
        }
    }
    bar += "]";
    return bar;
}

/**
 * @brief Demonstrates network statistics and performance monitoring
 */
void demonstrateNetworkStatistics() {
    std::cout << createSectionHeader(
        "Network Statistics and Performance Monitoring");

    try {
        std::cout << "Gathering network statistics and performance data...\n\n";

        auto netStats = getNetworkStats();

        std::cout << "Current Network Performance:\n";
        std::cout << "  Download Speed:       " << std::fixed
                  << std::setprecision(2) << netStats.downloadSpeed
                  << " MB/s\n";
        std::cout << "  Upload Speed:         " << std::fixed
                  << std::setprecision(2) << netStats.uploadSpeed << " MB/s\n";
        std::cout << "  Network Latency:      " << std::fixed
                  << std::setprecision(1) << netStats.latency << " ms\n";
        std::cout << "  Packet Loss:          " << std::fixed
                  << std::setprecision(2) << netStats.packetLoss << "%\n";
        std::cout << "  Signal Strength:      " << std::fixed
                  << std::setprecision(1) << netStats.signalStrength
                  << " dBm\n";

        // Performance analysis
        std::cout << "\nPerformance Analysis:\n";

        // Speed analysis
        if (netStats.downloadSpeed > 100.0) {
            std::cout << "  Download Quality:     🟢 EXCELLENT (>100 MB/s)\n";
        } else if (netStats.downloadSpeed > 25.0) {
            std::cout << "  Download Quality:     🟡 GOOD (25-100 MB/s)\n";
        } else if (netStats.downloadSpeed > 5.0) {
            std::cout << "  Download Quality:     🟠 FAIR (5-25 MB/s)\n";
        } else {
            std::cout << "  Download Quality:     🔴 POOR (<5 MB/s)\n";
        }

        // Latency analysis
        std::cout << "  Latency Quality:      "
                  << getLatencyQuality(netStats.latency) << " (" << std::fixed
                  << std::setprecision(1) << netStats.latency << " ms)\n";

        // Packet loss analysis
        if (netStats.packetLoss < 1.0) {
            std::cout << "  Packet Loss:          🟢 EXCELLENT (<1%)\n";
        } else if (netStats.packetLoss < 3.0) {
            std::cout << "  Packet Loss:          🟡 ACCEPTABLE (1-3%)\n";
        } else if (netStats.packetLoss < 5.0) {
            std::cout << "  Packet Loss:          🟠 POOR (3-5%)\n";
        } else {
            std::cout << "  Packet Loss:          🔴 VERY POOR (>5%)\n";
        }

        // Signal strength analysis
        if (netStats.signalStrength != 0.0) {
            std::cout << "  Signal Quality:       "
                      << getSignalQuality(netStats.signalStrength) << " ("
                      << std::fixed << std::setprecision(1)
                      << netStats.signalStrength << " dBm)\n";
            std::cout << "  Signal Bar:           "
                      << createSignalBar(netStats.signalStrength) << "\n";
        }

        // Connected devices
        if (!netStats.connectedDevices.empty()) {
            std::cout << "\nConnected Devices:\n";
            std::cout << "  Device Count:         "
                      << netStats.connectedDevices.size() << "\n";
            std::cout << "  Devices:\n";
            for (size_t i = 0;
                 i < std::min(netStats.connectedDevices.size(), size_t(10));
                 ++i) {
                std::cout << "    " << (i + 1) << ". "
                          << netStats.connectedDevices[i] << "\n";
            }
            if (netStats.connectedDevices.size() > 10) {
                std::cout << "    ... and "
                          << (netStats.connectedDevices.size() - 10)
                          << " more devices\n";
            }
        } else {
            std::cout << "\nConnected Devices:      No devices detected or "
                         "information unavailable\n";
        }

        std::cout
            << "\n✓ Network statistics gathering completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering network statistics: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates WiFi network scanning and analysis
 */
void demonstrateWiFiScanning() {
    std::cout << createSectionHeader("WiFi Network Scanning and Analysis");

    try {
        std::cout << "Scanning for available WiFi networks...\n\n";

        auto availableNetworks = scanAvailableNetworks();

        if (availableNetworks.empty()) {
            std::cout
                << "No WiFi networks found or WiFi scanning not available.\n";
            std::cout
                << "This may be normal on systems without WiFi capability.\n";
            return;
        }

        std::cout << "Available WiFi Networks:\n";
        std::cout << "  Networks Found:       " << availableNetworks.size()
                  << "\n\n";

        for (size_t i = 0; i < std::min(availableNetworks.size(), size_t(15));
             ++i) {
            std::cout << "  " << (i + 1) << ". " << availableNetworks[i]
                      << "\n";
        }

        if (availableNetworks.size() > 15) {
            std::cout << "  ... and " << (availableNetworks.size() - 15)
                      << " more networks\n";
        }

        // Network analysis
        std::cout << "\nWiFi Environment Analysis:\n";
        if (availableNetworks.size() > 20) {
            std::cout << "  Network Density:      🔴 VERY HIGH - Potential "
                         "interference\n";
            std::cout << "  Recommendation:       Consider 5GHz networks for "
                         "better performance\n";
        } else if (availableNetworks.size() > 10) {
            std::cout << "  Network Density:      🟠 HIGH - Some interference "
                         "possible\n";
            std::cout
                << "  Recommendation:       Monitor for performance issues\n";
        } else if (availableNetworks.size() > 5) {
            std::cout << "  Network Density:      🟡 MODERATE - Normal urban "
                         "environment\n";
        } else {
            std::cout
                << "  Network Density:      🟢 LOW - Good WiFi environment\n";
        }

        std::cout << "\n✓ WiFi network scanning completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error scanning WiFi networks: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates network connectivity and quality analysis
 */
void demonstrateConnectivityAnalysis() {
    std::cout << createSectionHeader(
        "Network Connectivity and Quality Analysis");

    try {
        std::cout << "Analyzing network connectivity and quality...\n\n";

        // Check internet connectivity
        std::cout << "Internet Connectivity Test:\n";
        try {
            bool connected = isConnectedToInternet();
            std::cout << "  Internet Access:      "
                      << (connected ? "🟢 CONNECTED" : "🔴 DISCONNECTED")
                      << "\n";

            if (!connected) {
                std::cout << "  Status:               No internet connection "
                             "detected\n";
                std::cout << "  Recommendation:       Check network settings "
                             "and cables\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  Internet Access:      ✗ Error: " << e.what()
                      << "\n";
        }

        // Bandwidth measurement
        std::cout << "\nBandwidth Measurement:\n";
        try {
            std::cout << "  Measuring bandwidth... (this may take a moment)\n";
            auto bandwidth = measureBandwidth();

            std::cout << "  Upload Speed:         " << std::fixed
                      << std::setprecision(2) << bandwidth.first << " MB/s\n";
            std::cout << "  Download Speed:       " << std::fixed
                      << std::setprecision(2) << bandwidth.second << " MB/s\n";

            // Bandwidth analysis
            if (bandwidth.second > 50.0) {
                std::cout << "  Bandwidth Quality:    🟢 EXCELLENT - "
                             "High-speed connection\n";
            } else if (bandwidth.second > 10.0) {
                std::cout << "  Bandwidth Quality:    🟡 GOOD - Adequate for "
                             "most uses\n";
            } else if (bandwidth.second > 1.0) {
                std::cout << "  Bandwidth Quality:    🟠 FAIR - Basic internet "
                             "usage\n";
            } else {
                std::cout << "  Bandwidth Quality:    🔴 POOR - Limited "
                             "functionality\n";
            }

        } catch (const std::exception& e) {
            std::cerr << "  Bandwidth Test:       ✗ Error: " << e.what()
                      << "\n";
        }

        // Network security information
        std::cout << "\nNetwork Security Information:\n";
        try {
            auto security = getNetworkSecurity();
            std::cout << "  Security Details:     "
                      << (security.empty() ? "Information not available"
                                           : security)
                      << "\n";
        } catch (const std::exception& e) {
            std::cerr << "  Security Info:        ✗ Error: " << e.what()
                      << "\n";
        }

        // Network quality analysis
        std::cout << "\nNetwork Quality Assessment:\n";
        try {
            auto quality = analyzeNetworkQuality();
            std::cout << "  Quality Analysis:     "
                      << (quality.empty() ? "Analysis not available" : quality)
                      << "\n";
        } catch (const std::exception& e) {
            std::cerr << "  Quality Analysis:     ✗ Error: " << e.what()
                      << "\n";
        }

        std::cout << "\n✓ Network connectivity and quality analysis completed "
                     "successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error in connectivity analysis: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive network capabilities
 */
int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "=== Atom Sysinfo - Network and WiFi Analysis Example ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "This example demonstrates comprehensive network and WiFi "
                 "information\n";
    std::cout << "gathering and analysis capabilities with detailed "
                 "performance metrics.\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Execute all demonstration functions
        demonstrateNetworkStatistics();
        demonstrateWiFiScanning();
        demonstrateConnectivityAnalysis();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        // Final summary
        std::cout << createSectionHeader("Execution Summary");
        std::cout << "All network and WiFi analysis completed successfully!\n";
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ Network statistics and performance monitoring\n";
        std::cout << "  ✓ WiFi network scanning and analysis\n";
        std::cout << "  ✓ Internet connectivity testing\n";
        std::cout << "  ✓ Bandwidth measurement and analysis\n";
        std::cout << "  ✓ Network security information gathering\n";
        std::cout << "  ✓ Network quality assessment\n";
        std::cout << "  ✓ Signal strength monitoring\n";
        std::cout << "  ✓ Cross-platform network information gathering\n";

        std::cout << "\n" << std::string(DISPLAY_WIDTH, '=') << "\n";
        std::cout << "Example completed successfully!\n";
        std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";

    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(DISPLAY_WIDTH, '=') << "\n";
        std::cerr << "CRITICAL ERROR: " << e.what() << "\n";
        std::cerr << std::string(DISPLAY_WIDTH, '=') << "\n";
        return 1;
    }

    return 0;
}
