# atom/sysinfo - System Information Module

> **Module Version:** 1.0.0
> **Documentation Version:** 1.0.0
> **Last Updated:** 2025-01-15

---

## Navigation

[Root Directory](../../CLAUDE.md) > **sysinfo**

---

## Module Overview

The **atom::sysinfo** module provides comprehensive system information querying capabilities for the Atom framework. It offers cross-platform APIs to retrieve hardware, network, storage, and operating system information.

### Key Features

- **CPU Information**: Processor details, architecture, core counts, usage statistics
- **GPU Information**: Graphics device detection and capabilities
- **Memory Information**: RAM statistics, memory usage, swap space
- **Disk Information**: Storage devices, usage, monitoring
- **Network Information**: WiFi status, network interfaces, connection data
- **System Information**: OS details, locale, window manager, virtualization detection
- **Battery Monitoring**: Power status, battery life, AC adapter detection
- **BIOS Information**: System firmware, motherboard details

---

## Directory Structure

```
atom/sysinfo/
├── hardware/          # Hardware information
│   ├── cpu.hpp
│   ├── cpu/
│   │   ├── common.hpp
│   │   ├── common.cpp
│   │   ├── windows.cpp
│   │   ├── linux.cpp
│   │   ├── macos.cpp
│   │   └── freebsd.cpp
│   ├── memory.hpp
│   ├── memory/
│   │   ├── memory.cpp
│   │   ├── common.hpp
│   │   ├── common.cpp
│   │   ├── windows.hpp
│   │   ├── windows.cpp
│   │   ├── linux.hpp
│   │   └── linux.cpp
│   ├── battery.hpp
│   ├── battery.cpp
│   ├── bios.hpp
│   ├── bios.cpp
│   └── gpu.hpp
│   ├── gpu.cpp
├── storage/           # Storage information
│   └── disk/
│       ├── disk_device.hpp
│       ├── disk_device.cpp
│       ├── disk_info.hpp
│       ├── disk_info.cpp
│       ├── disk_monitor.hpp
│       ├── disk_monitor.cpp
│       ├── disk_security.hpp
│       ├── disk_security.cpp
│       ├── disk_util.hpp
│       ├── disk_util.cpp
│       └── disk_types.hpp
├── network/           # Network information
│   └── wifi/
│       ├── wifi.hpp
│       ├── wifi.cpp
│       ├── common.hpp
│       ├── common.cpp
│       ├── windows.hpp
│       ├── windows.cpp
│       ├── linux.hpp
│       ├── linux.cpp
│       └── macos.cpp
├── info/              # General system info
│   ├── locale.hpp
│   ├── locale.cpp
│   ├── os.hpp
│   ├── os.cpp
│   ├── wm.hpp
│   ├── wm.cpp
│   ├── sn.hpp
│   ├── sn.cpp
│   ├── virtual.hpp
│   └── virtual.cpp
└── utils/             # Utility functions
    ├── sysinfo_printer.hpp
    └── sysinfo_printer.cpp
```

---

## Core Components

### CPU Information

```cpp
#include "atom/sysinfo/hardware/cpu.hpp"

using namespace atom::sysinfo;

CPUInfo cpu = CPUInfo::get();

std::cout << "Model: " << cpu.modelName << "\n";
std::cout << "Vendor: " << cpu.vendor << "\n";
std::cout << "Architecture: " << cpu.architecture << "\n";
std::cout << "Cores: " << cpu.numPhysicalCores << "\n";
std::cout << "Threads: " << cpu.numLogicalCores << "\n";
std::cout << "Frequency: " << cpu.maxFrequency << " MHz\n";

// Get current usage
auto usage = cpu.getCurrentUsage();
std::cout << "Usage: " << usage.percent << "%\n";
```

### GPU Information

```cpp
#include "atom/sysinfo/hardware/gpu.hpp"

using namespace atom::sysinfo;

auto gpus = GPUInfo::getAll();

for (const auto& gpu : gpus) {
    std::cout << "GPU: " << gpu.name << "\n";
    std::cout << "  Vendor: " << gpu.vendor << "\n";
    std::cout << "  Memory: " << gpu.memory << " MB\n";
    std::cout << "  Driver: " << gpu.driverVersion << "\n";
}
```

### Memory Information

```cpp
#include "atom/sysinfo/hardware/memory.hpp"

using namespace atom::sysinfo;

MemoryInfo mem = MemoryInfo::get();

std::cout << "Total: " << mem.totalMB << " MB\n";
std::cout << "Available: " << mem.availableMB << " MB\n";
std::cout << "Used: " << mem.usedMB << " MB\n";
std::cout << "Usage: " << mem.usagePercent << "%\n";

// Swap information
std::cout << "Swap Total: " << mem.swapTotalMB << " MB\n";
std::cout << "Swap Used: " << mem.swapUsedMB << " MB\n";
```

### Disk Information

