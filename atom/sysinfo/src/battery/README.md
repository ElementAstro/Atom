# Atom Battery Module

A comprehensive cross-platform battery management and monitoring library for C++20.

## Features

### Core Functionality
- **Battery Information**: Get detailed battery status, health, and specifications
- **Real-time Monitoring**: Monitor battery changes with customizable intervals
- **Cross-platform Support**: Windows, macOS, and Linux compatibility
- **Power Management**: Control system power plans and profiles

### Advanced Features
- **Battery Analytics**: Historical data tracking and statistics
- **Alert System**: Configurable alerts for low battery, high temperature, etc.
- **Thermal Management**: Monitor and respond to battery temperature changes
- **Battery Calibration**: Tools for battery health optimization
- **Power Profile Optimization**: Automatic power plan switching based on battery state
- **Energy Consumption Tracking**: Detailed power usage analytics

## Quick Start

```cpp
#include "atom/sysinfo/battery/battery.hpp"

using namespace atom::system;

// Get basic battery information
auto batteryInfo = getBatteryInfo();
if (batteryInfo) {
    std::cout << "Battery Level: " << batteryInfo->batteryLifePercent << "%\n";
    std::cout << "Charging: " << (batteryInfo->isCharging ? "Yes" : "No") << "\n";
}

// Start monitoring
BatteryManager& manager = BatteryManager::getInstance();
manager.startMonitoring(5000); // 5 second intervals

// Set up alerts
manager.setAlertCallback([](AlertType type, const BatteryInfo& info) {
    std::cout << "Battery Alert: " << static_cast<int>(type) << "\n";
});
```

## API Overview

### Core Classes
- `BatteryInfo`: Comprehensive battery information structure
- `BatteryMonitor`: Real-time battery status monitoring
- `BatteryManager`: Advanced battery management with alerts and analytics
- `PowerPlanManager`: System power plan control
- `BatteryCalibrator`: Battery health optimization tools
- `ThermalManager`: Battery thermal monitoring and management

### Platform Support
- **Windows**: Full support via Windows API and WMI
- **macOS**: IOKit integration for comprehensive battery data
- **Linux**: sysfs and upower integration

## Building

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Dependencies
- C++20 compatible compiler
- spdlog for logging
- Platform-specific libraries (automatically linked)

## Examples
See the `examples/` directory for comprehensive usage examples.

## Testing
Run tests with:
```bash
cd build
ctest
```

## License
Copyright (C) 2023-2024 Max Qian <lightapt.com>
