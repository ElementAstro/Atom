/**
 * @file voltage_monitoring.cpp
 * @brief Comprehensive example demonstrating voltage and power monitoring
 *
 * This example showcases voltage monitoring capabilities including:
 * - Input voltage monitoring and analysis
 * - Battery voltage tracking and health assessment
 * - Power source detection and management
 * - Voltage threshold monitoring and alerts
 * - Power consumption analysis
 * - Cross-platform power management
 *
 * @note Cross-platform compatibility: Windows, Linux, macOS
 * @note May require elevated privileges for hardware access
 * @note Actual voltage readings depend on hardware support
 * @author Atom Framework
 * @date 2024
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>
#include "atom/system/voltage.hpp"

using namespace atom::system;

/**
 * @brief Print a formatted section header
 */
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

/**
 * @brief Format voltage value for display
 */
std::string formatVoltage(std::optional<double> voltage) {
    if (!voltage.has_value()) {
        return "N/A";
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << voltage.value() << " V";
    return oss.str();
}

/**
 * @brief Analyze voltage stability
 */
void analyzeVoltageStability(const std::vector<double>& readings,
                             const std::string& source) {
    if (readings.empty()) {
        std::cout << "No readings available for " << source << std::endl;
        return;
    }

    double sum = 0.0;
    double min_val = readings[0];
    double max_val = readings[0];

    for (double reading : readings) {
        sum += reading;
        min_val = std::min(min_val, reading);
        max_val = std::max(max_val, reading);
    }

    double average = sum / readings.size();
    double variance = 0.0;

    for (double reading : readings) {
        variance += (reading - average) * (reading - average);
    }
    variance /= readings.size();
    double stddev = std::sqrt(variance);

    std::cout << source << " Voltage Analysis:" << std::endl;
    std::cout << "  Readings: " << readings.size() << std::endl;
    std::cout << "  Average: " << std::fixed << std::setprecision(3) << average
              << " V" << std::endl;
    std::cout << "  Minimum: " << std::fixed << std::setprecision(3) << min_val
              << " V" << std::endl;
    std::cout << "  Maximum: " << std::fixed << std::setprecision(3) << max_val
              << " V" << std::endl;
    std::cout << "  Range: " << std::fixed << std::setprecision(3)
              << (max_val - min_val) << " V" << std::endl;
    std::cout << "  Std Dev: " << std::fixed << std::setprecision(3) << stddev
              << " V" << std::endl;

    // Stability assessment
    double stability_threshold = 0.1;  // 0.1V variation considered stable
    if ((max_val - min_val) <= stability_threshold) {
        std::cout << "  Status: STABLE ✓" << std::endl;
    } else {
        std::cout << "  Status: UNSTABLE ⚠️" << std::endl;
    }
}

/**
 * @brief Assess battery health based on voltage
 */
std::string assessBatteryHealth(double voltage) {
    // These are general guidelines - actual values depend on battery type
    if (voltage >= 12.6) {
        return "Excellent (100%)";
    } else if (voltage >= 12.4) {
        return "Good (75-99%)";
    } else if (voltage >= 12.2) {
        return "Fair (50-74%)";
    } else if (voltage >= 12.0) {
        return "Poor (25-49%)";
    } else if (voltage >= 11.8) {
        return "Critical (0-24%)";
    } else {
        return "Depleted/Damaged";
    }
}

int main() {
    try {
        std::cout << "=== Voltage and Power Monitoring Example ==="
                  << std::endl;
        std::cout
            << "Demonstrating comprehensive voltage monitoring capabilities\n"
            << std::endl;

        // 1. Get Voltage Monitor Instance
        printSection("Voltage Monitor Initialization");

        auto monitor = getVoltageMonitor();
        if (!monitor) {
            std::cout << "Failed to get voltage monitor instance" << std::endl;
            std::cout << "This may indicate:" << std::endl;
            std::cout << "- No voltage monitoring hardware available"
                      << std::endl;
            std::cout << "- Insufficient permissions" << std::endl;
            std::cout << "- Platform not supported" << std::endl;
            return 1;
        }

        std::cout << "Voltage monitor initialized successfully" << std::endl;

        // 2. Basic Voltage Readings
        printSection("Basic Voltage Readings");

        auto inputVoltage = monitor->getInputVoltage();
        auto batteryVoltage = monitor->getBatteryVoltage();
        auto powerSources = monitor->getPowerSources();

        std::cout << "Current Voltage Readings:" << std::endl;
        std::cout << "  Input Voltage: " << formatVoltage(inputVoltage)
                  << std::endl;
        std::cout << "  Battery Voltage: " << formatVoltage(batteryVoltage)
                  << std::endl;
        std::cout << "  Power Sources: " << powerSources.size() << " detected"
                  << std::endl;

        // 3. Power Source Analysis
        printSection("Power Source Analysis");

        if (powerSources.empty()) {
            std::cout << "No power sources detected" << std::endl;
        } else {
            std::cout << "Detected Power Sources:" << std::endl;
            for (size_t i = 0; i < powerSources.size(); i++) {
                const auto& source = powerSources[i];
                std::cout << "\nSource " << (i + 1) << ":" << std::endl;
                std::cout << "  Name: " << source.name << std::endl;
                std::cout << "  Type: " << source.type << std::endl;
                std::cout << "  Voltage: " << formatVoltage(source.voltage)
                          << std::endl;
                std::cout << "  Current: ";
                if (source.current.has_value()) {
                    std::cout << std::fixed << std::setprecision(3)
                              << source.current.value() << " A";
                } else {
                    std::cout << "N/A";
                }
                std::cout << std::endl;
                std::cout << "  Power: ";
                if (source.power.has_value()) {
                    std::cout << std::fixed << std::setprecision(3)
                              << source.power.value() << " W";
                } else {
                    std::cout << "N/A";
                }
                std::cout << std::endl;
                std::cout << "  Available: "
                          << (source.available ? "Yes" : "No") << std::endl;
            }
        }

        // 4. Battery Health Assessment
        printSection("Battery Health Assessment");

        if (batteryVoltage.has_value()) {
            double voltage = batteryVoltage.value();
            std::string health = assessBatteryHealth(voltage);

            std::cout << "Battery Health Assessment:" << std::endl;
            std::cout << "  Current Voltage: " << std::fixed
                      << std::setprecision(3) << voltage << " V" << std::endl;
            std::cout << "  Health Status: " << health << std::endl;

            // Voltage-based recommendations
            if (voltage < 11.8) {
                std::cout << "  ⚠️  WARNING: Battery voltage critically low!"
                          << std::endl;
                std::cout
                    << "  Recommendation: Charge immediately or replace battery"
                    << std::endl;
            } else if (voltage < 12.0) {
                std::cout << "  ⚠️  CAUTION: Battery voltage low" << std::endl;
                std::cout << "  Recommendation: Charge soon" << std::endl;
            } else if (voltage >= 12.6) {
                std::cout << "  ✓ Battery voltage optimal" << std::endl;
            }
        } else {
            std::cout << "Battery voltage not available" << std::endl;
            std::cout << "This may indicate:" << std::endl;
            std::cout << "- No battery present" << std::endl;
            std::cout << "- Battery monitoring not supported" << std::endl;
            std::cout << "- Desktop system without battery" << std::endl;
        }

        // 5. Voltage Monitoring Over Time
        printSection("Voltage Monitoring Over Time");

        std::cout << "Monitoring voltages for 10 seconds..." << std::endl;
        std::cout << "Time\t\tInput V\t\tBattery V" << std::endl;
        std::cout << std::string(50, '-') << std::endl;

        std::vector<double> inputReadings;
        std::vector<double> batteryReadings;

        for (int i = 0; i < 10; i++) {
            auto currentInput = monitor->getInputVoltage();
            auto currentBattery = monitor->getBatteryVoltage();

            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);

            std::cout << std::put_time(std::localtime(&time_t), "%H:%M:%S")
                      << "\t";

            if (currentInput.has_value()) {
                std::cout << std::fixed << std::setprecision(3)
                          << currentInput.value() << " V\t\t";
                inputReadings.push_back(currentInput.value());
            } else {
                std::cout << "N/A\t\t";
            }

            if (currentBattery.has_value()) {
                std::cout << std::fixed << std::setprecision(3)
                          << currentBattery.value() << " V";
                batteryReadings.push_back(currentBattery.value());
            } else {
                std::cout << "N/A";
            }

            std::cout << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // 6. Voltage Stability Analysis
        printSection("Voltage Stability Analysis");

        if (!inputReadings.empty()) {
            analyzeVoltageStability(inputReadings, "Input");
        }

        if (!batteryReadings.empty()) {
            analyzeVoltageStability(batteryReadings, "Battery");
        }

        // 7. Power Consumption Estimation
        printSection("Power Consumption Estimation");

        if (inputVoltage.has_value()) {
            double voltage = inputVoltage.value();

            // Find current from power sources
            double totalCurrent = 0.0;
            bool currentAvailable = false;

            for (const auto& source : powerSources) {
                if (source.current.has_value()) {
                    totalCurrent += source.current.value();
                    currentAvailable = true;
                }
            }

            if (currentAvailable) {
                double power = voltage * totalCurrent;
                std::cout << "Power Consumption Estimate:" << std::endl;
                std::cout << "  Voltage: " << std::fixed << std::setprecision(3)
                          << voltage << " V" << std::endl;
                std::cout << "  Current: " << std::fixed << std::setprecision(3)
                          << totalCurrent << " A" << std::endl;
                std::cout << "  Power: " << std::fixed << std::setprecision(3)
                          << power << " W" << std::endl;

                // Energy consumption over time
                double energyPerHour = power;  // Wh
                std::cout << "  Energy per hour: " << std::fixed
                          << std::setprecision(3) << energyPerHour << " Wh"
                          << std::endl;
                std::cout << "  Energy per day: " << std::fixed
                          << std::setprecision(3) << (energyPerHour * 24)
                          << " Wh" << std::endl;
            } else {
                std::cout
                    << "Current information not available for power calculation"
                    << std::endl;
            }
        }

        // 8. Voltage Thresholds and Alerts
        printSection("Voltage Thresholds and Alerts");

        std::cout << "Voltage Threshold Monitoring:" << std::endl;

        // Define thresholds
        double inputLowThreshold = 11.0;
        double inputHighThreshold = 15.0;
        double batteryLowThreshold = 11.8;
        double batteryCriticalThreshold = 11.0;

        std::cout << "Defined Thresholds:" << std::endl;
        std::cout << "  Input Low: " << inputLowThreshold << " V" << std::endl;
        std::cout << "  Input High: " << inputHighThreshold << " V"
                  << std::endl;
        std::cout << "  Battery Low: " << batteryLowThreshold << " V"
                  << std::endl;
        std::cout << "  Battery Critical: " << batteryCriticalThreshold << " V"
                  << std::endl;

        // Check current readings against thresholds
        std::cout << "\nCurrent Status:" << std::endl;

        if (inputVoltage.has_value()) {
            double voltage = inputVoltage.value();
            if (voltage < inputLowThreshold) {
                std::cout << "  🔴 INPUT VOLTAGE LOW ALERT: " << voltage << " V"
                          << std::endl;
            } else if (voltage > inputHighThreshold) {
                std::cout << "  🟡 INPUT VOLTAGE HIGH WARNING: " << voltage
                          << " V" << std::endl;
            } else {
                std::cout << "  🟢 Input voltage normal: " << voltage << " V"
                          << std::endl;
            }
        }

        if (batteryVoltage.has_value()) {
            double voltage = batteryVoltage.value();
            if (voltage < batteryCriticalThreshold) {
                std::cout << "  🔴 BATTERY CRITICAL ALERT: " << voltage << " V"
                          << std::endl;
            } else if (voltage < batteryLowThreshold) {
                std::cout << "  🟡 BATTERY LOW WARNING: " << voltage << " V"
                          << std::endl;
            } else {
                std::cout << "  🟢 Battery voltage normal: " << voltage << " V"
                          << std::endl;
            }
        }

        std::cout << "\n=== Voltage Monitoring Complete ===" << std::endl;
        std::cout << "This example demonstrated:" << std::endl;
        std::cout << "- Input and battery voltage monitoring" << std::endl;
        std::cout << "- Power source detection and analysis" << std::endl;
        std::cout << "- Battery health assessment" << std::endl;
        std::cout << "- Voltage stability analysis over time" << std::endl;
        std::cout << "- Power consumption estimation" << std::endl;
        std::cout << "- Voltage threshold monitoring and alerts" << std::endl;
        std::cout << "- Cross-platform voltage monitoring" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Voltage monitoring error: " << e.what() << std::endl;
        std::cerr << "\nPossible causes:" << std::endl;
        std::cerr << "- No voltage monitoring hardware available" << std::endl;
        std::cerr
            << "- Insufficient permissions (try running as administrator/root)"
            << std::endl;
        std::cerr << "- Platform not supported" << std::endl;
        std::cerr << "- Hardware access restrictions" << std::endl;
        return 1;
    }

    return 0;
}
