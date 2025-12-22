/**
 * @file battery_example.cpp
 * @brief Comprehensive example demonstrating battery information and monitoring
 *
 * This example provides a complete demonstration of battery information
 * gathering and monitoring capabilities available in the Atom Sysinfo module.
 *
 * Features demonstrated:
 * - Battery presence detection
 * - Battery charge level and status
 * - Battery health calculation
 * - Charging status monitoring
 * - Battery capacity and energy information
 * - Battery temperature monitoring
 * - Battery cycle count tracking
 * - Estimated time remaining calculation
 * - Cross-platform battery information gathering
 *
 * Platform support:
 * - Windows (with WMI and Power Management)
 * - Linux (with /sys/class/power_supply)
 * - macOS (with IOKit)
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 1.0 - Comprehensive battery monitoring demonstration
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
// Atom Sysinfo module headers
#include "atom/sysinfo/hardware/battery.hpp"

using namespace atom::system;

namespace {
constexpr int DISPLAY_WIDTH = 80;
constexpr float HEALTH_WARNING_THRESHOLD = 80.0f;
constexpr float HEALTH_CRITICAL_THRESHOLD = 60.0f;
constexpr float CHARGE_LOW_THRESHOLD = 20.0f;
constexpr float CHARGE_CRITICAL_THRESHOLD = 10.0f;
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
 * @brief Create a simple text-based progress bar
 */
std::string createProgressBar(float percentage, int width = 40) {
    int filledWidth = static_cast<int>((percentage / 100.0f) * width);
    std::string bar = "[";

    for (int i = 0; i < width; ++i) {
        if (i < filledWidth) {
            if (percentage > 80.0f)
                bar += "█";
            else if (percentage > 50.0f)
                bar += "▓";
            else
                bar += "▒";
        } else {
            bar += "░";
        }
    }

    bar += "] " + std::to_string(static_cast<int>(percentage)) + "%";
    return bar;
}

/**
 * @brief Convert battery error to string
 */
std::string batteryErrorToString(BatteryError error) {
    switch (error) {
        case BatteryError::NOT_PRESENT:
            return "Battery not present";
        case BatteryError::ACCESS_DENIED:
            return "Access denied";
        case BatteryError::NOT_SUPPORTED:
            return "Operation not supported";
        case BatteryError::INVALID_DATA:
            return "Invalid battery data";
        case BatteryError::READ_ERROR:
            return "Error reading battery information";
        default:
            return "Unknown error";
    }
}

/**
 * @brief Demonstrates comprehensive battery information
 */
