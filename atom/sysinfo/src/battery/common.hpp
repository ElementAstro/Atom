#ifndef ATOM_SYSINFO_BATTERY_COMMON_HPP
#define ATOM_SYSINFO_BATTERY_COMMON_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system::battery {

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
 * @brief Battery chemistry types
 */
enum class BatteryChemistry {
    UNKNOWN,
    LITHIUM_ION,
    LITHIUM_POLYMER,
    NICKEL_METAL_HYDRIDE,
    NICKEL_CADMIUM,
    LEAD_ACID,
    ALKALINE
};

/**
 * @brief Battery power state
 */
enum class PowerState {
    UNKNOWN,
    DISCHARGING,
    CHARGING,
    FULL,
    NOT_CHARGING,
    CRITICAL
};

/**
 * @brief Enhanced battery information structure
 */
struct BatteryInfo {
    // Basic information
    bool isBatteryPresent = false;
    bool isCharging = false;
    float batteryLifePercent = 0.0f;
    float batteryLifeTime = 0.0f;
    float batteryFullLifeTime = 0.0f;

    // Energy information
    float energyNow = 0.0f;          // Current energy (Wh)
    float energyFull = 0.0f;         // Full charge energy (Wh)
    float energyDesign = 0.0f;       // Design energy (Wh)
    float powerNow = 0.0f;           // Current power consumption (W)

    // Electrical properties
    float voltageNow = 0.0f;         // Current voltage (V)
    float voltageMin = 0.0f;         // Minimum voltage (V)
    float voltageMax = 0.0f;         // Maximum voltage (V)
    float currentNow = 0.0f;         // Current flow (A)

    // Physical properties
    float temperature = 0.0f;        // Temperature (°C)
    int cycleCounts = 0;             // Charge cycles

    // Device information
    std::string manufacturer;
    std::string model;
    std::string serialNumber;
    std::string technology;
    BatteryChemistry chemistry = BatteryChemistry::UNKNOWN;
    PowerState powerState = PowerState::UNKNOWN;

    // Timestamps
    std::chrono::system_clock::time_point lastUpdated;

    BatteryInfo() = default;
    BatteryInfo(const BatteryInfo&) = default;
    BatteryInfo(BatteryInfo&&) noexcept = default;
    auto operator=(const BatteryInfo&) -> BatteryInfo& = default;
    auto operator=(BatteryInfo&&) noexcept -> BatteryInfo& = default;

    /**
     * @brief Equality comparison operator.
     */
    auto operator==(const BatteryInfo& other) const -> bool;

    /**
     * @brief Inequality comparison operator.
     */
    auto operator!=(const BatteryInfo& other) const -> bool;

    /**
     * @brief Calculate battery health (0-100%).
     */
    [[nodiscard]] auto getBatteryHealth() const -> float;

    /**
     * @brief Estimate remaining usage time (hours).
     */
    [[nodiscard]] auto getEstimatedTimeRemaining() const -> float;

    /**
     * @brief Get power consumption rate (W).
     */
    [[nodiscard]] auto getPowerConsumption() const -> float;

    /**
     * @brief Check if battery is in critical state.
     */
    [[nodiscard]] auto isCritical() const -> bool;

    /**
     * @brief Get battery age estimation based on cycles.
     */
    [[nodiscard]] auto getBatteryAge() const -> float;
} ATOM_ALIGNAS(64);

/**
 * @brief Result type for battery data operations
 */
using BatteryResult = std::variant<BatteryInfo, BatteryError>;

/**
 * @brief Multiple battery information for systems with multiple batteries
 */
struct MultiBatteryInfo {
    std::vector<BatteryInfo> batteries;
    BatteryInfo combined;  // Combined statistics
    int activeBatteryCount = 0;
    float totalCapacity = 0.0f;
    float totalEnergyRemaining = 0.0f;

    [[nodiscard]] auto isEmpty() const -> bool { return batteries.empty(); }
    [[nodiscard]] auto size() const -> size_t { return batteries.size(); }
    [[nodiscard]] auto getPrimaryBattery() const -> const BatteryInfo*;
};

/**
 * @brief Battery alert settings
 */
struct BatteryAlertSettings {
    float lowBatteryThreshold = 20.0f;
    float criticalBatteryThreshold = 5.0f;
    float highTempThreshold = 45.0f;
    float lowHealthThreshold = 60.0f;
    float highCycleCountThreshold = 1000.0f;
    bool enableTemperatureAlerts = true;
    bool enableHealthAlerts = true;
    bool enableCycleAlerts = true;
};

/**
 * @brief Types of battery alerts
 */
enum class AlertType {
    LOW_BATTERY,
    CRITICAL_BATTERY,
    HIGH_TEMPERATURE,
    LOW_BATTERY_HEALTH,
    HIGH_CYCLE_COUNT,
    BATTERY_REMOVED,
    BATTERY_INSERTED,
    CHARGING_STARTED,
    CHARGING_STOPPED,
    FULL_CHARGE_REACHED
};

/**
 * @brief Battery usage statistics
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
    std::chrono::system_clock::time_point statsStartTime;
    std::chrono::system_clock::time_point lastUpdateTime;
};

/**
 * @brief Power plan types
 */
enum class PowerPlan {
    BALANCED,
    PERFORMANCE,
    POWER_SAVER,
    CUSTOM,
    ADAPTIVE,      // AI-based adaptive power management
    GAMING,        // Optimized for gaming
    PRESENTATION   // Optimized for presentations
};

/**
 * @brief Thermal management settings
 */
struct ThermalSettings {
    float warningTemperature = 40.0f;
    float criticalTemperature = 50.0f;
    float shutdownTemperature = 60.0f;
    bool enableThermalThrottling = true;
    bool enableFanControl = false;
};

/**
 * @brief Battery calibration data
 */
struct CalibrationData {
    float actualCapacity = 0.0f;
    float designCapacity = 0.0f;
    float calibrationAccuracy = 0.0f;
    std::chrono::system_clock::time_point lastCalibration;
    int calibrationCycles = 0;
    bool needsCalibration = false;
};

}  // namespace atom::system::battery

#endif  // ATOM_SYSINFO_BATTERY_COMMON_HPP
