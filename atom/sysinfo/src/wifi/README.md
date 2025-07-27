# WiFi System Information Module

A comprehensive, cross-platform C++ library for WiFi and network information gathering, monitoring, and quality analysis.

## Features

### Core Functionality
- **Internet Connectivity Detection**: Check if the system is connected to the internet
- **WiFi Information**: Get current WiFi network name and connection status
- **Network Interfaces**: Enumerate and analyze network interfaces
- **IP Address Management**: Retrieve IPv4 and IPv6 addresses
- **Network Statistics**: Real-time network performance metrics

### Advanced Features
- **Network Scanning**: Discover available WiFi networks
- **Quality Analysis**: Comprehensive network quality assessment including jitter, packet loss, and throughput
- **Real-time Monitoring**: Continuous network performance monitoring with event callbacks
- **Bandwidth Measurement**: Measure actual network throughput
- **Device Discovery**: Find connected devices on the network
- **Security Analysis**: Analyze network security configurations

### Performance & Reliability
- **Intelligent Caching**: Multi-level caching system with configurable TTL
- **Error Handling**: Comprehensive error reporting and recovery
- **Performance Monitoring**: Built-in performance metrics and optimization
- **Configuration Management**: Flexible configuration system with auto-tuning
- **Cross-platform Support**: Windows, Linux, and macOS compatibility

## Architecture

The module is organized into several specialized components:

- **`wifi.hpp/cpp`**: Core WiFi functionality and main API
- **`common.hpp/cpp`**: Cross-platform utilities and helper functions
- **`monitor.hpp/cpp`**: Real-time network monitoring system
- **`quality.hpp/cpp`**: Network quality analysis and assessment
- **`config.hpp/cpp`**: Configuration management and performance tuning
- **`error_handler.hpp/cpp`**: Comprehensive error handling and logging
- **Platform-specific implementations**: `linux.hpp/cpp`, `windows.hpp/cpp`, `macos.hpp/cpp`

## Quick Start

### Basic Usage

```cpp
#include "atom/sysinfo/wifi/wifi.hpp"
using namespace atom::system;

// Check internet connectivity
bool connected = isConnectedToInternet();

// Get current WiFi network
std::string wifi_name = getCurrentWifi();

// Get network statistics
NetworkStats stats = getNetworkStats();
std::cout << "Latency: " << stats.latency << "ms\n";
std::cout << "Download: " << stats.downloadSpeed << "MB/s\n";

// Scan for available networks
auto networks = scanAvailableNetworks();
for (const auto& network : networks) {
    std::cout << "Network: " << network << "\n";
}
```

### Advanced Monitoring

```cpp
#include "atom/sysinfo/wifi/monitor.hpp"

// Initialize monitoring
initializeNetworkMonitoring();

auto& monitor = getGlobalNetworkMonitor();

// Register event callback
monitor.registerEventCallback([](const NetworkEventData& event) {
    std::cout << "Network event: " << event.description << "\n";
});

// Start monitoring
monitor.start();

// Get historical data
auto history = monitor.getStatsHistory(std::chrono::minutes(30));
auto avg_stats = monitor.getAverageStats(std::chrono::minutes(10));
```

### Quality Analysis

```cpp
#include "atom/sysinfo/wifi/quality.hpp"

NetworkQualityAnalyzer analyzer;

// Perform comprehensive analysis
auto metrics = analyzer.performComprehensiveAnalysis();

std::cout << "Overall Quality: " << (metrics.overall_quality * 100) << "%\n";
std::cout << "Jitter: " << metrics.jitter_ms << "ms\n";
std::cout << "Packet Loss: " << metrics.packet_loss_percentage << "%\n";

// Get quality level and recommendations
auto level = NetworkQualityAnalyzer::getQualityLevel(metrics);
auto recommendations = NetworkQualityAnalyzer::getQualityRecommendations(metrics);
```

### Configuration Management

```cpp
#include "atom/sysinfo/wifi/config.hpp"

// Initialize configuration
initializeConfiguration("config.ini");

auto& config = getGlobalConfigManager();

// Modify settings
config.setParameter("ping_timeout", "5000");
config.setParameter("max_cache_size", "200");

// Auto-tune for optimal performance
config.autoTune();
```

