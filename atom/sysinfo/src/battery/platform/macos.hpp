#ifndef ATOM_SYSINFO_BATTERY_MACOS_HPP
#define ATOM_SYSINFO_BATTERY_MACOS_HPP

#ifdef __APPLE__

#include "common.hpp"
#include <optional>
#include <vector>

namespace atom::system::battery::macos {

/**
 * @brief macOS-specific battery information retrieval
 */
class MacOSBatteryProvider {
public:
    /**
     * @brief Get basic battery information using IOKit
     */
    static auto getBatteryInfo() -> std::optional<BatteryInfo>;

    /**
     * @brief Get detailed battery information
     */
    static auto getDetailedBatteryInfo() -> BatteryResult;

    /**
     * @brief Get information for all batteries in the system
     */
    static auto getAllBatteries() -> MultiBatteryInfo;

    /**
     * @brief Get battery information using IOPowerSources
     */
    static auto getBatteryInfoIOPowerSources() -> std::optional<BatteryInfo>;

    /**
     * @brief Get thermal information for battery
     */
    static auto getBatteryThermalInfo() -> std::optional<float>;

    /**
     * @brief Check if system supports advanced battery features
     */
    static auto supportsAdvancedFeatures() -> bool;

private:
    /**
     * @brief Helper to get string value from CFDictionary
     */
    static auto getStringValue(void* dict, const char* key) -> std::string;

    /**
     * @brief Helper to get integer value from CFDictionary
     */
    static auto getIntValue(void* dict, const char* key) -> int;

    /**
     * @brief Helper to get float value from CFDictionary
     */
    static auto getFloatValue(void* dict, const char* key, float scale = 1.0f) -> float;

    /**
     * @brief Helper to get boolean value from CFDictionary
     */
    static auto getBoolValue(void* dict, const char* key) -> bool;

    /**
     * @brief Convert macOS power state to PowerState
     */
    static auto convertPowerState(const std::string& state, bool isCharging) -> PowerState;

    /**
     * @brief Convert macOS battery type to BatteryChemistry
     */
    static auto convertChemistry(const std::string& type) -> BatteryChemistry;
};

/**
 * @brief macOS power plan management
 */
class MacOSPowerManager {
public:
    /**
     * @brief Set active power plan (limited on macOS)
     */
    static auto setPowerPlan(PowerPlan plan) -> std::optional<bool>;

    /**
     * @brief Get current active power plan
     */
    static auto getCurrentPowerPlan() -> std::optional<PowerPlan>;

    /**
     * @brief Get list of available power plans
     */
    static auto getAvailablePowerPlans() -> std::vector<std::string>;

    /**
     * @brief Check if Energy Saver is enabled
     */
    static auto isEnergySaverEnabled() -> bool;

    /**
     * @brief Set Energy Saver state
     */
    static auto setEnergySaver(bool enabled) -> bool;

    /**
     * @brief Get power adapter information
     */
    static auto getPowerAdapterInfo() -> std::optional<std::string>;
};

/**
 * @brief macOS-specific thermal management
 */
class MacOSThermalManager {
public:
    /**
     * @brief Get system thermal state
     */
    static auto getThermalState() -> std::optional<int>;

    /**
     * @brief Check if thermal throttling is active
     */
    static auto isThermalThrottling() -> bool;

    /**
     * @brief Get CPU temperature (if available)
     */
    static auto getCpuTemperature() -> std::optional<float>;

    /**
     * @brief Get fan speeds
     */
    static auto getFanSpeeds() -> std::vector<int>;
};

}  // namespace atom::system::battery::macos

#endif  // __APPLE__

#endif  // ATOM_SYSINFO_BATTERY_MACOS_HPP
