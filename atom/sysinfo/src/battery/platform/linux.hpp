#ifndef ATOM_SYSINFO_BATTERY_LINUX_HPP
#define ATOM_SYSINFO_BATTERY_LINUX_HPP

#ifdef __linux__

#include "../common.hpp"
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace atom::system::battery::linux {

/**
 * @brief Linux-specific battery information retrieval
 */
class LinuxBatteryProvider {
public:
    /**
     * @brief Get basic battery information using sysfs
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
     * @brief Get battery information using UPower
     */
    static auto getBatteryInfoUPower() -> std::optional<BatteryInfo>;
    
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
     * @brief Helper to read a value from sysfs
     */
    static auto readSysfsValue(const std::string& path) -> std::optional<std::string>;
    
    /**
     * @brief Helper to read a numeric value from sysfs
     */
    template<typename T>
    static auto readSysfsNumeric(const std::string& path) -> std::optional<T>;
    
    /**
     * @brief Get list of battery paths in sysfs
     */
    static auto getBatteryPaths() -> std::vector<std::string>;
    
    /**
     * @brief Convert Linux power supply status to PowerState
     */
    static auto convertPowerState(const std::string& status) -> PowerState;
    
    /**
     * @brief Convert Linux battery technology to BatteryChemistry
     */
    static auto convertChemistry(const std::string& technology) -> BatteryChemistry;
};

/**
 * @brief Linux power plan management
 */
class LinuxPowerManager {
public:
    /**
     * @brief Set active power plan using powerprofilesctl
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
     * @brief Check if TLP is available
     */
    static auto isTLPAvailable() -> bool;
    
    /**
     * @brief Set TLP profile
     */
    static auto setTLPProfile(const std::string& profile) -> bool;
    
    /**
     * @brief Check if power-profiles-daemon is available
     */
    static auto isPowerProfilesDaemonAvailable() -> bool;
};

/**
 * @brief Linux-specific thermal management
 */
class LinuxThermalManager {
public:
    /**
     * @brief Get system thermal zones
     */
    static auto getThermalZones() -> std::vector<std::pair<std::string, float>>;
    
    /**
     * @brief Get CPU temperature
     */
    static auto getCpuTemperature() -> std::optional<float>;
    
    /**
     * @brief Check if thermal throttling is active
     */
    static auto isThermalThrottling() -> bool;
    
    /**
     * @brief Set thermal policy
     */
    static auto setThermalPolicy(const std::string& policy) -> bool;
};

}  // namespace atom::system::battery::linux

#endif  // __linux__

#endif  // ATOM_SYSINFO_BATTERY_LINUX_HPP