```cpp
#include "atom/sysinfo/storage/disk.hpp"

using namespace atom::sysinfo;

// Get all disk devices
auto disks = DiskDevice::getAll();

for (const auto& disk : disks) {
    std::cout << "Device: " << disk.device << "\n";
    std::cout << "  Size: " << disk.sizeGB << " GB\n";
    std::cout << "  Model: " << disk.model << "\n";
    std::cout << "  Type: " << disk.type << "\n";
}

// Monitor disk usage
DiskMonitor monitor;
auto usage = monitor.getDiskUsage("/home");
std::cout << "Usage: " << usage.usedGB << "/" << usage.totalGB << " GB\n";
```

### WiFi Information

```cpp
#include "atom/sysinfo/network/wifi.hpp"

using namespace atom::sysinfo;

WiFiInfo wifi = WiFiInfo::get();

std::cout << "SSID: " << wifi.ssid << "\n";
std::cout << "Signal: " << wifi.signalStrength << "%\n";
std::cout << "Frequency: " << wifi.frequency << " GHz\n";
std::cout << "Connected: " << (wifi.isConnected ? "Yes" : "No") << "\n";
```

### System Information

```cpp
#include "atom/sysinfo/info/os.hpp"
#include "atom/sysinfo/info/locale.hpp"

using namespace atom::sysinfo;

OSInfo os = OSInfo::get();
std::cout << "OS: " << os.name << " " << os.version << "\n";
std::cout << "Kernel: " << os.kernelVersion << "\n";
std::cout << "Architecture: " << os.architecture << "\n";

LocaleInfo locale = LocaleInfo::get();
std::cout << "Language: " << locale.language << "\n";
std::cout << "Country: " << locale.country << "\n";
std::cout << "Encoding: " << locale.encoding << "\n";
```

### Battery Information

```cpp
#include "atom/sysinfo/hardware/battery.hpp"

using namespace atom::sysinfo;

BatteryInfo battery = BatteryInfo::get();

std::cout << "Level: " << battery.level << "%\n";
std::cout << "Charging: " << (battery.isCharging ? "Yes" : "No") << "\n";
if (battery.isCharging) {
    std::cout << "Time to full: " << battery.timeToFull << " min\n";
} else {
    std::cout << "Time remaining: " << battery.timeRemaining << " min\n";
}
```

---

## Public Interfaces

### CPUInfo Structure

```cpp
struct CPUInfo {
    std::string modelName;       // CPU model name
    std::string vendor;          // Vendor (Intel, AMD, etc.)
    std::string architecture;    // Architecture (x86_64, ARM, etc.)
    int numPhysicalCores;        // Physical core count
    int numLogicalCores;         // Logical core count (with hyperthreading)
    int maxFrequency;            // Maximum frequency in MHz
    std::vector<std::string> features;  // CPU features (SSE, AVX, etc.)

    // Current usage statistics
    struct Usage {
        float percent;           // Overall CPU usage (0-100)
        std::vector<float> perCore;  // Per-core usage
    };

    static CPUInfo get();
    Usage getCurrentUsage();
};
```

### GPUInfo Structure

```cpp
struct GPUInfo {
    std::string name;            // GPU name
    std::string vendor;          // Vendor (NVIDIA, AMD, Intel, etc.)
    int memory;                  // Memory in MB
    std::string driverVersion;   // Driver version
    std::string driverDate;      // Driver date

    static std::vector<GPUInfo> getAll();
};
```

### MemoryInfo Structure

```cpp
struct MemoryInfo {
    size_t totalMB;              // Total physical memory
    size_t availableMB;          // Available memory
    size_t usedMB;               // Used memory
    float usagePercent;          // Usage percentage

    // Swap information
    size_t swapTotalMB;
    size_t swapUsedMB;

    static MemoryInfo get();
};
```

### DiskDevice Structure

```cpp
struct DiskDevice {
    std::string device;          // Device path (/dev/sda1, C:, etc.)
    std::string model;           // Disk model
    std::string type;            // Type (SSD, HDD, NVMe, etc.)
    size_t sizeGB;               // Size in GB
    bool isRemovable;            // Removable media

    static std::vector<DiskDevice> getAll();
};
```

### WiFiInfo Structure

```cpp
struct WiFiInfo {
    std::string ssid;            // Network SSID
    int signalStrength;          // Signal strength (0-100)
    double frequency;            // Frequency in GHz
    bool isConnected;            // Connection status
    std::string bssid;           // BSSID (MAC address)
    std::string security;        // Security type (WPA2, WEP, etc.)

    static WiFiInfo get();
    static std::vector<WiFiInfo> getAvailableNetworks();
};
```

---

## Dependencies

### Required Dependencies

- **atom::error**: Error handling framework
- **fmt**: Enhanced formatting (optional but recommended)
- **spdlog**: Enhanced logging (optional but recommended)

### Platform-Specific Libraries

**Windows:**

- pdh (Performance Data Helper)
- wlanapi (WiFi API)
- ws2_32 (Windows Sockets)
- setupapi (Device installation)
- iphlpapi (IP Helper API)
- dwmapi (Desktop Window Manager)
- powrprof (Power management)

**Linux:**

