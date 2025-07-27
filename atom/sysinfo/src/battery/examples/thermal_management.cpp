/**
 * @file thermal_management.cpp
 * @brief Thermal management example
 */

#include "atom/sysinfo/battery/battery.hpp"
#include <iostream>
#include <iomanip>

using namespace atom::system::battery;

int main() {
    std::cout << "Thermal Management Example" << std::endl;
    std::cout << "==========================" << std::endl << std::endl;

    // Configure thermal settings
    ThermalSettings settings;
    settings.warningTemperature = 35.0f;
    settings.criticalTemperature = 45.0f;
    settings.shutdownTemperature = 55.0f;
    settings.enableThermalThrottling = true;

    ThermalManager::setThermalSettings(settings);
    std::cout << "Thermal settings configured:" << std::endl;
    std::cout << "  Warning: " << settings.warningTemperature << "°C" << std::endl;
    std::cout << "  Critical: " << settings.criticalTemperature << "°C" << std::endl;
    std::cout << "  Shutdown: " << settings.shutdownTemperature << "°C" << std::endl;
    std::cout << std::endl;

    // Enable thermal monitoring
    ThermalManager::setThermalMonitoring(true);
    std::cout << "Thermal monitoring enabled" << std::endl;

    // Get current temperature
    auto temp = ThermalManager::getSystemTemperature();
    if (temp) {
        std::cout << "Current system temperature: " << std::fixed << std::setprecision(1)
                  << *temp << "°C" << std::endl;

        if (*temp >= settings.criticalTemperature) {
            std::cout << "WARNING: Critical temperature reached!" << std::endl;
        } else if (*temp >= settings.warningTemperature) {
            std::cout << "CAUTION: High temperature detected" << std::endl;
        } else {
            std::cout << "Temperature is within normal range" << std::endl;
        }
    } else {
        std::cout << "Temperature information not available" << std::endl;
    }

    // Check thermal throttling
    bool throttling = ThermalManager::isThermalThrottling();
    std::cout << "Thermal throttling: " << (throttling ? "Active" : "Inactive") << std::endl;

    return 0;
}
