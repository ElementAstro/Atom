# Disk System Information Components

This directory contains a comprehensive set of C++ components for disk and storage device management, monitoring, and analysis. The components provide advanced features including real-time monitoring, predictive analytics, security scanning, and performance optimization.

## Components Overview

### Core Components

- **`disk_types.hpp`** - Modern C++ data structures and types for disk information
- **`disk_util.hpp/cpp`** - Utility functions with performance optimizations and caching
- **`disk_info.hpp/cpp`** - Enhanced disk information retrieval with async operations
- **`disk_device.hpp/cpp`** - Advanced storage device detection and management
- **`disk_monitor.hpp/cpp`** - Real-time event-driven monitoring system
- **`disk_security.hpp/cpp`** - Advanced security features with ML-based threat detection
- **`disk_performance.hpp/cpp`** - Comprehensive performance monitoring and benchmarking
- **`disk_analytics.hpp/cpp`** - Predictive analytics and maintenance planning

### Key Features

#### 🚀 Performance Optimizations
- **Async Operations**: Non-blocking disk operations with futures
- **Intelligent Caching**: Multi-level caching with automatic expiration
- **SIMD Optimizations**: Vectorized calculations for batch operations
- **Lock-free Data Structures**: High-performance concurrent access

#### 📊 Advanced Monitoring
- **Event-driven Architecture**: Real-time device change notifications
- **Configurable Intervals**: Adaptive monitoring frequencies
- **Performance Metrics**: IOPS, throughput, latency, and health tracking
- **Anomaly Detection**: Statistical analysis for performance anomalies

#### 🔒 Security Features
- **Device Fingerprinting**: Unique device identification and verification
- **Encryption Detection**: Support for LUKS, BitLocker, and other encryption
- **Threat Scanning**: ML-based malware and threat detection
- **Access Control**: Whitelist management and security policies

#### 🔮 Predictive Analytics
- **Usage Pattern Analysis**: Temporal and behavioral pattern recognition
- **Capacity Planning**: Growth prediction and expansion recommendations
- **Health Prediction**: Failure prediction with confidence intervals
- **Maintenance Scheduling**: Automated maintenance planning

## Quick Start

### Basic Usage

```cpp
#include "atom/sysinfo/disk/disk_info.hpp"
#include "atom/sysinfo/disk/disk_device.hpp"

using namespace atom::system;

// Get all disk information
auto diskInfos = getDiskInfo(true);
for (const auto& info : diskInfos) {
    std::cout << "Device: " << info.devicePath 
              << ", Usage: " << info.usagePercent << "%" << std::endl;
}

// Get storage devices
auto devices = getStorageDevices(true);
for (const auto& device : devices) {
    std::cout << "Device: " << device.devicePath 
              << ", Size: " << device.getFormattedSize() << std::endl;
}
```

### Advanced Monitoring

```cpp
#include "atom/sysinfo/disk/disk_monitor.hpp"

// Create and configure monitor
DiskMonitor monitor;
monitor.startMonitoring([](const DiskMonitorEventData& event) {
    std::cout << "Event: " << static_cast<int>(event.eventType) 
              << " for device: " << event.devicePath << std::endl;
});

// Add devices to monitor
monitor.addDevice("/dev/sda");
monitor.addDevice("/dev/nvme0n1");

// Monitor will run until stopped
std::this_thread::sleep_for(std::chrono::minutes(5));
monitor.stopMonitoring();
```

### Performance Analysis

```cpp
#include "atom/sysinfo/disk/disk_performance.hpp"

// Get current performance metrics
auto metrics = getCurrentPerformanceMetrics("/dev/sda");
if (metrics) {
    std::cout << "IOPS: " << metrics->totalIOPS << std::endl;
    std::cout << "Throughput: " << metrics->totalThroughputMBps << " MB/s" << std::endl;
    std::cout << "Health Score: " << metrics->healthScore << std::endl;
}

// Perform benchmark
BenchmarkConfig config;
config.testSizeMB = 100;
config.testDurationSeconds = 30;

auto benchmarkResult = performBenchmark("/dev/sda", config);
std::cout << "Benchmark IOPS: " << benchmarkResult.totalIOPS << std::endl;
```

### Security Analysis

```cpp
#include "atom/sysinfo/disk/disk_security.hpp"

// Perform security audit
auto auditResult = performSecurityAudit("/dev/sda");
std::cout << "Encryption Status: " << static_cast<int>(auditResult.encryptionStatus) << std::endl;
std::cout << "Security Issues: " << auditResult.securityIssues.size() << std::endl;

// Advanced threat scanning
SecurityScanConfig scanConfig;
scanConfig.enableMLPatterns = true;
scanConfig.enableDeepScan = true;

auto scanResult = scanForThreatsAdvanced("/mnt/usb", scanConfig);
std::cout << "Threats Found: " << scanResult.suspiciousFiles << std::endl;
std::cout << "Max Threat Level: " << static_cast<int>(scanResult.maxThreatLevel) << std::endl;
```

### Predictive Analytics

