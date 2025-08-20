#ifndef ATOM_SYSTEM_MODULE_BATTERY_HPP
#define ATOM_SYSTEM_MODULE_BATTERY_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Represents possible error types for battery operations.
 */
enum class BatteryError {
    NOT_PRESENT,    //!< Battery not detected.
    ACCESS_DENIED,  //!< Access to battery information denied.
    NOT_SUPPORTED,  //!< Operation not supported.
    INVALID_DATA,   //!< Invalid battery data.
    READ_ERROR      //!< Error reading battery information.
};

/**
 * @brief Battery information.
 */
struct BatteryInfo {
    bool isBatteryPresent = false;
    bool isCharging = false;
    float batteryLifePercent = 0.0f;
    float batteryLifeTime = 0.0f;
    float batteryFullLifeTime = 0.0f;
    float energyNow = 0.0f;
    float energyFull = 0.0f;
    float energyDesign = 0.0f;
    float voltageNow = 0.0f;
    float currentNow = 0.0f;
    float temperature = 0.0f;
    int cycleCounts = 0;
    std::string manufacturer;
    std::string model;
    std::string serialNumber;

    BatteryInfo() = default;
    BatteryInfo(const BatteryInfo&) = default;
    BatteryInfo(BatteryInfo&&) noexcept = default;
    auto operator=(const BatteryInfo&) -> BatteryInfo& = default;
    auto operator=(BatteryInfo&&) noexcept -> BatteryInfo& = default;

    /**
     * @brief Equality comparison operator.
     * @param other The other BatteryInfo object to compare with.
     * @return True if the objects are equal, false otherwise.
     */
    auto operator==(const BatteryInfo& other) const -> bool;

    /**
     * @brief Inequality comparison operator.
     * @param other The other BatteryInfo object to compare with.
     * @return True if the objects are not equal, false otherwise.
     */
    auto operator!=(const BatteryInfo& other) const -> bool;

    /**
     * @brief Calculate battery health (0-100%).
     * @return Battery health percentage.
     */
    [[nodiscard]] auto getBatteryHealth() const -> float;

    /**
     * @brief Estimate remaining usage time (hours).
     * @return Estimated time remaining in hours.
     */
    [[nodiscard]] auto getEstimatedTimeRemaining() const -> float;
} ATOM_ALIGNAS(64);

/**
 * @brief Result type for battery data operations, containing either BatteryInfo
 * or an error code.
 */
using BatteryResult = std::variant<BatteryInfo, BatteryError>;

/**
 * @brief Get basic battery information.
 * @return An std::optional containing BatteryInfo if successful, or
 * std::nullopt on error.
 */
[[nodiscard]] auto getBatteryInfo() -> std::optional<BatteryInfo>;

/**
 * @brief Get detailed battery information (including manufacturer, model,
 * etc.).
 * @return A BatteryResult containing either BatteryInfo or a BatteryError.
 */
[[nodiscard]] auto getDetailedBatteryInfo() -> BatteryResult;

/**
 * @brief Monitors battery status changes.
 */
class BatteryMonitor {
public:
    /**
     * @brief Callback function type for battery status updates.
     * @param info The updated BatteryInfo.
     */
    using BatteryCallback = std::function<void(const BatteryInfo&)>;

    /**
     * @brief Start monitoring battery status.
     * @param callback Callback function to handle battery status updates.
     * @param interval_ms Monitoring interval in milliseconds.
     * @return True if monitoring started successfully, false otherwise.
     */
    static auto startMonitoring(BatteryCallback callback,
                                unsigned int interval_ms = 1000) -> bool;

    /**
     * @brief Stop monitoring battery status.
     */
    static void stopMonitoring();

    /**
     * @brief Check if monitoring is active.
     * @return True if monitoring is active, false otherwise.
     */
    [[nodiscard]] static auto isMonitoring() noexcept -> bool;
};

/**
 * @brief Settings for battery alerts.
 */
struct BatteryAlertSettings {
    float lowBatteryThreshold = 20.0f;
    float criticalBatteryThreshold = 5.0f;
    float highTempThreshold = 45.0f;
    float lowHealthThreshold = 60.0f;
};

/**
 * @brief Types of battery alerts.
 */
enum class AlertType {
    LOW_BATTERY,
    CRITICAL_BATTERY,
    HIGH_TEMPERATURE,
    LOW_BATTERY_HEALTH
};

