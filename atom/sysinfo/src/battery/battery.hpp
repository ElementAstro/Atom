#ifndef ATOM_SYSINFO_BATTERY_BATTERY_HPP
#define ATOM_SYSINFO_BATTERY_BATTERY_HPP

#include "common.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace atom::system::battery {

/**
 * @brief Get basic battery information (cross-platform)
 */
[[nodiscard]] auto getBatteryInfo() -> std::optional<BatteryInfo>;

/**
 * @brief Get detailed battery information (cross-platform)
 */
[[nodiscard]] auto getDetailedBatteryInfo() -> BatteryResult;

/**
 * @brief Get information for all batteries in the system
 */
[[nodiscard]] auto getAllBatteries() -> MultiBatteryInfo;

/**
 * @brief Monitors battery status changes
 */
class BatteryMonitor {
public:
    /**
     * @brief Callback function type for battery status updates
     */
    using BatteryCallback = std::function<void(const BatteryInfo&)>;

    /**
     * @brief Start monitoring battery status
     */
    static auto startMonitoring(BatteryCallback callback, unsigned int interval_ms = 1000) -> bool;

    /**
     * @brief Stop monitoring battery status
     */
    static void stopMonitoring();

    /**
     * @brief Check if monitoring is active
     */
    [[nodiscard]] static auto isMonitoring() noexcept -> bool;
};

/**
 * @brief Advanced battery management with alerts and analytics
 */
class BatteryManager {
public:
    /**
     * @brief Callback function type for battery alerts
     */
    using AlertCallback = std::function<void(AlertType alert, const BatteryInfo&)>;

    /**
     * @brief Gets the singleton instance of the BatteryManager
     */
    [[nodiscard]] static auto getInstance() -> BatteryManager&;

    ~BatteryManager();

    BatteryManager(const BatteryManager&) = delete;
    BatteryManager(BatteryManager&&) = delete;
    auto operator=(const BatteryManager&) -> BatteryManager& = delete;
    auto operator=(BatteryManager&&) -> BatteryManager& = delete;

    /**
     * @brief Sets the callback function for battery alerts
     */
    void setAlertCallback(AlertCallback callback);

    /**
     * @brief Configures the thresholds for battery alerts
     */
    void setAlertSettings(const BatteryAlertSettings& settings);

    /**
     * @brief Gets the current battery usage statistics
     */
    [[nodiscard]] auto getStats() const -> const BatteryStats&;

    /**
     * @brief Starts recording battery history data to a log file
     */
    auto startRecording(std::string_view logFile = "") -> bool;

    /**
     * @brief Stops recording battery history data
     */
    void stopRecording();

    /**
     * @brief Starts monitoring battery status for alerts and statistics
     */
    auto startMonitoring(unsigned int interval_ms = 10000) -> bool;

    /**
     * @brief Stops monitoring battery status
     */
    void stopMonitoring();

    /**
     * @brief Gets the recorded battery history
     */
    [[nodiscard]] auto getHistory(unsigned int maxEntries = 0) const
        -> std::vector<std::pair<std::chrono::system_clock::time_point, BatteryInfo>>;

private:
    BatteryManager();
    class BatteryManagerImpl;
    std::unique_ptr<BatteryManagerImpl> impl;
};

/**
 * @brief Manages system power plans
 */
class PowerPlanManager {
public:
    /**
     * @brief Sets the active system power plan
     */
    [[nodiscard]] static auto setPowerPlan(PowerPlan plan) -> std::optional<bool>;

    /**
     * @brief Gets the current active system power plan
     */
    [[nodiscard]] static auto getCurrentPowerPlan() -> std::optional<PowerPlan>;

    /**
     * @brief Gets a list of available power plan names
     */
    [[nodiscard]] static auto getAvailablePowerPlans() -> std::vector<std::string>;
};

/**
 * @brief Battery calibration and health optimization
 */
class BatteryCalibrator {
public:
    /**
     * @brief Start battery calibration process
     */
    static auto startCalibration() -> bool;

    /**
     * @brief Stop battery calibration process
     */
    static void stopCalibration();

    /**
     * @brief Check if calibration is in progress
     */
    [[nodiscard]] static auto isCalibrating() -> bool;

    /**
     * @brief Get calibration progress (0-100%)
     */
    [[nodiscard]] static auto getCalibrationProgress() -> float;

    /**
     * @brief Get calibration data
     */
    [[nodiscard]] static auto getCalibrationData() -> CalibrationData;

    /**
     * @brief Check if battery needs calibration
     */
    [[nodiscard]] static auto needsCalibration() -> bool;
};

/**
 * @brief Thermal management for battery safety
 */
class ThermalManager {
public:
    /**
     * @brief Set thermal management settings
     */
    static void setThermalSettings(const ThermalSettings& settings);

    /**
     * @brief Get current thermal settings
     */
    [[nodiscard]] static auto getThermalSettings() -> ThermalSettings;

    /**
     * @brief Check if thermal throttling is active
     */
    [[nodiscard]] static auto isThermalThrottling() -> bool;

    /**
     * @brief Get current system temperature
     */
    [[nodiscard]] static auto getSystemTemperature() -> std::optional<float>;

    /**
     * @brief Enable/disable thermal monitoring
     */
    static void setThermalMonitoring(bool enabled);

    /**
     * @brief Check if thermal monitoring is enabled
     */
    [[nodiscard]] static auto isThermalMonitoringEnabled() -> bool;
};

/**
 * @brief Adaptive power management
 */
class AdaptivePowerManager {
public:
    /**
     * @brief Enable adaptive power management
     */
    static auto enableAdaptivePower() -> bool;

    /**
     * @brief Disable adaptive power management
     */
    static void disableAdaptivePower();

    /**
     * @brief Check if adaptive power is enabled
     */
    [[nodiscard]] static auto isAdaptivePowerEnabled() -> bool;

    /**
     * @brief Set power optimization profile
     */
    static auto setOptimizationProfile(const std::string& profile) -> bool;

    /**
     * @brief Get current optimization profile
     */
    [[nodiscard]] static auto getCurrentProfile() -> std::string;

    /**
     * @brief Get available optimization profiles
     */
    [[nodiscard]] static auto getAvailableProfiles() -> std::vector<std::string>;
};

}  // namespace atom::system::battery

#endif  // ATOM_SYSINFO_BATTERY_BATTERY_HPP