```cpp
#include "atom/sysinfo/disk/disk_analytics.hpp"

// Analyze usage patterns
auto usagePattern = analyzeUsagePatterns("/dev/sda", 30);
std::cout << "Daily Growth: " << usagePattern.avgDailyGrowthMB << " MB/day" << std::endl;
std::cout << "Days Until Full: " << usagePattern.daysUntilFull << std::endl;

// Capacity planning
auto capacityPlan = performCapacityPlanning("/dev/sda", 12);
if (capacityPlan.needsExpansion) {
    std::cout << "Expansion needed: " << capacityPlan.recommendedExpansionGB << " GB" << std::endl;
}

// Generate maintenance alerts
auto alerts = generateMaintenanceAlerts("/dev/sda");
for (const auto& alert : alerts) {
    std::cout << "Alert: " << alert.message 
              << " (Confidence: " << alert.confidence << ")" << std::endl;
}
```

## Manager Classes

For complex applications, use the manager classes that provide higher-level abstractions:

### DiskInfoManager
```cpp
DiskInfoManager infoManager;
auto diskInfos = infoManager.getDiskInfo(true);
auto stats = infoManager.getCacheStats();
```

### DeviceManager
```cpp
DeviceManager deviceManager;
deviceManager.startMonitoring(
    [](const StorageDevice& device) { /* device added */ },
    [](const std::string& path) { /* device removed */ }
);
```

### PerformanceManager
```cpp
PerformanceManager perfManager;
perfManager.addDevice("/dev/sda");
perfManager.startMonitoring([](const std::string& path, const ExtendedPerformanceMetrics& metrics) {
    // Handle performance updates
});
```

### SecurityManager
```cpp
SecurityManager secManager;
secManager.addDeviceToWhitelist("/dev/sda");
secManager.startSecurityMonitoring([](const SecurityAuditResult& result) {
    // Handle security events
});
```

### DiskAnalytics
```cpp
DiskAnalytics analytics;
analytics.addDevice("/dev/sda");
analytics.startMonitoring([](const MaintenanceAlert& alert) {
    // Handle maintenance alerts
});
```

## Configuration

### Monitoring Configuration
```cpp
AdvancedMonitorConfig config;
config.pollInterval = std::chrono::seconds(1);
config.enableEventDriven = true;
config.enablePerformanceMonitoring = true;
config.enableHealthMonitoring = true;
config.temperatureThreshold = 70.0f;
config.spaceWarningThreshold = 90.0f;
```

### Performance Configuration
```cpp
PerformanceMonitorConfig config;
config.sampleInterval = std::chrono::seconds(2);
config.historySize = 3600;
config.enablePredictiveAnalysis = true;
config.enableAnomalyDetection = true;
config.anomalyThreshold = 2.5f;
```

### Security Configuration
```cpp
SecurityScanConfig config;
config.enableMLPatterns = true;
config.enableBehaviorAnalysis = true;
config.enableDeepScan = false;
config.quarantineThreats = true;
config.quarantineThreshold = ThreatLevel::MEDIUM;
```

### Analytics Configuration
```cpp
AnalyticsConfig config;
config.analysisWindow = std::chrono::hours(24 * 7);
config.minSamplesRequired = 100;
config.anomalyThreshold = 2.5f;
config.enablePredictiveAnalysis = true;
config.enableSeasonalDetection = true;
config.capacityWarningThreshold = 80.0f;
```

## Performance Guidelines

### Best Practices

1. **Use Async Operations**: For non-blocking operations, prefer async versions
2. **Enable Caching**: Use cached versions for frequently accessed data
3. **Batch Operations**: Use batch functions for processing multiple items
4. **Configure Intervals**: Adjust monitoring intervals based on requirements
5. **Resource Management**: Use RAII and smart pointers for automatic cleanup

### Memory Optimization

- Use `reserve()` for vectors when size is known
- Prefer move semantics for large objects
- Use const references to avoid unnecessary copies
- Clear unused caches periodically

### Threading Considerations

- All manager classes are thread-safe
- Use separate instances for different threads when possible
- Be aware of shared cache state in utility functions
- Consider using thread-local storage for high-frequency operations

## Error Handling

All functions use modern C++ error handling patterns:

- **Optional Returns**: Functions return `std::optional<T>` for nullable results
- **Result Types**: Complex operations return `DiskOperationResult<T>`
- **Exception Safety**: All operations are exception-safe with RAII
- **Graceful Degradation**: Functions handle missing permissions gracefully

## Platform Support

- **Linux**: Full support with advanced features
- **Windows**: Basic support (some features limited)
- **macOS**: Basic support (some features limited)

## Dependencies

- **C++20**: Modern C++ features (concepts, ranges, coroutines)
- **spdlog**: Logging framework
- **Standard Library**: chrono, filesystem, thread, future, etc.

## Testing

Comprehensive test suite included:
- Unit tests (`disk_components_test.cpp`)
- Performance benchmarks (`disk_performance_test.cpp`)
- Integration tests (`disk_integration_test.cpp`)
- Stress tests (`disk_stress_test.cpp`)

Run tests with:
```bash
make test_disk_all
```

## Contributing

When contributing to these components:

1. Follow the existing code style and patterns
2. Add comprehensive tests for new features
3. Update documentation for API changes
4. Consider performance implications
5. Ensure thread safety for shared resources

## License

Copyright (C) 2023-2024 Max Qian <lightapt.com>

See the main project license for details.