/**
 * @brief Battery usage statistics.
 */
struct BatteryStats {
    float averagePowerConsumption = 0.0f;
    float totalEnergyConsumed = 0.0f;
    float averageDischargeRate = 0.0f;
    std::chrono::seconds totalUptime{0};
    float minBatteryLevel = 100.0f;
    float maxBatteryLevel = 0.0f;
    float minTemperature = 100.0f;
    float maxTemperature = 0.0f;
    float minVoltage = 100.0f;
    float maxVoltage = 0.0f;
    float avgDischargeRate = -1.0f;
    int cycleCount = 0;
    float batteryHealth = 100.0f;
};

/**
 * @brief Manages battery information, monitoring, alerts, and statistics.
 * This class is a singleton.
 */
class BatteryManager {
public:
    /**
     * @brief Callback function type for battery alerts.
     * @param alert The type of alert.
     * @param info The BatteryInfo at the time of the alert.
     */
    using AlertCallback =
        std::function<void(AlertType alert, const BatteryInfo&)>;

    /**
     * @brief Gets the singleton instance of the BatteryManager.
     * @return Reference to the BatteryManager instance.
     */
    [[nodiscard]] static auto getInstance() -> BatteryManager&;

    ~BatteryManager();

    BatteryManager(const BatteryManager&) = delete;
    BatteryManager(BatteryManager&&) = delete;
    auto operator=(const BatteryManager&) -> BatteryManager& = delete;
    auto operator=(BatteryManager&&) -> BatteryManager& = delete;

    /**
     * @brief Sets the callback function for battery alerts.
     * @param callback The function to call when an alert is triggered.
     */
    void setAlertCallback(AlertCallback callback);

    /**
     * @brief Configures the thresholds for battery alerts.
     * @param settings The BatteryAlertSettings to apply.
     */
    void setAlertSettings(const BatteryAlertSettings& settings);

    /**
     * @brief Gets the current battery usage statistics.
     * @return A constant reference to the BatteryStats.
     */
    [[nodiscard]] auto getStats() const -> const BatteryStats&;

    /**
     * @brief Starts recording battery history data to a log file.
     * @param logFile Optional path to the log file. If empty, data is recorded
     * in memory only.
     * @return True if recording started successfully, false otherwise.
     */
    auto startRecording(std::string_view logFile = "") -> bool;

    /**
     * @brief Stops recording battery history data.
     */
    void stopRecording();

    /**
     * @brief Starts monitoring battery status for alerts and statistics.
     * @param interval_ms Monitoring interval in milliseconds.
     * @return True if monitoring started successfully, false otherwise.
     */
    auto startMonitoring(unsigned int interval_ms = 10000) -> bool;

    /**
     * @brief Stops monitoring battery status.
     */
    void stopMonitoring();

    /**
     * @brief Gets the recorded battery history.
     * @param maxEntries Maximum number of entries to return. If 0, returns all
     * available history.
     * @return A vector of pairs containing timestamp and BatteryInfo.
     */
    [[nodiscard]] auto getHistory(unsigned int maxEntries = 0) const
        -> std::vector<
            std::pair<std::chrono::system_clock::time_point, BatteryInfo>>;

private:
    BatteryManager();
    class BatteryManagerImpl;
    std::unique_ptr<BatteryManagerImpl> impl;
};

/**
 * @brief Represents system power plans.
 */
enum class PowerPlan { BALANCED, PERFORMANCE, POWER_SAVER, CUSTOM };

/**
 * @brief Manages system power plans.
 */
class PowerPlanManager {
public:
    /**
     * @brief Sets the active system power plan.
     * @param plan The PowerPlan to set.
     * @return An std::optional containing true if successful, false on failure,
     * or std::nullopt if not supported.
     */
    [[nodiscard]] static auto setPowerPlan(PowerPlan plan)
        -> std::optional<bool>;

    /**
     * @brief Gets the current active system power plan.
     * @return An std::optional containing the current PowerPlan, or
     * std::nullopt on error.
     */
    [[nodiscard]] static auto getCurrentPowerPlan() -> std::optional<PowerPlan>;

    /**
     * @brief Gets a list of available power plan names.
     * @return A vector of strings representing available power plans.
     */
    [[nodiscard]] static auto getAvailablePowerPlans()
        -> std::vector<std::string>;
};

}  // namespace atom::system
#endif  // ATOM_SYSTEM_MODULE_BATTERY_HPP
