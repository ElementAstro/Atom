# BIOS Information Module

## Overview

The BIOS Information Module provides comprehensive access to system BIOS/UEFI firmware information and operations across Windows, Linux, and macOS platforms. This module has been completely restructured from a monolithic implementation to a modular, platform-specific architecture while maintaining backward compatibility.

## Features

### Core Features

- **BIOS Information Retrieval**: Version, manufacturer, release date, serial number
- **Health Monitoring**: System health checks, error detection, age analysis
- **SMBIOS Data Access**: Complete SMBIOS table information
- **Secure Boot Management**: Enable/disable Secure Boot (where supported)
- **UEFI Boot Configuration**: UEFI boot mode management
- **Settings Backup/Restore**: BIOS settings backup and restoration

### Enhanced Features (New)

- **Firmware Information**: Detailed firmware type, version, and capabilities
- **Boot Configuration Management**: Boot order, boot devices, boot settings
- **Power Management**: ACPI settings, wake-on-LAN, power profiles
- **Overclocking Information**: CPU/memory frequencies, overclocking profiles
- **Hardware Monitoring**: Temperature sensors, fan speeds, voltages
- **Comprehensive Diagnostics**: POST tests, component validation
- **Security Settings**: TPM, virtualization, hyper-threading status
- **Utility Functions**: Temperature conversion, frequency conversion, validation

## Architecture

The module follows a platform-abstraction pattern:

```
bios/
├── common.hpp          # Common interfaces and structures
├── common.cpp          # Shared implementation and utilities
├── bios.hpp           # Main BIOS class interface
├── bios.cpp           # Main BIOS class implementation
├── windows.hpp        # Windows-specific interface
├── windows.cpp        # Windows WMI implementation
├── linux.hpp          # Linux-specific interface
├── linux.cpp          # Linux dmidecode/sysfs implementation
├── macos.hpp          # macOS-specific interface
├── macos.cpp          # macOS IOKit/system_profiler implementation
└── examples/          # Usage examples
```

## Usage

### Basic Usage

```cpp
#include "atom/sysinfo/bios.hpp"

using namespace atom::system;

// Get BIOS information
auto& biosInfo = BiosInfo::getInstance();
const auto& info = biosInfo.getBiosInfo();

std::cout << "BIOS Version: " << info.version << std::endl;
std::cout << "Manufacturer: " << info.manufacturer << std::endl;
std::cout << "Release Date: " << info.releaseDate << std::endl;
```

### Enhanced Features

```cpp
// Get firmware information
auto firmwareInfo = biosInfo.getFirmwareInfo();
std::cout << "Firmware Type: " << firmwareInfo.type << std::endl;
std::cout << "Secure Boot Capable: " << firmwareInfo.secureBootCapable << std::endl;

// Get boot configuration
auto bootConfig = biosInfo.getBootConfiguration();
std::cout << "UEFI Mode: " << bootConfig.uefiMode << std::endl;

// Hardware monitoring
auto monitoring = biosInfo.getHardwareMonitoring();
for (int temp : monitoring.cpuTemperatures) {
    std::cout << "CPU Temp: " << temp << "°C" << std::endl;
}

// Power management
auto powerSettings = biosInfo.getPowerManagementSettings();
std::cout << "Wake on LAN: " << powerSettings.wakeOnLanEnabled << std::endl;

// Run diagnostics
auto diagnostics = biosInfo.runDiagnostics();
std::cout << "POST Test: " << (diagnostics.postTestPassed ? "PASS" : "FAIL") << std::endl;
```

### Utility Functions

```cpp
// Temperature conversion
double fahrenheit = BiosUtils::celsiusToFahrenheit(25.0);

// Frequency conversion
double ghz = BiosUtils::mhzToGhz(3200);

// Boot time formatting
std::string bootTime = BiosUtils::formatBootTime(15500); // "15.5 s"

// BIOS version validation
bool valid = BiosUtils::isValidBiosVersion("1.2.3");
```

## Platform Support

### Windows

- Uses WMI (Windows Management Instrumentation) for data retrieval
- Supports UEFI variable access
- Requires administrator privileges for modifications
- COM interface management with RAII

### Linux

- Uses `dmidecode` for BIOS information
- Accesses `/sys/firmware/efi` for UEFI data
- Uses `/sys/class/thermal` for temperature monitoring
- Requires root privileges for system modifications

### macOS

- Uses IOKit framework for hardware information
- Integrates with `system_profiler` for detailed data
- Uses `pmset` for power management
- Limited BIOS modification capabilities (by design)

## Error Handling

The module uses comprehensive error handling:

```cpp
// Operation results
enum class BiosOperationResult {
    SUCCESS,
    FAILED,
    NOT_SUPPORTED,
    INSUFFICIENT_PRIVILEGES,
    REBOOT_REQUIRED
};

// Check operation results
auto result = biosInfo.setBootOrder({"USB", "HDD", "Network"});
std::cout << BiosInfo::operationResultToString(result) << std::endl;
```

## Security Considerations

- **Privilege Requirements**: Many operations require elevated privileges
- **System Stability**: BIOS modifications can affect system stability
- **Backup Recommended**: Always backup settings before modifications
- **Platform Limitations**: Some features are platform-specific

## Building

### CMake Integration

```cmake
find_package(atom_sysinfo_bios REQUIRED)
target_link_libraries(your_target atom::atom_sysinfo_bios)
```

### Dependencies

- **spdlog**: Logging framework
- **Platform-specific**:
  - Windows: WMI libraries (wbemuuid, ole32, oleaut32)
  - Linux: dmidecode, efibootmgr (optional)
  - macOS: CoreFoundation, IOKit frameworks

## Examples

See the `examples/` directory for comprehensive usage examples:

- `bios_example.cpp`: Complete feature demonstration
- Build and run: `cd examples && make && ./bios_example`

## Backward Compatibility

The original `bios.hpp` and `bios.cpp` files are maintained as compatibility headers that include the new modular implementation. Existing code should continue to work without modifications.

## Thread Safety

The BiosInfo class uses the singleton pattern and is thread-safe for read operations. Write operations should be synchronized externally if used from multiple threads.

## Limitations

- **Windows**: Some features require specific Windows versions
- **Linux**: Requires dmidecode and appropriate permissions
- **macOS**: Limited BIOS modification capabilities
- **General**: Hardware-specific features may not be available on all systems

## Contributing

When adding new features:

1. Add interface to `common.hpp`
2. Implement in platform-specific files
3. Add to main `BiosInfo` class
4. Update examples and documentation
5. Test on all supported platforms

## License

Copyright (C) 2023-2024 Max Qian <lightapt.com>
