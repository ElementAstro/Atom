# Battery Module API Reference

## Overview

The Atom Battery Module provides comprehensive cross-platform battery management and monitoring capabilities for C++20 applications. This document describes the complete API available in the `atom::system::battery` namespace.

## Core Data Types

### BatteryInfo

Comprehensive battery information structure containing all available battery data.

```cpp
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

    // Methods
    auto getBatteryHealth() const -> float;
    auto getEstimatedTimeRemaining() const -> float;
    auto getPowerConsumption() const -> float;
    auto isCritical() const -> bool;
    auto getBatteryAge() const -> float;
};
```

### Enumerations

#### BatteryError
```cpp
enum class BatteryError {
    NOT_PRESENT,    // Battery not detected
    ACCESS_DENIED,  // Access to battery information denied
    NOT_SUPPORTED,  // Operation not supported
    INVALID_DATA,   // Invalid battery data
    READ_ERROR      // Error reading battery information
};
```

#### BatteryChemistry
```cpp
enum class BatteryChemistry {
    UNKNOWN,
    LITHIUM_ION,
    LITHIUM_POLYMER,
    NICKEL_METAL_HYDRIDE,
    NICKEL_CADMIUM,
    LEAD_ACID,
    ALKALINE
};
```

#### PowerState
```cpp
enum class PowerState {
    UNKNOWN,
    DISCHARGING,
    CHARGING,
    FULL,
    NOT_CHARGING,
    CRITICAL
};
```

#### PowerPlan
```cpp
enum class PowerPlan {
    BALANCED,
    PERFORMANCE,
    POWER_SAVER,
    CUSTOM,
    ADAPTIVE,      // AI-based adaptive power management
    GAMING,        // Optimized for gaming
    PRESENTATION   // Optimized for presentations
};
```

#### AlertType
```cpp
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
```

## Core Functions

### Battery Information Retrieval

#### getBatteryInfo()
```cpp
auto getBatteryInfo() -> std::optional<BatteryInfo>;
```
Gets basic battery information. Returns `std::nullopt` if no battery is present or accessible.

#### getDetailedBatteryInfo()
```cpp
auto getDetailedBatteryInfo() -> BatteryResult;
```
Gets detailed battery information including manufacturer, model, and advanced metrics. Returns either `BatteryInfo` or `BatteryError`.

#### getAllBatteries()
```cpp
auto getAllBatteries() -> MultiBatteryInfo;
```
Gets information for all batteries in the system. Useful for laptops with multiple batteries.

## Classes

### BatteryMonitor

Real-time battery status monitoring.

```cpp
class BatteryMonitor {
public:
    using BatteryCallback = std::function<void(const BatteryInfo&)>;

    static auto startMonitoring(BatteryCallback callback,
                               unsigned int interval_ms = 1000) -> bool;
    static void stopMonitoring();
    static auto isMonitoring() noexcept -> bool;
};
```

**Usage Example:**
```cpp
BatteryMonitor::startMonitoring([](const BatteryInfo& info) {
    std::cout << "Battery: " << info.batteryLifePercent << "%" << std::endl;
}, 5000);  // 5-second intervals
```

### BatteryManager

Advanced battery management with alerts, statistics, and data recording.

```cpp
class BatteryManager {
public:
    using AlertCallback = std::function<void(AlertType alert, const BatteryInfo&)>;

    static auto getInstance() -> BatteryManager&;

    void setAlertCallback(AlertCallback callback);
    void setAlertSettings(const BatteryAlertSettings& settings);
    auto getStats() const -> const BatteryStats&;
    auto startRecording(std::string_view logFile = "") -> bool;
    void stopRecording();
    auto startMonitoring(unsigned int interval_ms = 10000) -> bool;
    void stopMonitoring();
    auto getHistory(unsigned int maxEntries = 0) const
        -> std::vector<std::pair<std::chrono::system_clock::time_point, BatteryInfo>>;
};
```

**Usage Example:**
```cpp
auto& manager = BatteryManager::getInstance();
manager.setAlertCallback([](AlertType type, const BatteryInfo& info) {
    if (type == AlertType::LOW_BATTERY) {
        std::cout << "Low battery: " << info.batteryLifePercent << "%" << std::endl;
    }
});
manager.startMonitoring(5000);
manager.startRecording("battery_log.csv");
```

### PowerPlanManager

System power plan control.

```cpp
class PowerPlanManager {
public:
    static auto setPowerPlan(PowerPlan plan) -> std::optional<bool>;
    static auto getCurrentPowerPlan() -> std::optional<PowerPlan>;
    static auto getAvailablePowerPlans() -> std::vector<std::string>;
};
```

**Usage Example:**
```cpp
// Set power plan based on battery level
auto batteryInfo = getBatteryInfo();
if (batteryInfo && batteryInfo->batteryLifePercent < 20.0f) {
    PowerPlanManager::setPowerPlan(PowerPlan::POWER_SAVER);
}
```

### BatteryCalibrator

Battery health optimization and calibration.

```cpp
class BatteryCalibrator {
public:
    static auto startCalibration() -> bool;
    static void stopCalibration();
    static auto isCalibrating() -> bool;
    static auto getCalibrationProgress() -> float;
    static auto getCalibrationData() -> CalibrationData;
    static auto needsCalibration() -> bool;
};
```

### ThermalManager

Battery thermal monitoring and management.

```cpp
class ThermalManager {
public:
    static void setThermalSettings(const ThermalSettings& settings);
    static auto getThermalSettings() -> ThermalSettings;
    static auto isThermalThrottling() -> bool;
    static auto getSystemTemperature() -> std::optional<float>;
    static void setThermalMonitoring(bool enabled);
    static auto isThermalMonitoringEnabled() -> bool;
};
```

### AdaptivePowerManager

AI-based adaptive power management.

```cpp
class AdaptivePowerManager {
public:
    static auto enableAdaptivePower() -> bool;
    static void disableAdaptivePower();
    static auto isAdaptivePowerEnabled() -> bool;
    static auto setOptimizationProfile(const std::string& profile) -> bool;
    static auto getCurrentProfile() -> std::string;
    static auto getAvailableProfiles() -> std::vector<std::string>;
};
```

## Platform Support

### Windows
- Full support via Windows API and WMI
- Advanced battery information through device APIs
- Power plan management through Windows power APIs
- Thermal information where available

### macOS
- IOKit integration for comprehensive battery data
- Power source information through IOPowerSources
- Limited power plan control (macOS manages power automatically)
- Thermal monitoring through system APIs

### Linux
- sysfs integration for battery information
- UPower support where available
- powerprofilesctl integration for power plan management
- Thermal zone monitoring

## Error Handling

All functions that can fail return either `std::optional` or `std::variant` types to indicate success or failure. Always check return values:

```cpp
auto batteryInfo = getBatteryInfo();
if (batteryInfo) {
    // Use batteryInfo.value() or *batteryInfo
} else {
    // Handle no battery case
}

auto detailedResult = getDetailedBatteryInfo();
if (auto* info = std::get_if<BatteryInfo>(&detailedResult)) {
    // Use info
} else {
    auto error = std::get<BatteryError>(detailedResult);
    // Handle error
}
```

## Thread Safety

- All static functions are thread-safe
- BatteryManager is a singleton and thread-safe
- Monitoring callbacks may be called from background threads
- Use appropriate synchronization when accessing shared data from callbacks
