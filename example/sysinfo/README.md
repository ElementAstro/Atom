# Atom Sysinfo Module Examples

This directory contains examples demonstrating the system information gathering capabilities of the Atom framework.

## 🚀 Overview

The Atom sysinfo module provides comprehensive system information gathering including:
- **Operating System**: OS details, version, architecture, and properties
- **Hardware Information**: CPU, memory, storage, and network details
- **System Monitoring**: Real-time system metrics and performance data
- **Cross-Platform**: Unified API across Windows, Linux, macOS, and FreeBSD

## 📁 Examples

### ✅ **Header Test**
**File**: `header_test.cpp`  
**Status**: Fully functional ✅

A simple test that verifies the sysinfo module headers can be included successfully.

#### **Features Demonstrated**
- Header inclusion verification
- Basic module structure validation
- Compilation compatibility testing

### 🔧 **Basic Sysinfo Example**
**File**: `basic_sysinfo_example.cpp`  
**Status**: Has runtime issues (dependency/initialization specific)

Demonstrates basic system information gathering capabilities.

#### **Features Demonstrated**
- Operating system information retrieval
- System uptime and status
- Memory usage monitoring
- System language and encoding detection
- Basic system health checks

### 🔧 **System Info Example**
**File**: `system_info_example.cpp`  
**Status**: Has linking issues (missing function implementations)

Comprehensive demonstration of system information capabilities.

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

# Build the sysinfo examples
cmake --build build --target sysinfo_header_test
cmake --build build --target sysinfo_basic_sysinfo_example
cmake --build build --target sysinfo_system_info_example
```

### Run the Examples
```bash
# Run the working header test
./build/example/sysinfo/sysinfo_header_test.exe

# Note: Other examples may have runtime issues
# ./build/example/sysinfo/sysinfo_basic_sysinfo_example.exe
# ./build/example/sysinfo/sysinfo_system_info_example.exe
```

## 🎯 Key Features (Intended)

### **1. Operating System Information**
```cpp
// Get comprehensive OS information
auto osInfo = getOperatingSystemInfo();
std::cout << "OS: " << osInfo.osName << " " << osInfo.osVersion << std::endl;
std::cout << "Architecture: " << osInfo.architecture << std::endl;
std::cout << "Kernel: " << osInfo.kernelVersion << std::endl;
```

### **2. System Uptime and Status**
```cpp
// Get system uptime
auto uptime = getSystemUptime();
std::cout << "Uptime: " << uptime.count() << " seconds" << std::endl;

// Check system language and encoding
std::cout << "Language: " << getSystemLanguage() << std::endl;
std::cout << "Encoding: " << getSystemEncoding() << std::endl;
```

### **3. Memory Information**
```cpp
// Get memory usage
auto memoryUsage = getMemoryUsage();
std::cout << "Memory usage: " << memoryUsage << "%" << std::endl;

// Get detailed memory statistics
auto memInfo = getDetailedMemoryStats();
std::cout << "Total RAM: " << (memInfo.totalPhysicalMemory / (1024*1024*1024)) << " GB" << std::endl;
```

### **4. CPU Information**
```cpp
// Get CPU information
auto cpuInfo = getCpuInfo();
std::cout << "CPU: " << cpuInfo.model << std::endl;
std::cout << "Cores: " << cpuInfo.numPhysicalCores << " physical, " 
          << cpuInfo.numLogicalCores << " logical" << std::endl;

// Get current CPU usage
auto cpuUsage = getCurrentCpuUsage();
std::cout << "CPU usage: " << cpuUsage << "%" << std::endl;
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
- ✅ **API Structure**: Core API is properly defined
- ✅ **Build System**: Examples build successfully
- ✅ **Cross-Platform**: Code compiles on multiple platforms

### **Known Issues**
- ❌ **Runtime Initialization**: Module initialization fails on some platforms
- ❌ **Function Implementations**: Some functions may not be fully implemented
- ❌ **Platform Dependencies**: Missing platform-specific libraries
- ❌ **Linking Issues**: Some symbols may not be properly linked

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
- Better error reporting and diagnostics
- Enhanced cross-platform compatibility
- More comprehensive hardware detection
- Real-time monitoring capabilities

### **Implementation Improvements**
- Complete function implementations for all platforms
- Better dependency management
- Improved initialization and cleanup
- Enhanced performance optimization

---

**Note**: While the sysinfo module examples currently have runtime issues, the header test demonstrates that the module structure is sound. The comprehensive examples showcase the intended functionality and serve as a foundation for future development and debugging.
