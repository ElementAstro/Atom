# Atom Sysinfo Module Examples

This directory contains comprehensive examples demonstrating the advanced system information gathering capabilities of the Atom framework.

## 🚀 Overview

The Atom sysinfo module provides comprehensive system information gathering including:

- **Operating System**: Complete OS details, version, architecture, and configuration
- **Hardware Information**: CPU, memory, storage, GPU, BIOS, and battery details
- **System Monitoring**: Real-time system metrics, performance data, and health monitoring
- **Network Analysis**: WiFi scanning, connectivity testing, and performance metrics
- **Cross-Platform**: Unified API across Windows, Linux, macOS, and FreeBSD
- **Advanced Features**: Virtual environment detection, security analysis, and comprehensive reporting

## 📁 Examples

### ✅ **Header Test**

**File**: `header_test.cpp`
**Status**: Fully functional ✅

A simple test that verifies the sysinfo module headers can be included successfully.

#### **Features Demonstrated**

- Header inclusion verification
- Basic module structure validation
- Compilation compatibility testing

### ✅ **Enhanced Basic Sysinfo Example**

**File**: `basic_sysinfo_example.cpp`
**Status**: Enhanced with comprehensive error handling ✅

Demonstrates enhanced basic system information gathering with robust error handling.

#### **Features Demonstrated**

- Enhanced operating system information retrieval
- Advanced system uptime analysis and formatting
- Comprehensive memory usage monitoring with health assessment
- System language, encoding, and locale detection
- WSL (Windows Subsystem for Linux) detection
- Intelligent system health monitoring and recommendations
- Performance metrics and timing analysis
- Cross-platform compatibility with graceful error handling

### ✅ **Comprehensive OS Information Example**

**File**: `comprehensive_os_example.cpp`
**Status**: Complete OS information demonstration ✅

Exhaustive demonstration of all operating system information capabilities.

#### **Features Demonstrated**

- Complete operating system information retrieval
- Advanced uptime and boot time analysis
- System language, locale, and encoding detection
- Virtual environment detection (WSL, containers, VMs)
- System update and patch information
- Performance monitoring and timing analysis
- Cross-platform compatibility testing
- Advanced error handling and diagnostics

### ✅ **CPU Information and Monitoring Example**

**File**: `cpu_monitoring_example.cpp`
**Status**: Complete CPU monitoring demonstration ✅

Comprehensive demonstration of CPU information gathering and real-time monitoring.

#### **Features Demonstrated**

- Complete CPU information retrieval (model, vendor, architecture)
- CPU core and thread information
- CPU frequency and performance monitoring
- CPU cache information and analysis
- CPU temperature monitoring and thermal management
- CPU load average and usage tracking
- CPU power consumption monitoring
- CPU feature flags and instruction set detection
- Real-time CPU performance monitoring

### ✅ **Memory Information and Monitoring Example**

**File**: `memory_monitoring_example.cpp`
**Status**: Complete memory monitoring demonstration ✅

Comprehensive demonstration of memory information gathering and performance monitoring.

#### **Features Demonstrated**

- Complete memory information retrieval (physical, virtual, swap)
- Memory usage monitoring and analysis
- Memory performance metrics and bandwidth monitoring
- Memory health assessment and warnings
- Physical memory module information
- Virtual memory management analysis
- Memory pressure detection and recommendations
- Real-time memory monitoring capabilities

### ✅ **Disk/Storage Information and Monitoring Example**

**File**: `disk_monitoring_example.cpp`
**Status**: Complete disk monitoring demonstration ✅

Comprehensive demonstration of disk and storage information gathering and monitoring.

#### **Features Demonstrated**

- Complete disk information retrieval (all mounted drives)
- Storage device enumeration and identification
- Disk usage monitoring and analysis
- Storage device model and specification detection
- Removable storage detection and handling
- File system type identification
- Storage health assessment and warnings
- Cross-platform storage information gathering

### ✅ **Network and WiFi Analysis Example**

**File**: `network_analysis_example.cpp`
**Status**: Complete network analysis demonstration ✅

