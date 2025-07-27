# Enhanced Operating System Information Module

## Overview

The Enhanced Operating System Information Module provides comprehensive system analysis, monitoring, and management capabilities across Windows, Linux, and macOS platforms. This module has been completely redesigned and moved to the BIOS folder structure to provide advanced features while maintaining backward compatibility.

## Features

### 🔍 **Comprehensive System Information**
- Detailed OS detection and version information
- Hardware architecture and platform details
- Kernel and system build information
- Installation and update history
- System environment and configuration

### 📊 **Real-time Performance Monitoring**
- CPU, memory, and disk usage metrics
- Network performance monitoring
- Process and thread tracking
- System resource analysis
- Performance trend analysis

### 🔒 **Advanced Security Analysis**
- Firewall and antivirus status
- Security feature detection (SELinux, AppArmor, SIP)
- Vulnerability assessment
- Security event monitoring
- System integrity validation

### 🌐 **Network Configuration Management**
- Network interface detection
- IP and MAC address information
- DNS configuration
- Network adapter details
- Connection status monitoring

### ⚡ **System Optimization**
- Automated performance optimization
- Memory and disk cleanup
- Service management
- Startup program optimization
- System health recommendations

### 🔧 **Cross-platform Management**
- Service start/stop/restart operations
- System update management
- Firewall configuration
- Software installation tracking
- System backup and restore

## Architecture

```
bios/os/
├── common.hpp          # Common interfaces and structures
├── common.cpp          # Shared implementation and utilities
├── os.hpp             # Main OS class interface
├── os.cpp             # Main OS class implementation
├── windows.hpp        # Windows-specific interface
├── windows.cpp        # Windows WMI implementation
├── linux.hpp          # Linux-specific interface
├── linux.cpp          # Linux system calls implementation
├── macos.hpp          # macOS-specific interface
├── macos.cpp          # macOS IOKit/system_profiler implementation
├── examples/          # Usage examples and demos
├── tests/             # Unit tests and validation
└── CMakeLists.txt     # Build configuration
```

## Usage

### Basic Usage

```cpp
#include "atom/sysinfo/bios/os/os.hpp"

using namespace atom::system;

// Get enhanced OS information
auto& manager = EnhancedOSManager::getInstance();
auto osInfo = manager.getEnhancedOSInfo();

std::cout << "OS: " << osInfo.osName << " " << osInfo.osVersion << std::endl;
std::cout << "Architecture: " << osArchitectureToString(osInfo.osArch) << std::endl;
std::cout << "Uptime: " << osInfo.uptime.count() << " seconds" << std::endl;
```

### Performance Monitoring

```cpp
#include "atom/sysinfo/bios/os/os.hpp"

using namespace atom::system;

auto& manager = EnhancedOSManager::getInstance();

// Set up performance monitoring
SystemMonitoringConfig config;
config.performanceIntervalMs = 1000;
config.enablePerformanceMonitoring = true;

manager.setPerformanceCallback([](const SystemPerformanceMetrics& metrics) {
    std::cout << "CPU: " << metrics.cpuUsagePercent << "%" << std::endl;
    std::cout << "Memory: " << metrics.memoryUsagePercent << "%" << std::endl;
    std::cout << "Disk: " << metrics.diskUsagePercent << "%" << std::endl;
});

manager.startMonitoring(config);

// Monitor for 30 seconds
std::this_thread::sleep_for(std::chrono::seconds(30));

manager.stopMonitoring();
```

### Security Analysis

```cpp
#include "atom/sysinfo/bios/os/os.hpp"

using namespace atom::system;

auto& manager = EnhancedOSManager::getInstance();

// Get security information
auto secInfo = manager.getSecurityInfo();

std::cout << "Firewall: " << (secInfo.firewallEnabled ? "Enabled" : "Disabled") << std::endl;
std::cout << "Antivirus: " << (secInfo.antivirusEnabled ? "Enabled" : "Disabled") << std::endl;

// Perform security scan
auto scanResults = manager.performSecurityScan(true); // deep scan
for (const auto& result : scanResults) {
    std::cout << "Security finding: " << result << std::endl;
}
```

### System Health Check

```cpp
#include "atom/sysinfo/bios/os/os.hpp"

using namespace atom::system;

auto& manager = EnhancedOSManager::getInstance();

// Perform comprehensive health check
auto healthResults = manager.performHealthCheck();

std::cout << "System Health Report:" << std::endl;
for (const auto& finding : healthResults) {
    std::cout << "- " << finding << std::endl;
}

// Get resource usage analysis
auto resourceUsage = manager.analyzeResourceUsage();
for (const auto& [resource, usage] : resourceUsage) {
    std::cout << resource << ": " << usage << std::endl;
}
```

