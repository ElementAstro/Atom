# GPU Module

The GPU module provides comprehensive GPU information retrieval and monitoring capabilities across Windows, Linux, and macOS platforms. This module has been redesigned with a modular architecture for better maintainability and enhanced functionality.

## Features

### Core Functionality
- **Multi-GPU Support**: Detect and monitor multiple GPUs in the system
- **Cross-Platform**: Windows, Linux, and macOS support
- **Detailed Information**: Comprehensive GPU specifications and capabilities
- **Real-time Monitoring**: Performance metrics, temperature, and power consumption
- **Driver Information**: Version, features, and API support details

### Enhanced Features
- **Performance Benchmarking**: Built-in GPU performance testing
- **Thermal Monitoring**: Temperature tracking with alert thresholds
- **Power Management**: Power consumption monitoring and efficiency calculations
- **Memory Analytics**: VRAM usage tracking and analysis
- **Health Monitoring**: GPU health status and error detection
- **Vendor-Specific APIs**: Support for NVIDIA, AMD, and Intel specific features

## Architecture

The module is organized into several components:

```
gpu/
├── gpu.hpp          # Main API definitions and structures
├── gpu.cpp          # Main implementation with platform delegation
├── common.hpp       # Shared utilities and helper functions
├── common.cpp       # Common implementation
├── windows.hpp      # Windows-specific declarations
├── windows.cpp      # Windows implementation (WMI, DirectX, etc.)
├── linux.hpp        # Linux-specific declarations
├── linux.cpp        # Linux implementation (sysfs, DRM, etc.)
├── macos.hpp        # macOS-specific declarations
├── macos.cpp        # macOS implementation (IOKit, Metal, etc.)
└── CMakeLists.txt   # Build configuration
```

## API Overview

### Basic GPU Information

```cpp
#include "atom/sysinfo/gpu/gpu.hpp"

// Get basic GPU information string
std::string gpuInfo = atom::system::getGPUInfo();

// Get number of GPUs
int gpuCount = atom::system::getGPUCount();

// Get detailed information for all GPUs
auto gpus = atom::system::getDetailedGPUInfo();

// Get specific GPU information
auto gpu = atom::system::getGPUInfo(0);  // First GPU
```

### Performance Monitoring

```cpp
// Get performance metrics
auto metrics = atom::system::getGPUPerformanceMetrics(0);
std::cout << "GPU Utilization: " << metrics.gpuUtilization << "%" << std::endl;
std::cout << "Temperature: " << metrics.temperature << "°C" << std::endl;
std::cout << "Power Draw: " << metrics.powerDraw << "W" << std::endl;

// Get memory information
auto memInfo = atom::system::getGPUMemoryInfo(0);
std::cout << "Total VRAM: " << memInfo.totalMemory / (1024*1024*1024) << " GB" << std::endl;
std::cout << "Used VRAM: " << memInfo.usedMemory / (1024*1024*1024) << " GB" << std::endl;
```

### Continuous Monitoring

```cpp
// Set up monitoring configuration
atom::system::GPUMonitoringConfig config;
config.updateInterval = std::chrono::milliseconds(1000);
config.temperatureThreshold = 80.0;  // Alert at 80°C
config.memoryThreshold = 90.0;       // Alert at 90% memory usage

// Start monitoring with callback
atom::system::startGPUMonitoring(0, config, [](const auto& metrics) {
    std::cout << "GPU Temp: " << metrics.temperature << "°C, "
              << "Util: " << metrics.gpuUtilization << "%" << std::endl;
});

// Stop monitoring when done
atom::system::stopGPUMonitoring(0);
```

### GPU Benchmarking

```cpp
// Configure benchmark
atom::system::GPUBenchmarkConfig benchConfig;
benchConfig.duration = std::chrono::seconds(30);
benchConfig.testCompute = true;
benchConfig.testMemory = true;
benchConfig.testGraphics = true;

// Run benchmark
auto results = atom::system::benchmarkGPU(0, benchConfig);
std::cout << "Overall Score: " << results.overallScore << std::endl;
std::cout << "Memory Bandwidth: " << results.memoryBandwidth << " GB/s" << std::endl;
```

### Monitor Information

```cpp
// Get all connected monitors
auto monitors = atom::system::getAllMonitorsInfo();

for (const auto& monitor : monitors) {
    std::cout << "Monitor: " << monitor.model 
              << " (" << monitor.width << "x" << monitor.height 
              << " @ " << monitor.refreshRate << "Hz)" << std::endl;
}
```

## Data Structures

### GPUInfo
Comprehensive GPU information including:
- Basic identification (name, vendor, device ID)
- Hardware specifications (memory, architecture, type)
- Performance capabilities (compute units, clock speeds)
- Driver information (version, API support)

### GPUPerformanceMetrics
Real-time performance data:
- Utilization percentages (GPU, memory, encoder, decoder)
- Clock speeds (core, memory, base, boost)
- Thermal information (temperature, hotspot, memory temp)
- Power consumption (current draw, limits, efficiency)
- Fan information (speed percentage, RPM)

### GPUMemoryInfo
Memory subsystem details:
- Capacity information (total, free, used, shared)
- Memory specifications (type, bus width, clock speed)
- Bandwidth and usage statistics

### MonitorInfo
Display information:
- Basic specs (resolution, refresh rate, size)
- Color capabilities (color space, bit depth, HDR)
- Connection details (type, connected GPU)

## Platform-Specific Features

### Windows
- WMI integration for comprehensive GPU information
- DirectX feature detection and capabilities
- Performance counters for real-time metrics
- NVIDIA/AMD vendor-specific APIs when available

### Linux
- sysfs integration for hardware information
- DRM subsystem support for display information
- X11/Xrandr for monitor detection
- Vendor-specific driver interfaces (nvidia-ml, amdgpu)

### macOS
- IOKit integration for hardware enumeration
- Metal framework support for GPU capabilities
- Core Graphics for display management
- Apple Silicon specific features (Neural Engine, Media Engine)

## Building

The GPU module is built as part of the main sysinfo library. To build:

```bash
mkdir build && cd build
cmake ..
make atom_sysinfo_gpu
```

### Dependencies

**All Platforms:**
- C++17 compatible compiler
- CMake 3.15+
- spdlog for logging

**Windows:**
- Windows SDK
- WMI libraries (wbemuuid, ole32, oleaut32)
- SetupAPI

**Linux:**
- X11 development libraries
- Xrandr extension (optional)
- DRM libraries (optional)

**macOS:**
- Xcode command line tools
- IOKit framework
- CoreGraphics framework
- Metal framework

## Error Handling

The module uses comprehensive error handling:
- Invalid GPU indices return empty/default structures
- Platform-specific errors are logged via spdlog
- Graceful degradation when optional features are unavailable
- Validation functions for data integrity

## Thread Safety

- All read operations are thread-safe
- Monitoring uses separate threads with proper synchronization
- Platform-specific implementations handle concurrent access appropriately

## Performance Considerations

- Caching of static information to reduce API calls
- Efficient polling intervals for monitoring
- Minimal overhead for basic information queries
- Optional features can be disabled to reduce resource usage

## Future Enhancements

- GPU overclocking support
- Advanced power management controls
- Machine learning workload optimization
- Enhanced vendor-specific feature support
- GPU virtualization detection
- Multi-GPU load balancing recommendations

## License

Copyright (C) 2023-2024 Max Qian <lightapt.com>

This module is part of the Atom system information library.