Comprehensive demonstration of network and WiFi information gathering and analysis.

#### **Features Demonstrated**

- Network statistics and performance monitoring
- WiFi network scanning and analysis
- Network connectivity testing and validation
- Bandwidth measurement and analysis
- Network security information gathering
- Connected device enumeration
- Network quality assessment
- Signal strength monitoring
- Latency and packet loss analysis

### ✅ **Comprehensive System Report Generator**

**File**: `system_report_generator.cpp`
**Status**: Complete system reporting demonstration ✅

Demonstrates comprehensive system report generation using SystemInfoPrinter utility.

#### **Features Demonstrated**

- Complete system report generation using SystemInfoPrinter
- Individual component report formatting
- Custom report generation with selective information
- Report export to files (text, HTML formats)
- Performance timing and analysis
- Error handling and graceful degradation
- Cross-platform system reporting

### 🔧 **Legacy System Info Example**

**File**: `system_info_example.cpp`
**Status**: Legacy example - use newer examples above ⚠️

Original comprehensive demonstration of system information capabilities.

#### **Features Demonstrated**

- Detailed CPU information and monitoring
- Memory statistics and performance metrics
- Disk and storage information
- Network and WiFi information
- Hardware detection and enumeration

## 🛠️ Building and Running

### Build the Examples

```bash
# Configure CMake with examples enabled
cmake -B build -S . -DATOM_EXAMPLE_BUILD_ALL=ON

# Build all sysinfo examples
cmake --build build --target sysinfo_header_test
cmake --build build --target sysinfo_basic_sysinfo_example
cmake --build build --target sysinfo_comprehensive_os_example
cmake --build build --target sysinfo_cpu_monitoring_example
cmake --build build --target sysinfo_memory_monitoring_example
cmake --build build --target sysinfo_disk_monitoring_example
cmake --build build --target sysinfo_network_analysis_example
cmake --build build --target sysinfo_system_report_generator
cmake --build build --target sysinfo_system_info_example
```

### Run the Examples

#### **Recommended Examples (Enhanced)**

```bash
# Header compatibility test
./build/example/sysinfo/sysinfo_header_test.exe

# Enhanced basic system information with error handling
./build/example/sysinfo/sysinfo_basic_sysinfo_example.exe

# Comprehensive OS information demonstration
./build/example/sysinfo/sysinfo_comprehensive_os_example.exe

# CPU information and monitoring
./build/example/sysinfo/sysinfo_cpu_monitoring_example.exe

# Memory information and monitoring
./build/example/sysinfo/sysinfo_memory_monitoring_example.exe

# Disk/storage information and monitoring
./build/example/sysinfo/sysinfo_disk_monitoring_example.exe

# Network and WiFi analysis
./build/example/sysinfo/sysinfo_network_analysis_example.exe

# Comprehensive system report generator
./build/example/sysinfo/sysinfo_system_report_generator.exe
```

#### **Legacy Examples**

```bash
# Original system info example (may have runtime issues)
./build/example/sysinfo/sysinfo_system_info_example.exe
```

### Example Execution Order

For the best learning experience, run the examples in this order:

1. **Header Test** - Verify compilation compatibility
2. **Enhanced Basic Example** - Learn basic system information gathering
3. **Comprehensive OS Example** - Explore detailed OS information
4. **CPU Monitoring Example** - Understand CPU information and monitoring
5. **Memory Monitoring Example** - Learn memory analysis and monitoring
6. **Disk Monitoring Example** - Explore storage information and analysis
7. **Network Analysis Example** - Understand network and WiFi capabilities
8. **System Report Generator** - See comprehensive reporting capabilities

## 🎯 Key Features and Usage Examples

### **1. Enhanced Operating System Information**

```cpp
// Get comprehensive OS information with error handling
try {
    auto osInfo = getOperatingSystemInfo();
    std::cout << "OS: " << osInfo.osName << " " << osInfo.osVersion << std::endl;
    std::cout << "Architecture: " << osInfo.architecture << std::endl;
    std::cout << "Kernel: " << osInfo.kernelVersion << std::endl;
    std::cout << "Computer: " << osInfo.computerName << std::endl;
    std::cout << "Time Zone: " << osInfo.timeZone << std::endl;

    // WSL detection
    if (isWsl()) {
        std::cout << "Running in WSL: Yes" << std::endl;
    }
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
```

