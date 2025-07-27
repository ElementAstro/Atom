#ifndef ATOM_SYSINFO_BATTERY_WINDOWS_HPP
#define ATOM_SYSINFO_BATTERY_WINDOWS_HPP

#ifdef _WIN32

#include "common.hpp"
#include <optional>
#include <vector>

namespace atom::system::battery::windows {

/**
 * @brief Windows-specific battery information retrieval
 */
class WindowsBatteryProvider {
public:
    /**
     * @brief Get basic battery information using Windows API
     */
    static auto getBatteryInfo() -> std::optional<BatteryInfo>;

    /**
     * @brief Get detailed battery information using WMI and device APIs
     */
    static auto getDetailedBatteryInfo() -> BatteryResult;

    /**
     * @brief Get information for all batteries in the system
     */
    static auto getAllBatteries() -> MultiBatteryInfo;

    /**
     * @brief Get battery information using WMI
     */
    static auto getBatteryInfoWMI() -> std::optional<BatteryInfo>;

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
     * @brief Helper to get battery device handle
     */
    static auto getBatteryDeviceHandle() -> void*;

    /**
     * @brief Helper to query battery information via IOCTL
     */
    static auto queryBatteryInformation(void* handle, int infoLevel) -> std::vector<uint8_t>;

    /**
     * @brief Convert Windows battery status to PowerState
     */
    static auto convertPowerState(uint32_t status) -> PowerState;

    /**
     * @brief Convert Windows battery chemistry to BatteryChemistry
     */
    static auto convertChemistry(const std::string& chemistry) -> BatteryChemistry;
};

/**
 * @brief Windows power plan management
 */
class WindowsPowerManager {
public:
    /**
     * @brief Set active power plan
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
     * @brief Create custom power plan
     */
    static auto createCustomPowerPlan(const std::string& name) -> std::optional<std::string>;

    /**
     * @brief Delete custom power plan
     */
    static auto deleteCustomPowerPlan(const std::string& guid) -> bool;

private:
    /**
     * @brief Convert PowerPlan enum to Windows GUID
     */
    static auto powerPlanToGuid(PowerPlan plan) -> std::optional<std::string>;

    /**
     * @brief Convert Windows GUID to PowerPlan enum
     */
    static auto guidToPowerPlan(const std::string& guid) -> PowerPlan;
};

/**
 * @brief Windows-specific thermal management
 */
class WindowsThermalManager {
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
     * @brief Set thermal policy
     */
    static auto setThermalPolicy(int policy) -> bool;
};

}  // namespace atom::system::battery::windows

#endif  // _WIN32

#endif  // ATOM_SYSINFO_BATTERY_WINDOWS_HPP
