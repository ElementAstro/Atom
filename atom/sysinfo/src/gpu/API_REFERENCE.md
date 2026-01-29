# GPU Module API Reference

This document provides a comprehensive reference for the GPU module API.

## Core Functions

### Basic Information

#### `getGPUInfo() -> std::string`

Returns basic GPU information as a formatted string.

**Returns:** String containing GPU information
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPUCount() -> int`

Returns the number of GPUs detected in the system.

**Returns:** Number of GPUs (0 if none detected)
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getDetailedGPUInfo() -> std::vector<GPUInfo>`

Returns detailed information for all detected GPUs.

**Returns:** Vector of GPUInfo structures
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPUInfo(int gpuIndex) -> GPUInfo`

Returns detailed information for a specific GPU.

**Parameters:**

- `gpuIndex`: GPU index (0-based)

**Returns:** GPUInfo structure for the specified GPU
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

### Performance Monitoring

#### `getGPUPerformanceMetrics(int gpuIndex = 0) -> GPUPerformanceMetrics`

Returns current performance metrics for a specific GPU.

**Parameters:**

- `gpuIndex`: GPU index (0-based, defaults to 0)

**Returns:** GPUPerformanceMetrics structure
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPUMemoryInfo(int gpuIndex = 0) -> GPUMemoryInfo`

Returns memory information for a specific GPU.

**Parameters:**

- `gpuIndex`: GPU index (0-based, defaults to 0)

**Returns:** GPUMemoryInfo structure
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPUTemperature(int gpuIndex = 0) -> double`

Returns current temperature for a specific GPU.

**Parameters:**

- `gpuIndex`: GPU index (0-based, defaults to 0)

**Returns:** Temperature in Celsius
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPUUtilization(int gpuIndex = 0) -> double`

Returns current utilization percentage for a specific GPU.

**Parameters:**

- `gpuIndex`: GPU index (0-based, defaults to 0)

**Returns:** Utilization percentage (0-100)
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

### Monitor Information

#### `getAllMonitorsInfo() -> std::vector<MonitorInfo>`

Returns information for all connected monitors.

**Returns:** Vector of MonitorInfo structures
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

### Enhanced Functions

#### `benchmarkGPU(int gpuIndex, const GPUBenchmarkConfig& config) -> GPUBenchmarkResults`

Runs a comprehensive GPU benchmark.

**Parameters:**

- `gpuIndex`: GPU index (0-based)
- `config`: Benchmark configuration

**Returns:** GPUBenchmarkResults structure
**Thread Safety:** Not thread-safe (blocking operation)
**Platform Support:** Windows, Linux, macOS

#### `startGPUMonitoring(int gpuIndex, const GPUMonitoringConfig& config, callback) -> bool`

Starts continuous GPU monitoring with a callback function.

**Parameters:**

- `gpuIndex`: GPU index (0-based)
- `config`: Monitoring configuration
- `callback`: Function to call with performance updates

**Returns:** True if monitoring started successfully
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `stopGPUMonitoring(int gpuIndex) -> bool`

Stops GPU monitoring for a specific GPU.

**Parameters:**

- `gpuIndex`: GPU index (0-based)

**Returns:** True if monitoring stopped successfully
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getPrimaryGPU() -> GPUInfo`

Returns information for the primary GPU (usually the most powerful discrete GPU).

**Returns:** GPUInfo structure for the primary GPU
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPUsByVendor(GPUVendor vendor) -> std::vector<GPUInfo>`

Returns all GPUs from a specific vendor.

**Parameters:**

- `vendor`: GPU vendor to filter by

**Returns:** Vector of GPUInfo structures
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPUsByType(GPUType type) -> std::vector<GPUInfo>`

Returns all GPUs of a specific type.

**Parameters:**

- `type`: GPU type to filter by

**Returns:** Vector of GPUInfo structures
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

### Utility Functions

#### `getGPUPowerDraw(int gpuIndex) -> double`

Returns current power consumption for a specific GPU.

**Parameters:**

- `gpuIndex`: GPU index (0-based)

**Returns:** Power consumption in watts
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPUFanSpeed(int gpuIndex) -> double`

