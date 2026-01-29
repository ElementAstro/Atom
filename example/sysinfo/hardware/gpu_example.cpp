/**
 * @file gpu_example.cpp
 * @brief Comprehensive example demonstrating GPU and monitor information
 *
 * This example provides a complete demonstration of GPU and monitor information
 * gathering capabilities available in the Atom Sysinfo module.
 *
 * Features demonstrated:
 * - GPU information retrieval
 * - Monitor detection and enumeration
 * - Monitor resolution and refresh rate information
 * - Multi-monitor setup detection
 * - Cross-platform GPU information gathering
 *
 * Platform support:
 * - Windows (with WMI and Display APIs)
 * - Linux (with X11/Wayland and sysfs)
 * - macOS (with IOKit and CoreGraphics)
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 1.0 - Comprehensive GPU and monitor information demonstration
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// Atom Sysinfo module headers
#include "atom/sysinfo/hardware/gpu.hpp"

using namespace atom::system;

namespace {
constexpr int DISPLAY_WIDTH = 80;
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
 * @brief Demonstrates GPU information
 */
void demonstrateGPUInformation() {
    std::cout << createSectionHeader("GPU Information");

    try {
        std::cout << "Gathering GPU information...\n\n";

        auto gpuInfo = getGPUInfo();

        std::cout << "GPU Details:\n";
        std::cout << gpuInfo << "\n";

        std::cout << "\n✓ GPU information gathering completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering GPU information: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates monitor information
 */
void demonstrateMonitorInformation() {
    std::cout << createSectionHeader("Monitor Information");

    try {
        std::cout << "Gathering monitor information...\n\n";

        auto monitors = getAllMonitorsInfo();

        if (monitors.empty()) {
            std::cout << "No monitors detected.\n";
            return;
        }

        std::cout << "Total Monitors Detected: " << monitors.size() << "\n\n";

        for (size_t i = 0; i < monitors.size(); ++i) {
            const auto& monitor = monitors[i];

            std::cout << "Monitor " << (i + 1) << ":\n";
            std::cout << "  Model:                " << monitor.model << "\n";
            std::cout << "  Identifier:           " << monitor.identifier
                      << "\n";
            std::cout << "  Resolution:           " << monitor.width << " x "
                      << monitor.height << " pixels\n";
            std::cout << "  Refresh Rate:         " << monitor.refreshRate
                      << " Hz\n";

            // Calculate aspect ratio
            int gcd_val = std::__gcd(monitor.width, monitor.height);
            int aspectW = monitor.width / gcd_val;
            int aspectH = monitor.height / gcd_val;
            std::cout << "  Aspect Ratio:         " << aspectW << ":" << aspectH
                      << "\n";

            // Calculate total pixels
            long long totalPixels =
                static_cast<long long>(monitor.width) * monitor.height;
            std::cout << "  Total Pixels:         " << totalPixels << "\n";

            // Determine resolution category
            if (monitor.width >= 3840 && monitor.height >= 2160) {
                std::cout << "  Resolution Category:  4K UHD or higher\n";
            } else if (monitor.width >= 2560 && monitor.height >= 1440) {
                std::cout << "  Resolution Category:  QHD (1440p)\n";
            } else if (monitor.width >= 1920 && monitor.height >= 1080) {
                std::cout << "  Resolution Category:  Full HD (1080p)\n";
            } else if (monitor.width >= 1280 && monitor.height >= 720) {
                std::cout << "  Resolution Category:  HD (720p)\n";
            } else {
                std::cout << "  Resolution Category:  Standard Definition\n";
            }

            // Refresh rate analysis
            if (monitor.refreshRate >= 144) {
                std::cout
                    << "  Refresh Rate Type:    High refresh rate (gaming/"
                       "professional)\n";
            } else if (monitor.refreshRate >= 75) {
                std::cout << "  Refresh Rate Type:    Enhanced (smooth)\n";
            } else if (monitor.refreshRate >= 60) {
                std::cout << "  Refresh Rate Type:    Standard\n";
            } else {
                std::cout << "  Refresh Rate Type:    Low\n";
            }

            if (i < monitors.size() - 1) {
                std::cout << "\n";
            }
        }

        // Multi-monitor analysis
        if (monitors.size() > 1) {
            std::cout << "\nMulti-Monitor Setup Analysis:\n";
            std::cout << "  Total Displays:       " << monitors.size() << "\n";

            int totalWidth = 0;
            int maxHeight = 0;
            for (const auto& monitor : monitors) {
                totalWidth += monitor.width;
                maxHeight = std::max(maxHeight, monitor.height);
            }

            std::cout << "  Combined Resolution:  " << totalWidth << " x "
                      << maxHeight << " (if arranged horizontally)\n";
        }

        std::cout
            << "\n✓ Monitor information gathering completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering monitor information: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive GPU and monitor capabilities
 */
int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "=== Atom Sysinfo - GPU and Monitor Information Example ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "This example demonstrates comprehensive GPU and monitor "
                 "information\n";
    std::cout << "gathering capabilities with detailed analysis.\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Execute all demonstration functions
        demonstrateGPUInformation();
        demonstrateMonitorInformation();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        // Final summary
        std::cout << createSectionHeader("Execution Summary");
        std::cout << "GPU and monitor information gathering completed!\n";
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ GPU information retrieval\n";
        std::cout << "  ✓ Monitor detection and enumeration\n";
        std::cout << "  ✓ Resolution and refresh rate information\n";
        std::cout << "  ✓ Multi-monitor setup analysis\n";
        std::cout << "  ✓ Cross-platform GPU information gathering\n";

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
