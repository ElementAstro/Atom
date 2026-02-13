# Atom System Information - Serial Numbers Module

A comprehensive, cross-platform C++ library for collecting system hardware identification information including serial numbers, system UUIDs, network interfaces, memory modules, and more.

## Features

### Core Functionality

- **Hardware Serial Numbers**: BIOS, motherboard, CPU, and disk serial numbers
- **System Identification**: System UUID, machine ID, boot ID, hostname, domain name
- **Network Interfaces**: MAC addresses, interface details, driver information
- **Memory Modules**: Serial numbers, manufacturer, capacity, speed, form factor
- **System Fingerprinting**: Unique hardware-based system identification

### Enhanced Features

- **Cross-Platform Support**: Windows (WMI) and Linux (filesystem/DMI)
- **Caching System**: Configurable result caching with timeout
- **Error Handling**: Comprehensive error reporting and validation
- **Export Formats**: JSON and XML export capabilities
- **Backward Compatibility**: Compatible with original HardwareInfo API
- **Thread Safety**: Thread-safe operations with mutex protection

## Quick Start

### Basic Usage (New API)

```cpp
#include "atom/sysinfo/sn/sn.hpp"

int main() {
    // Create system info instance
    auto sysInfo = atom::system::createSystemInfo();

    // Get hardware serial numbers
    auto hardwareResult = sysInfo->getHardwareSerials();
    if (hardwareResult.success) {
        std::cout << "BIOS Serial: " << hardwareResult.data.biosSerial << std::endl;
        std::cout << "Motherboard Serial: " << hardwareResult.data.motherboardSerial << std::endl;
        std::cout << "CPU Serial: " << hardwareResult.data.cpuSerial << std::endl;
    }

    // Get system fingerprint
    std::string fingerprint = sysInfo->getSystemFingerprint();
    std::cout << "System Fingerprint: " << fingerprint << std::endl;

    return 0;
}
```

### Backward Compatible Usage

```cpp
#include "atom/sysinfo/sn.hpp"

int main() {
    // Original API still works
    HardwareInfo hwInfo;

    std::cout << "BIOS Serial: " << hwInfo.getBiosSerialNumber() << std::endl;
    std::cout << "Motherboard Serial: " << hwInfo.getMotherboardSerialNumber() << std::endl;
    std::cout << "CPU Serial: " << hwInfo.getCpuSerialNumber() << std::endl;

    auto diskSerials = hwInfo.getDiskSerialNumbers();
    for (const auto& serial : diskSerials) {
        std::cout << "Disk Serial: " << serial << std::endl;
    }

    // Access enhanced features
    if (hwInfo.isEnhancedModeAvailable()) {
        std::string fingerprint = hwInfo.getSystemFingerprint();
        std::cout << "System Fingerprint: " << fingerprint << std::endl;
    }

    return 0;
}
```

### Advanced Configuration

```cpp
#include "atom/sysinfo/sn/sn.hpp"

int main() {
    // Configure data collection
    atom::system::SystemInfoConfig config;
    config.includeMemoryModules = true;
    config.includeNetworkInterfaces = true;
    config.cacheResults = true;
    config.cacheTimeout = std::chrono::minutes(10);
    config.enableLogging = true;

    auto sysInfo = atom::system::createSystemInfo(config);

    // Get comprehensive system information
    auto result = sysInfo->getComprehensiveInfo();
    if (result.success) {
        std::cout << result.data.toString() << std::endl;

        // Export to JSON
        std::string json = sysInfo->exportToJson(true);
        std::cout << "JSON Export:\n" << json << std::endl;
    }

    return 0;
}
```

## API Reference

### Core Classes

#### `atom::system::SystemInfo`

Main class for collecting system information with enhanced features.

**Key Methods:**

- `getHardwareSerials()` - Get hardware serial numbers
- `getSystemIdentification()` - Get system identification data
- `getMemoryModules()` - Get memory module information
- `getNetworkInterfaces()` - Get network interface details
- `getComprehensiveInfo()` - Get all available information
- `getSystemFingerprint()` - Get unique system fingerprint
- `exportToJson()` / `exportToXml()` - Export data in various formats

#### `HardwareInfo` (Backward Compatibility)

Original API maintained for backward compatibility.

**Key Methods:**

- `getBiosSerialNumber()` - Get BIOS serial number
- `getMotherboardSerialNumber()` - Get motherboard serial number
- `getCpuSerialNumber()` - Get CPU serial number
- `getDiskSerialNumbers()` - Get disk serial numbers
- `getSystemFingerprint()` - Get system fingerprint (new)
- `isEnhancedModeAvailable()` - Check enhanced features (new)

### Data Structures

#### `HardwareSerialData`

Contains hardware serial number information.

#### `SystemIdentificationData`

Contains system identification information including UUID, machine ID, MAC addresses.

#### `MemoryModuleInfo`

Contains detailed memory module information.

#### `NetworkInterfaceInfo`

Contains network interface details.

#### `ComprehensiveSystemInfo`

Contains all available system information.

## Platform Support

### Windows

- Uses Windows Management Instrumentation (WMI)
- Requires Windows Vista or later
- Supports all hardware serial numbers, memory modules, and network interfaces

### Linux

- Uses DMI (Desktop Management Interface) via `/sys/class/dmi/id`
- Uses `/proc` and `/sys` filesystems
- Supports most hardware information on modern Linux distributions

## Building

### Requirements

- C++20 compatible compiler
- CMake 3.16 or later
- spdlog library

### Build Instructions

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### CMake Integration

```cmake
find_package(atom_sysinfo_sn REQUIRED)
target_link_libraries(your_target atom::atom_sysinfo_sn)
```

## Error Handling

The library provides comprehensive error handling through the `SystemInfoResult` template:

```cpp
auto result = sysInfo->getHardwareSerials();
if (!result.success) {
    std::cerr << "Error: " << result.errorMessage << std::endl;
    std::cerr << "Error Code: " << static_cast<int>(result.error) << std::endl;
}
```

## Caching

The library supports intelligent caching to improve performance:

```cpp
// Configure caching
atom::system::SystemInfoConfig config;
config.cacheResults = true;
config.cacheTimeout = std::chrono::minutes(5);

auto sysInfo = atom::system::createSystemInfo(config);

// First call - data collected from system
auto result1 = sysInfo->getHardwareSerials();

// Second call - data returned from cache (if within timeout)
auto result2 = sysInfo->getHardwareSerials();

// Force refresh
auto result3 = sysInfo->getHardwareSerials(true);
```

## Security Considerations

- Some hardware information may be considered sensitive
- Use the anonymization features for privacy-sensitive applications
- Consider access permissions when deploying on multi-user systems

## License

This module is part of the Atom project and follows the same licensing terms.

## Contributing

Contributions are welcome! Please ensure:

- Code follows the existing style
- Tests are included for new features
- Documentation is updated
- Cross-platform compatibility is maintained
