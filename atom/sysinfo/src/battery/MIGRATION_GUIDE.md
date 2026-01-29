# Battery Module Migration Guide

## Overview

This guide helps you migrate from the old monolithic battery module to the new modular battery system. The new system provides enhanced functionality while maintaining backward compatibility.

## What's New

### Enhanced Features

- **Multi-battery support**: Handle systems with multiple batteries
- **Advanced monitoring**: Real-time alerts and statistics
- **Battery calibration**: Optimize battery health and accuracy
- **Thermal management**: Monitor and respond to temperature changes
- **Adaptive power management**: AI-based power optimization
- **Cross-platform improvements**: Better platform-specific implementations
- **Enhanced data logging**: CSV export and historical data tracking

### Improved Architecture

- **Modular design**: Platform-specific implementations separated
- **Better error handling**: More detailed error information
- **Enhanced type safety**: Stronger typing with enums
- **Thread safety**: Improved concurrent access handling
- **Resource management**: Better cleanup and lifecycle management

## Backward Compatibility

The new module maintains full backward compatibility with the old API. Existing code will continue to work without changes:

```cpp
// Old code continues to work
#include "atom/sysinfo/battery.hpp"

using namespace atom::system;

auto info = getBatteryInfo();
if (info) {
    std::cout << "Battery: " << info->batteryLifePercent << "%" << std::endl;
}
```

## Migration Steps

### Step 1: Update Include Paths (Optional)

For new code, you can use the new modular includes:

```cpp
// Old way (still works)
#include "atom/sysinfo/battery.hpp"
using namespace atom::system;

// New way (recommended for new code)
#include "atom/sysinfo/battery/battery.hpp"
using namespace atom::system::battery;
```

### Step 2: Enhanced Error Handling

The new module provides more detailed error information:

```cpp
// Old way
auto info = getBatteryInfo();
if (!info) {
    std::cout << "Failed to get battery info" << std::endl;
}

// New way (enhanced)
auto result = getDetailedBatteryInfo();
if (auto* info = std::get_if<BatteryInfo>(&result)) {
    // Use battery info
} else {
    auto error = std::get<BatteryError>(result);
    switch (error) {
        case BatteryError::NOT_PRESENT:
            std::cout << "No battery detected" << std::endl;
            break;
        case BatteryError::ACCESS_DENIED:
            std::cout << "Access denied to battery information" << std::endl;
            break;
        // Handle other errors...
    }
}
```

### Step 3: Utilize New Features

#### Multi-Battery Support

```cpp
// Get information for all batteries
auto allBatteries = getAllBatteries();
if (!allBatteries.isEmpty()) {
    std::cout << "Found " << allBatteries.size() << " batteries" << std::endl;
    std::cout << "Combined level: " << allBatteries.combined.batteryLifePercent << "%" << std::endl;

    for (const auto& battery : allBatteries.batteries) {
        std::cout << "Battery: " << battery.batteryLifePercent << "%" << std::endl;
    }
}
```

#### Advanced Monitoring with Alerts

```cpp
// Old monitoring (still works)
BatteryMonitor::startMonitoring([](const BatteryInfo& info) {
    std::cout << "Battery: " << info.batteryLifePercent << "%" << std::endl;
}, 5000);

// New advanced monitoring
auto& manager = BatteryManager::getInstance();

// Set up alerts
BatteryAlertSettings settings;
settings.lowBatteryThreshold = 20.0f;
settings.criticalBatteryThreshold = 5.0f;
manager.setAlertSettings(settings);

manager.setAlertCallback([](AlertType type, const BatteryInfo& info) {
    switch (type) {
        case AlertType::LOW_BATTERY:
            std::cout << "Low battery alert: " << info.batteryLifePercent << "%" << std::endl;
            break;
        case AlertType::CRITICAL_BATTERY:
            std::cout << "Critical battery alert!" << std::endl;
            break;
        // Handle other alerts...
    }
});

manager.startMonitoring(5000);
manager.startRecording("battery_log.csv");
```

#### Enhanced Battery Information