### **2. Advanced System Uptime and Status**

```cpp
// Get system uptime with detailed formatting
auto uptime = getSystemUptime();
auto uptimeSeconds = uptime.count();
auto days = uptimeSeconds / (24 * 3600);
auto hours = (uptimeSeconds % (24 * 3600)) / 3600;
auto minutes = (uptimeSeconds % 3600) / 60;

std::cout << "Uptime: " << days << " days, " << hours << " hours, "
          << minutes << " minutes" << std::endl;

// System language and encoding with error handling
try {
    std::cout << "Language: " << getSystemLanguage() << std::endl;
    std::cout << "Encoding: " << getSystemEncoding() << std::endl;
} catch (const std::exception& e) {
    std::cerr << "Locale error: " << e.what() << std::endl;
}
```

### **3. Comprehensive Memory Information**

```cpp
// Get memory usage with health assessment
auto memoryUsage = getMemoryUsage();
std::cout << "Memory usage: " << std::fixed << std::setprecision(1)
          << memoryUsage << "%" << std::endl;

// Health assessment
if (memoryUsage > 90.0) {
    std::cout << "⚠️ WARNING: Critical memory usage!" << std::endl;
} else if (memoryUsage > 80.0) {
    std::cout << "⚠️ CAUTION: High memory usage" << std::endl;
} else {
    std::cout << "✓ Memory usage is healthy" << std::endl;
}

// Get detailed memory statistics
auto memInfo = getDetailedMemoryStats();
std::cout << "Total RAM: " << (memInfo.totalPhysicalMemory / (1024*1024*1024))
          << " GB" << std::endl;
std::cout << "Available: " << (memInfo.availablePhysicalMemory / (1024*1024*1024))
          << " GB" << std::endl;
```

### **4. Advanced CPU Information and Monitoring**

```cpp
// Get comprehensive CPU information
auto cpuInfo = getCpuInfo();
std::cout << "CPU: " << cpuInfo.model << std::endl;
std::cout << "Vendor: " << cpuVendorToString(cpuInfo.vendor) << std::endl;
std::cout << "Architecture: " << cpuArchitectureToString(cpuInfo.architecture) << std::endl;
std::cout << "Cores: " << cpuInfo.numPhysicalCores << " physical, "
          << cpuInfo.numLogicalCores << " logical" << std::endl;
std::cout << "Base Frequency: " << cpuInfo.baseFrequency << " GHz" << std::endl;

// Real-time monitoring
try {
    auto cpuUsage = getCurrentCpuUsage();
    auto cpuTemp = getCurrentCpuTemperature();

    std::cout << "CPU Usage: " << cpuUsage << "%" << std::endl;
    std::cout << "CPU Temperature: " << cpuTemp << "°C" << std::endl;

    // Temperature warning
    if (cpuTemp > 85.0) {
        std::cout << "⚠️ WARNING: High CPU temperature!" << std::endl;
    }
} catch (const std::exception& e) {
    std::cerr << "Monitoring error: " << e.what() << std::endl;
}
```

### **5. Disk and Storage Information**

```cpp
// Get all disk information
auto disks = getDiskInfo(true); // Include removable drives

for (const auto& disk : disks) {
    std::cout << "Drive: " << disk.path << std::endl;
    std::cout << "  Model: " << disk.model << std::endl;
    std::cout << "  File System: " << disk.fsType << std::endl;
    std::cout << "  Total: " << (disk.totalSpace / (1024*1024*1024)) << " GB" << std::endl;
    std::cout << "  Free: " << (disk.freeSpace / (1024*1024*1024)) << " GB" << std::endl;
    std::cout << "  Usage: " << std::fixed << std::setprecision(1)
              << disk.usagePercent << "%" << std::endl;

    // Disk health warning
    if (disk.usagePercent > 90.0) {
        std::cout << "  ⚠️ WARNING: Low disk space!" << std::endl;
    }
}
```