- Standard system libraries via libc
- Wireless extensions for WiFi

**macOS:**

- IOKit framework
- Foundation framework
- CoreWLAN framework

---

## Build Configuration

### CMake Options

```cmake
# Build the sysinfo module
-DBUILD_SYSINFO=ON

# The module automatically detects platform and links
# appropriate libraries
```

### Platform Detection

The module automatically detects the platform and compiles the appropriate platform-specific source files:

- **Windows**: Compiles `windows.cpp` files
- **Linux**: Compiles `linux.cpp` files
- **macOS**: Compiles `macos.cpp` files
- **FreeBSD**: Compiles `freebsd.cpp` files

---

## Usage Examples

### System Monitoring Dashboard

```cpp
#include "atom/sysinfo/hardware/cpu.hpp"
#include "atom/sysinfo/hardware/memory.hpp"
#include "atom/sysinfo/hardware/gpu.hpp"

void printSystemStatus() {
    using namespace atom::sysinfo;

    // CPU
    auto cpu = CPUInfo::get();
    auto cpuUsage = cpu.getCurrentUsage();
    std::cout << "CPU: " << cpuUsage.percent << "%\n";

    // Memory
    auto mem = MemoryInfo::get();
    std::cout << "RAM: " << mem.usedMB << "/" << mem.totalMB << " MB\n";

    // GPU
    auto gpus = GPUInfo::getAll();
    for (const auto& gpu : gpus) {
        std::cout << "GPU: " << gpu.name << "\n";
    }
}
```

### Disk Usage Monitor

```cpp
#include "atom/sysinfo/storage/disk.hpp"

void checkDiskSpace(const std::string& path) {
    using namespace atom::sysinfo;

    DiskMonitor monitor;
    auto usage = monitor.getDiskUsage(path);

    float percent = (100.0f * usage.usedGB) / usage.totalGB;
    std::cout << "Disk usage: " << percent << "%\n";

    if (percent > 90.0f) {
        std::cerr << "WARNING: Disk almost full!\n";
    }
}
```

### WiFi Scanner

```cpp
#include "atom/sysinfo/network/wifi.hpp"

void scanNetworks() {
    using namespace atom::sysinfo;

    auto networks = WiFiInfo::getAvailableNetworks();

    std::cout << "Available networks:\n";
    for (const auto& net : networks) {
        std::cout << "  " << net.ssid
                  << " (" << net.signalStrength << "%)\n";
    }
}
```

---

## Platform-Specific Notes

### Windows

- Some APIs require administrator privileges
- GPU detection may fail without proper drivers
- Battery information may not be available on desktop systems

### Linux

- Requires `/proc` filesystem access
- Some information requires root privileges
- GPU detection varies by driver (NVIDIA, AMD, Intel)

### macOS

- Most information available without special privileges
- GPU detection via IOKit
- Battery information available on portable Macs

### FreeBSD

- Limited GPU detection support
- CPU and memory information fully supported
- Network interface detection supported

---

## Performance Considerations

### Caching Information

Some system information queries are expensive. Cache results when appropriate:

```cpp
class SystemInfoCache {
    CPUInfo cpuCache;
    MemoryInfo memCache;

public:
    const CPUInfo& getCPU() {
        if (/* cache expired */) {
            cpuCache = CPUInfo::get();
        }
        return cpuCache;
    }
};
```

### Polling Frequency

For monitoring applications, avoid excessive polling:

```cpp
// Good: Poll every second
std::this_thread::sleep_for(std::chrono::seconds(1));

// Bad: Poll in tight loop
while (true) {
    auto cpu = CPUInfo::getCurrentUsage();  // Expensive!
}
```

---

## Testing

### Test Organization

Tests are located in `tests/sysinfo/`:

- `test_cpu.cpp`: CPU information tests
- `test_memory.cpp`: Memory information tests
- `test_disk.cpp`: Disk information tests
- `test_network.cpp`: Network information tests

### Running Tests

```bash
# Build tests
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run sysinfo tests
ctest -R sysinfo_ --output-on-failure
```

---

## Common Patterns

### Cross-Platform System Information

```cpp
#include "atom/sysinfo/info/os.hpp"

void printSystemInfo() {
    using namespace atom::sysinfo;

    OSInfo os = OSInfo::get();

    std::cout << "System Information:\n";
    std::cout << "  OS: " << os.name << "\n";
    std::cout << "  Version: " << os.version << "\n";
    std::cout << "  Architecture: " << os.architecture << "\n";

    #ifdef _WIN32
    std::cout << "  Platform: Windows\n";
    #elif __linux__
    std::cout << "  Platform: Linux\n";
    #elif __APPLE__
    std::cout << "  Platform: macOS\n";
    #endif
}
```

---

## Related Modules

- **atom::system**: System-level operations
- **atom::hardware**: Hardware interaction
- **atom::utils**: Utility functions

---

## Change Log

### 2025-01-15

- Initial module documentation
- Documented all hardware, storage, network components
- Added usage examples and platform-specific notes

---

**Maintained By:** Atom Framework Team