```cpp
// Access new battery properties
auto result = getDetailedBatteryInfo();
if (auto* info = std::get_if<BatteryInfo>(&result)) {
    // New properties available
    std::cout << "Chemistry: " << static_cast<int>(info->chemistry) << std::endl;
    std::cout << "Power State: " << static_cast<int>(info->powerState) << std::endl;
    std::cout << "Technology: " << info->technology << std::endl;
    std::cout << "Power Consumption: " << info->getPowerConsumption() << " W" << std::endl;
    std::cout << "Battery Age: " << info->getBatteryAge() << "%" << std::endl;
    std::cout << "Is Critical: " << (info->isCritical() ? "Yes" : "No") << std::endl;
}
```

### Step 4: Power Management Enhancements

#### Adaptive Power Management

```cpp
// Enable adaptive power management
if (AdaptivePowerManager::enableAdaptivePower()) {
    std::cout << "Adaptive power management enabled" << std::endl;

    // Set optimization profile
    AdaptivePowerManager::setOptimizationProfile("gaming");
}
```

#### Enhanced Power Plan Control

```cpp
// Old way (still works)
PowerPlanManager::setPowerPlan(PowerPlan::POWER_SAVER);

// New way with better error handling
auto result = PowerPlanManager::setPowerPlan(PowerPlan::POWER_SAVER);
if (result && *result) {
    std::cout << "Power plan set successfully" << std::endl;
} else if (result && !*result) {
    std::cout << "Failed to set power plan" << std::endl;
} else {
    std::cout << "Power plan management not supported" << std::endl;
}
```

### Step 5: Thermal Management

```cpp
// Configure thermal management
ThermalSettings thermalSettings;
thermalSettings.warningTemperature = 40.0f;
thermalSettings.criticalTemperature = 50.0f;
thermalSettings.enableThermalThrottling = true;

ThermalManager::setThermalSettings(thermalSettings);
ThermalManager::setThermalMonitoring(true);

// Check thermal status
if (ThermalManager::isThermalThrottling()) {
    std::cout << "System is thermal throttling" << std::endl;
}

auto temp = ThermalManager::getSystemTemperature();
if (temp) {
    std::cout << "System temperature: " << *temp << "°C" << std::endl;
}
```

### Step 6: Battery Calibration

```cpp
// Check if calibration is needed
if (BatteryCalibrator::needsCalibration()) {
    std::cout << "Battery calibration recommended" << std::endl;

    // Start calibration
    if (BatteryCalibrator::startCalibration()) {
        std::cout << "Calibration started" << std::endl;

        // Monitor progress
        while (BatteryCalibrator::isCalibrating()) {
            float progress = BatteryCalibrator::getCalibrationProgress();
            std::cout << "Calibration progress: " << progress << "%" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }

        std::cout << "Calibration completed" << std::endl;
    }
}
```

## Breaking Changes

### None for Basic Usage

The new module maintains full backward compatibility for basic battery information retrieval and monitoring.

### Advanced Features Only

Breaking changes only affect advanced features that didn't exist in the old module:

- New enum values in existing enums
- New optional parameters in some functions
- Enhanced error reporting (more detailed error types)

## Performance Improvements

### Reduced Memory Usage

- More efficient data structures
- Better memory management
- Reduced allocations in hot paths

### Improved Platform Integration

- Native platform APIs used more efficiently
- Reduced system call overhead
- Better caching of frequently accessed data

### Enhanced Threading

- Better thread safety
- Reduced lock contention
- More efficient background monitoring

## Recommended Migration Timeline

### Phase 1: Update Dependencies

- Update to the new battery module
- Verify existing functionality works
- Run existing tests

### Phase 2: Enhance Error Handling

- Update error handling to use new detailed error types
- Add proper error recovery logic
- Update logging to include more detailed error information

### Phase 3: Add New Features

- Implement battery alerts where appropriate
- Add thermal monitoring for critical applications
- Consider adaptive power management for mobile applications

### Phase 4: Optimize

- Use multi-battery support if applicable
- Implement battery calibration workflows
- Add comprehensive logging and monitoring

## Support and Resources

### Documentation

- [API Reference](API_REFERENCE.md)
- [README](README.md)
- Inline code documentation

### Examples

- See `examples/` directory for comprehensive usage examples
- Each example demonstrates specific features
- Examples include error handling and best practices

### Testing

- Run the test suite to verify functionality
- Use `simple_test.cpp` for basic validation
- Platform-specific tests available

### Getting Help

- Check the examples for similar use cases
- Review the API documentation for detailed function descriptions
- Test on your target platforms to ensure compatibility