void demonstrateBatteryInformation() {
    std::cout << createSectionHeader("Comprehensive Battery Information");

    try {
        std::cout << "Gathering comprehensive battery information...\n\n";

        auto batteryInfoOpt = getBatteryInfo();

        if (!batteryInfoOpt) {
            std::cerr << "✗ Error: Failed to get battery information\n";
            return;
        }

        auto batteryInfo = *batteryInfoOpt;

        if (!batteryInfo.isBatteryPresent) {
            std::cout << "No battery detected in this system.\n";
            std::cout << "This may be a desktop computer or the battery is not "
                         "properly connected.\n";
            return;
        }

        // Basic Battery Information
        std::cout << "Basic Battery Information:\n";
        std::cout << "  Battery Present:      Yes ✓\n";
        std::cout << "  Manufacturer:         " << batteryInfo.manufacturer
                  << "\n";
        std::cout << "  Model:                " << batteryInfo.model << "\n";
        std::cout << "  Serial Number:        " << batteryInfo.serialNumber
                  << "\n";

        // Charging Status
        std::cout << "\nCharging Status:\n";
        std::cout << "  Charging:             "
                  << (batteryInfo.isCharging ? "Yes ⚡" : "No") << "\n";
        std::cout << "  Charge Level:         " << std::fixed
                  << std::setprecision(1) << batteryInfo.batteryLifePercent
                  << "%\n";
        std::cout << "  Charge Bar:           "
                  << createProgressBar(batteryInfo.batteryLifePercent) << "\n";

        if (batteryInfo.batteryLifePercent < CHARGE_CRITICAL_THRESHOLD) {
            std::cout << "  Charge Status:        🔴 CRITICAL - Charge "
                         "immediately!\n";
        } else if (batteryInfo.batteryLifePercent < CHARGE_LOW_THRESHOLD) {
            std::cout
                << "  Charge Status:        🟠 LOW - Please charge soon\n";
        } else if (batteryInfo.batteryLifePercent < 50.0f) {
            std::cout << "  Charge Status:        🟡 MODERATE - Consider "
                         "charging\n";
        } else {
            std::cout
                << "  Charge Status:        🟢 GOOD - Sufficient charge\n";
        }

        // Battery Health
        std::cout << "\nBattery Health:\n";
        float health = batteryInfo.getBatteryHealth();
        std::cout << "  Battery Health:       " << std::fixed
                  << std::setprecision(1) << health << "%\n";
        std::cout << "  Health Bar:           " << createProgressBar(health)
                  << "\n";

        if (health < HEALTH_CRITICAL_THRESHOLD) {
            std::cout << "  Health Status:        🔴 CRITICAL - Battery needs "
                         "replacement\n";
        } else if (health < HEALTH_WARNING_THRESHOLD) {
            std::cout << "  Health Status:        🟠 WARNING - Battery "
                         "degrading\n";
        } else if (health < 95.0f) {
            std::cout << "  Health Status:        🟡 GOOD - Normal wear\n";
        } else {
            std::cout << "  Health Status:        🟢 EXCELLENT - Like new\n";
        }

        std::cout << "  Cycle Count:          " << batteryInfo.cycleCounts
                  << "\n";

        // Energy Information
        std::cout << "\nEnergy Information:\n";
        std::cout << "  Current Energy:       " << std::fixed
                  << std::setprecision(2) << batteryInfo.energyNow << " Wh\n";
        std::cout << "  Full Charge Capacity: " << std::fixed
                  << std::setprecision(2) << batteryInfo.energyFull << " Wh\n";
        std::cout << "  Design Capacity:      " << std::fixed
                  << std::setprecision(2) << batteryInfo.energyDesign
                  << " Wh\n";
        std::cout << "  Voltage:              " << std::fixed
                  << std::setprecision(2) << batteryInfo.voltageNow << " V\n";
        std::cout << "  Current:              " << std::fixed
                  << std::setprecision(2) << batteryInfo.currentNow << " A\n";

        // Temperature
        if (batteryInfo.temperature > 0.0f) {
            std::cout << "\nTemperature:\n";
            std::cout << "  Battery Temperature:  " << std::fixed
                      << std::setprecision(1) << batteryInfo.temperature
                      << "°C\n";

            if (batteryInfo.temperature > 45.0f) {
                std::cout << "  Temperature Status:   🔴 HOT - Battery may be "
                             "overheating\n";
            } else if (batteryInfo.temperature > 35.0f) {
                std::cout << "  Temperature Status:   🟠 WARM - Normal under "
                             "load\n";
            } else {
                std::cout << "  Temperature Status:   🟢 NORMAL - Good "
                             "temperature\n";
            }
        }

        // Time Estimates
        std::cout << "\nTime Estimates:\n";
        float timeRemaining = batteryInfo.getEstimatedTimeRemaining();
        if (timeRemaining > 0.0f) {
            int hours = static_cast<int>(timeRemaining);
            int minutes = static_cast<int>((timeRemaining - hours) * 60);
            std::cout << "  Estimated Time:       " << hours << "h " << minutes
                      << "m\n";
        } else {
            std::cout << "  Estimated Time:       "
                      << (batteryInfo.isCharging ? "Charging..." : "N/A")
                      << "\n";
        }

        std::cout
            << "\n✓ Battery information gathering completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering battery information: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive battery capabilities
 */
int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "=== Atom Sysinfo - Battery Information and Monitoring "
                 "Example ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "This example demonstrates comprehensive battery information "
                 "gathering\n";
    std::cout << "and monitoring capabilities with detailed analysis.\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Execute demonstration
        demonstrateBatteryInformation();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        // Final summary
        std::cout << createSectionHeader("Execution Summary");
        std::cout << "Battery information gathering completed!\n";
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ Battery presence detection\n";
        std::cout << "  ✓ Charge level and status monitoring\n";
        std::cout << "  ✓ Battery health calculation\n";
        std::cout << "  ✓ Energy and capacity information\n";
        std::cout << "  ✓ Temperature monitoring\n";
        std::cout << "  ✓ Time remaining estimation\n";
        std::cout << "  ✓ Cross-platform battery information gathering\n";

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