### **6. Network and WiFi Analysis**

```cpp
// Get network statistics
auto netStats = getNetworkStats();
std::cout << "Download Speed: " << netStats.downloadSpeed << " MB/s" << std::endl;
std::cout << "Upload Speed: " << netStats.uploadSpeed << " MB/s" << std::endl;
std::cout << "Latency: " << netStats.latency << " ms" << std::endl;
std::cout << "Signal Strength: " << netStats.signalStrength << " dBm" << std::endl;

// Scan for WiFi networks
auto networks = scanAvailableNetworks();
std::cout << "Available Networks: " << networks.size() << std::endl;
for (size_t i = 0; i < std::min(networks.size(), size_t(5)); ++i) {
    std::cout << "  " << (i+1) << ". " << networks[i] << std::endl;
}

// Check internet connectivity
if (isConnectedToInternet()) {
    std::cout << "✓ Internet connection available" << std::endl;
} else {
    std::cout << "✗ No internet connection" << std::endl;
}
```

### **7. Comprehensive System Report Generation**

```cpp
// Generate complete system report
auto fullReport = SystemInfoPrinter::generateFullReport();
std::cout << fullReport << std::endl;

// Generate individual component reports
auto osInfo = getOperatingSystemInfo();
auto osReport = SystemInfoPrinter::formatOsInfo(osInfo);

auto cpuInfo = getCpuInfo();
auto cpuReport = SystemInfoPrinter::formatCpuInfo(cpuInfo);

auto memInfo = getDetailedMemoryStats();
auto memReport = SystemInfoPrinter::formatMemoryInfo(memInfo);

// Export report to file
std::ofstream reportFile("system_report.txt");
reportFile << "System Report Generated: " << getCurrentTimestamp() << std::endl;
reportFile << fullReport;
reportFile.close();
```

## 📊 System Information Categories

### **Operating System**

- OS name and version
- Kernel version and build
- Architecture (x86, x64, ARM, etc.)
- Computer name and domain
- Time zone and locale settings
- Character set and encoding

### **Hardware Information**

- CPU model, vendor, and specifications
- Physical and logical core counts
- CPU frequency and cache sizes
- Memory capacity and usage
- Storage devices and capacity
- Network interfaces and configuration

### **Performance Metrics**

- Real-time CPU usage (total and per-core)
- Memory usage and availability
- Disk I/O statistics
- Network traffic statistics
- System load averages
- Temperature monitoring (where available)

### **System Status**

- System uptime and boot time
- Running processes and services
- System health indicators
- Power management status
- Hardware sensor readings

## 🔧 Platform-Specific Features

### **Windows**

- Windows Management Instrumentation (WMI) integration
- Performance Data Helper (PDH) for metrics
- Registry access for system information
- Windows-specific hardware detection

### **Linux**

- /proc filesystem parsing
- sysfs hardware information
- systemd integration
- Linux-specific performance counters

### **macOS**

- System Configuration framework
- IOKit for hardware information
- sysctl system calls
- macOS-specific system APIs

### **FreeBSD**

- sysctl system information
- kvm library integration
- FreeBSD-specific hardware detection
- Performance monitoring frameworks

## 🚨 Current Status and Known Issues

### **Working Components**

- ✅ **Header Inclusion**: All headers can be included successfully
- ✅ **API Structure**: Core API is properly defined and comprehensive
- ✅ **Build System**: All examples build successfully
- ✅ **Cross-Platform**: Code compiles and runs on multiple platforms
- ✅ **Enhanced Examples**: Comprehensive examples with robust error handling
- ✅ **Error Handling**: Graceful degradation when features are unavailable
- ✅ **Documentation**: Complete usage examples and API demonstrations
- ✅ **Performance Monitoring**: Real-time system monitoring capabilities
- ✅ **Report Generation**: Comprehensive system reporting functionality

### **Known Issues and Limitations**