### System Optimization

```cpp
#include "atom/sysinfo/bios/os/os.hpp"

using namespace atom::system;

auto& manager = EnhancedOSManager::getInstance();

// Configure optimization
SystemOptimizationConfig config;
config.optimizeMemory = true;
config.optimizeDisk = true;
config.cleanTemporaryFiles = true;
config.optimizeServices = false; // Be careful with this

// Perform optimization
auto optimizationResults = manager.optimizeSystem(config);

std::cout << "Optimization Results:" << std::endl;
for (const auto& result : optimizationResults) {
    std::cout << "- " << result << std::endl;
}
```

### Platform-specific Features

#### Linux-specific

```cpp
#ifdef __linux__
#include "atom/sysinfo/bios/os/linux.hpp"

LinuxOSImplementation linuxImpl;

// Get distribution information
auto distInfo = linuxImpl.getDistributionInfo();
std::cout << "Distribution: " << distInfo.prettyName << std::endl;

// Get package information
auto packageInfo = linuxImpl.getPackageInfo();
std::cout << "Package Manager: " << packageInfo.packageManager << std::endl;
std::cout << "Installed Packages: " << packageInfo.totalPackages << std::endl;

// Get container information
auto containerInfo = linuxImpl.getContainerInfo();
if (containerInfo.isContainer) {
    std::cout << "Running in container: " << containerInfo.containerType << std::endl;
}
#endif
```

#### Windows-specific

```cpp
#ifdef _WIN32
#include "atom/sysinfo/bios/os/windows.hpp"

WindowsOSImplementation windowsImpl;

// Get Windows edition information
auto editionInfo = windowsImpl.getEditionInfo();
std::cout << "Edition: " << editionInfo.edition << std::endl;
std::cout << "Build: " << editionInfo.buildNumber << std::endl;

// Get Windows services
auto servicesInfo = windowsImpl.getServicesInfo();
std::cout << "Running Services: " << servicesInfo.runningServices.size() << std::endl;

// Get Windows updates
auto updateInfo = windowsImpl.getUpdateInfo();
std::cout << "Available Updates: " << updateInfo.availableUpdates.size() << std::endl;
#endif
```

#### macOS-specific

```cpp
#ifdef __APPLE__
#include "atom/sysinfo/bios/os/macos.hpp"

MacOSOSImplementation macosImpl;

// Get macOS version information
auto versionInfo = macosImpl.getVersionInfo();
std::cout << "macOS Version: " << versionInfo.productVersion << std::endl;
std::cout << "Build: " << versionInfo.productBuildVersion << std::endl;

// Get security information
auto secInfo = macosImpl.getSecurityInfo();
std::cout << "SIP: " << (secInfo.sipEnabled ? "Enabled" : "Disabled") << std::endl;
std::cout << "Gatekeeper: " << (secInfo.gatekeeperEnabled ? "Enabled" : "Disabled") << std::endl;

// Get application information
auto appInfo = macosImpl.getApplicationInfo();
std::cout << "Installed Applications: " << appInfo.installedApplications.size() << std::endl;
#endif
```

## Legacy Compatibility

The module maintains full backward compatibility with the original OS module API:

```cpp
#include "atom/sysinfo/os.hpp" // Original header still works

using namespace atom::system;

// Original functions still work
auto osInfo = getOperatingSystemInfo();
auto computerName = getComputerName();
auto uptime = getSystemUptime();

// New convenience functions
auto osName = getOSName();
auto osVersion = getOSVersion();
auto metrics = getSystemMetrics();
```

## Building

### Requirements

- C++20 compatible compiler
- CMake 3.15 or later
- spdlog library
- Platform-specific dependencies:
  - **Windows**: WMI libraries, Windows SDK
  - **Linux**: Standard system libraries
  - **macOS**: IOKit, CoreFoundation frameworks

### Build Instructions

```bash
# Configure
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel

# Install (optional)
cmake --install build --prefix /usr/local
```

### CMake Integration

```cmake
find_package(atom_sysinfo_bios_os REQUIRED)
target_link_libraries(your_target PRIVATE atom::atom_sysinfo_bios_os)
```

## Testing

```bash
# Run tests
cd build
ctest --verbose

# Run specific test
./tests/test_enhanced_os
```

## Contributing

1. Follow the existing code style and patterns
2. Add comprehensive tests for new features
3. Update documentation for API changes
4. Ensure cross-platform compatibility
5. Test on all supported platforms

## License

Copyright (C) 2023-2024 Max Qian <lightapt.com>

This project is licensed under the same terms as the parent Atom project.

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for detailed version history and changes.