Returns current fan speed for a specific GPU.

**Parameters:**

- `gpuIndex`: GPU index (0-based)

**Returns:** Fan speed percentage (0-100)
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPUClockSpeeds(int gpuIndex) -> std::map<std::string, double>`

Returns current clock speeds for a specific GPU.

**Parameters:**

- `gpuIndex`: GPU index (0-based)

**Returns:** Map of clock type to speed in MHz
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `supportsGPUFeature(int gpuIndex, const std::string& feature) -> bool`

Checks if a GPU supports a specific feature.

**Parameters:**

- `gpuIndex`: GPU index (0-based)
- `feature`: Feature name to check

**Returns:** True if the feature is supported
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPURecommendedSettings(int gpuIndex) -> std::map<std::string, std::string>`

Returns recommended settings for optimal GPU performance.

**Parameters:**

- `gpuIndex`: GPU index (0-based)

**Returns:** Map of setting names to recommended values
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `isGPUThermalThrottling(int gpuIndex) -> bool`

Detects if a GPU is thermal throttling.

**Parameters:**

- `gpuIndex`: GPU index (0-based)

**Returns:** True if thermal throttling is detected
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPUPowerEfficiency(int gpuIndex) -> double`

Calculates GPU power efficiency.

**Parameters:**

- `gpuIndex`: GPU index (0-based)

**Returns:** Power efficiency score (performance per watt)
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPUHealthStatus(int gpuIndex) -> std::string`

Returns GPU health status.

**Parameters:**

- `gpuIndex`: GPU index (0-based)

**Returns:** Health status string
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPUErrorLog(int gpuIndex) -> std::vector<std::string>`

Returns GPU error log.

**Parameters:**

- `gpuIndex`: GPU index (0-based)

**Returns:** Vector of error messages
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPUPerformanceScore(int gpuIndex) -> double`

Calculates GPU performance score.

**Parameters:**

- `gpuIndex`: GPU index (0-based)

**Returns:** Performance score (0-100)
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

#### `getGPUSummary(int gpuIndex) -> std::string`

Returns formatted GPU summary information.

**Parameters:**

- `gpuIndex`: GPU index (0-based)

**Returns:** Formatted summary string
**Thread Safety:** Thread-safe
**Platform Support:** Windows, Linux, macOS

## Common Utility Functions

### String Formatting

#### `gpuVendorToString(GPUVendor vendor) -> std::string`

Converts GPU vendor enum to string.

#### `gpuTypeToString(GPUType type) -> std::string`

Converts GPU type enum to string.

#### `gpuArchitectureToString(GPUArchitecture arch) -> std::string`

Converts GPU architecture enum to string.

#### `formatMemorySize(size_t bytes) -> std::string`

Formats memory size to human-readable string.

#### `formatClockSpeed(double mhz) -> std::string`

Formats clock speed to human-readable string.

#### `formatTemperature(double celsius) -> std::string`

Formats temperature to string with unit.

#### `formatPower(double watts) -> std::string`

Formats power to string with unit.

### Analysis Functions

#### `calculateGPUScore(const GPUInfo& info) -> double`

Calculates GPU performance score from GPU information.

#### `validateGPUInfo(const GPUInfo& info) -> bool`

Validates GPU information structure.

#### `detectThermalThrottling(const GPUPerformanceMetrics& metrics) -> bool`

Detects thermal throttling from performance metrics.

#### `calculatePowerEfficiency(const GPUPerformanceMetrics& metrics) -> double`

Calculates power efficiency from performance metrics.

## Error Handling

All functions handle errors gracefully:

- Invalid GPU indices return empty/default structures
- Platform-specific errors are logged via spdlog
- Functions return appropriate default values on failure
- No exceptions are thrown from the API functions

## Thread Safety

- All read operations are thread-safe
- Monitoring functions use proper synchronization
- Multiple threads can safely call different functions simultaneously
- Monitoring callbacks are executed in separate threads