- ⚠️ **Platform-Specific Features**: Some advanced features may be limited on certain platforms
- ⚠️ **Permissions**: Some information may require elevated privileges
- ⚠️ **Hardware Dependencies**: Some features depend on specific hardware capabilities
- ⚠️ **Network Features**: WiFi scanning may not be available on all systems

### **Platform-Specific Issues**

#### **Windows**

- WMI initialization may require COM library setup
- Performance counters may need administrative privileges
- Some hardware information requires specific Windows versions

#### **Linux**

- /proc and /sys filesystem access permissions
- Hardware monitoring requires specific kernel modules
- Some features need root privileges or special groups

#### **macOS**

- IOKit framework access may require entitlements
- Some system information requires administrative access
- Hardware monitoring APIs may be restricted

## 🔍 Troubleshooting

### **Common Issues**

1. **Module Initialization Failures**

   ```
   Error: Failed to initialize system information module
   ```

   - **Solution**: Check platform-specific dependencies
   - **Workaround**: Use header test to verify compilation

2. **Permission Denied**

   ```
   Error: Access denied to system information
   ```

   - **Solution**: Run with appropriate privileges
   - **Check**: User permissions for system information access

3. **Missing Functions**

   ```
   Error: Undefined reference to 'getCurrentCpuUsage'
   ```

   - **Solution**: Check if function is implemented for your platform
   - **Workaround**: Use alternative information gathering methods

### **Debugging Steps**

1. **Verify Header Inclusion**

   ```bash
   ./build/example/sysinfo/sysinfo_header_test.exe
   ```

2. **Check Platform Support**

   ```cpp
   #ifdef _WIN32
       // Windows-specific code
   #elif defined(__linux__)
       // Linux-specific code
   #elif defined(__APPLE__)
       // macOS-specific code
   #endif
   ```

3. **Test Individual Components**

   ```cpp
   // Test each component separately
   try {
       auto osInfo = getOperatingSystemInfo();
       std::cout << "OS info: OK" << std::endl;
   } catch (const std::exception& e) {
       std::cout << "OS info failed: " << e.what() << std::endl;
   }
   ```

## 📚 Usage Best Practices

### **Error Handling**

- Always wrap system information calls in try-catch blocks
- Check for null/empty results before using
- Implement fallback mechanisms for unavailable information
- Log errors for debugging purposes

### **Performance Considerations**

- Cache frequently accessed information
- Use appropriate polling intervals for real-time metrics
- Consider the performance impact of system calls
- Implement rate limiting for continuous monitoring

### **Security Considerations**

- Be aware of information disclosure risks
- Respect user privacy settings
- Handle sensitive system information appropriately
- Consider access control requirements

## 🎯 Future Improvements

### **Planned Features**

- Additional hardware component support (sensors, thermal monitoring)
- Enhanced real-time monitoring with configurable intervals
- More comprehensive network analysis tools
- Advanced system health prediction algorithms
- Integration with system monitoring dashboards

### **Implementation Improvements**

- Performance optimizations for large-scale monitoring
- Enhanced caching mechanisms for frequently accessed data
- Improved cross-platform feature parity
- Additional export formats for system reports
- Enhanced security and permission handling

## 🏆 Success Story

The Atom Sysinfo module examples have been successfully enhanced with:

- ✅ **Complete Feature Coverage**: All major system information APIs are demonstrated
- ✅ **Robust Error Handling**: Graceful degradation when features are unavailable
- ✅ **Cross-Platform Compatibility**: Examples work across Windows, Linux, macOS, and FreeBSD
- ✅ **Real-World Usage**: Practical examples that can be used in production applications
- ✅ **Comprehensive Documentation**: Clear usage patterns and best practices
- ✅ **Performance Monitoring**: Real-time system monitoring capabilities
- ✅ **Professional Reporting**: Enterprise-grade system report generation

---

**Note**: The enhanced sysinfo module examples provide a complete demonstration of the module's capabilities. All examples include comprehensive error handling, detailed documentation, and practical usage patterns that can be directly applied in real-world applications.