## Building

### Requirements
- C++20 compatible compiler
- CMake 3.16 or later
- Platform-specific dependencies:
  - **Windows**: Windows SDK, WinSock2
  - **Linux**: Standard system libraries
  - **macOS**: CoreFoundation, SystemConfiguration frameworks

### Build Instructions

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run tests (optional)
cmake --build build --target wifi_tests

# Run demo (optional)
cmake --build build --target run_wifi_demo
```

### CMake Options

- `BUILD_WIFI_TESTS=ON/OFF`: Build test suite (default: ON)
- `BUILD_WIFI_EXAMPLES=ON/OFF`: Build example programs (default: ON)
- `INSTALL_TESTS=ON/OFF`: Install test executables (default: OFF)
- `INSTALL_EXAMPLES=ON/OFF`: Install example programs (default: OFF)

## Testing

The module includes comprehensive test suites:

```bash
# Run all tests
./build/tests/wifi_tests

# Run specific test categories
./build/tests/wifi_basic_tests
./build/tests/wifi_advanced_tests

# Run with memory checking (if valgrind available)
cmake --build build --target run_wifi_memory_tests

# Generate coverage report (if gcov/lcov available)
cmake --build build --target wifi_coverage
```

## Configuration

The module supports configuration through:

1. **Configuration files** (INI format)
2. **Environment variables**
3. **Runtime API calls**
4. **Auto-tuning** based on system capabilities

### Example Configuration

```ini
# Timeout settings (milliseconds)
ping_timeout=5000
command_timeout=10000

# Cache settings
max_cache_size=100
cache_ttl_wifi_info=15

# Performance settings
max_concurrent_operations=4
enable_background_monitoring=true

# Host settings
preferred_ping_host=8.8.8.8
backup_ping_host=1.1.1.1
```

## Performance Optimization

The module includes several performance optimizations:

- **Intelligent Caching**: Reduces redundant system calls
- **Parallel Processing**: Concurrent operations where beneficial
- **Memory Pooling**: Efficient memory management
- **Adaptive Algorithms**: Self-tuning based on system performance
- **Performance Monitoring**: Built-in metrics collection

## Error Handling

Comprehensive error handling with:

- **Structured Error Codes**: Specific error types for different failure modes
- **Error Callbacks**: Register custom error handlers
- **Error Statistics**: Track error rates and patterns
- **Recovery Mechanisms**: Automatic retry and fallback strategies
- **Detailed Logging**: Integration with the project's logging system

## Platform Support

### Windows
- Uses WinSock2 and Windows API for network operations
- Supports Windows 10 and later
- Requires appropriate permissions for network scanning

### Linux
- Uses standard Linux networking APIs
- Supports most modern Linux distributions
- May require root privileges for some advanced features

### macOS
- Uses CoreFoundation and SystemConfiguration frameworks
- Supports macOS 10.15 and later
- Requires appropriate entitlements for network access

## API Reference

### Core Functions
- `isConnectedToInternet()`: Check internet connectivity
- `getCurrentWifi()`: Get current WiFi network name
- `getNetworkStats()`: Get real-time network statistics
- `scanAvailableNetworks()`: Discover available networks
- `measureBandwidth()`: Measure network throughput

### Monitoring Functions
- `initializeNetworkMonitoring()`: Initialize monitoring system
- `getGlobalNetworkMonitor()`: Get global monitor instance
- `NetworkMonitor::start()/stop()`: Control monitoring

### Quality Analysis
- `NetworkQualityAnalyzer::performComprehensiveAnalysis()`: Full quality assessment
- `NetworkQualityAnalyzer::measureJitter()`: Measure network jitter
- `NetworkQualityAnalyzer::measurePacketLoss()`: Measure packet loss

### Configuration
- `initializeConfiguration()`: Initialize configuration system
- `getGlobalConfigManager()`: Get configuration manager
- `WiFiConfigManager::setParameter()`: Set configuration values

## Contributing

1. Follow the existing code style and conventions
2. Add comprehensive tests for new features
3. Update documentation for API changes
4. Ensure cross-platform compatibility
5. Run the full test suite before submitting

## License

Copyright (C) 2023-2024 Max Qian <lightapt.com>

This project is part of the Atom system information library.
