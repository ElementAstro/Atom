#ifdef _WIN32

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <windows.h>

#include "voltage.hpp"

namespace atom::system {

/**
 * @brief WindowsVoltageMonitor class implementation for voltage monitoring on
 * Windows systems.
 *
 * This class provides methods to retrieve voltage and power source information
 * from the Windows operating system using Windows API functions.
 */
class WindowsVoltageMonitor : public VoltageMonitor {
public:
    /**
     * @brief Default constructor for the WindowsVoltageMonitor.
     */
    WindowsVoltageMonitor();

    /**
     * @brief Default virtual destructor for proper cleanup.
     */
    ~WindowsVoltageMonitor() override;

    /**
     * @brief Gets the input voltage in volts (V).
     * @return An optional double representing the input voltage, or
     * std::nullopt if not available.
     * @override
     */
    std::optional<double> getInputVoltage() const override;

    /**
     * @brief Gets the battery voltage in volts (V).
     * @return An optional double representing the battery voltage, or
     * std::nullopt if not available.
     * @override
     */
    std::optional<double> getBatteryVoltage() const override;

    /**
     * @brief Gets information about all available power sources.
     * @return A vector of PowerSourceInfo structures, each representing a power
     * source.
     * @override
     */
    std::vector<PowerSourceInfo> getAllPowerSources() const override;

    /**
     * @brief Gets the name of the platform the monitor is running on.
     * @return A string representing the platform name (e.g., "Windows").
     * @override
     */
    std::string getPlatformName() const override;

private:
    /**
     * @brief Cached battery state information.
     */
    mutable SYSTEM_BATTERY_STATE batteryState;

    /**
     * @brief Flag indicating whether the cached battery state is valid.
     */
    mutable bool batteryStateValid = false;

    /**
     * @brief Updates the cached battery state information.
     * @return True if the battery state was successfully updated, false
     * otherwise.
     */
    bool updateBatteryState() const;

    /**
     * @brief Retrieves more detailed power information from WMI (Windows
     * Management Instrumentation).
     * @return A vector of PowerSourceInfo structures containing power source
     * information from WMI.
     */
    std::vector<PowerSourceInfo> getWMIPowerInfo() const;
};

}  // namespace atom::system

#endif  // _WIN32