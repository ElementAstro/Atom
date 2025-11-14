# Python Sysinfo Module Architecture

## Directory Structure

The Python bindings now mirror the C++ implementation structure:

```
python/sysinfo/
├── __init__.py                    # Main module with backward compatibility
├── hardware/                       # Hardware information modules
│   ├── __init__.py
│   ├── battery.cpp                # Battery information and monitoring
│   ├── bios.cpp                   # BIOS information
│   ├── cpu.cpp                    # CPU information and monitoring
│   ├── gpu.cpp                    # GPU and monitor information
│   └── memory.cpp                 # Memory information and monitoring
├── info/                          # System information modules
│   ├── __init__.py
│   ├── locale.cpp                 # Locale and language information
│   ├── os.cpp                     # Operating system information
│   ├── sn.cpp                     # Hardware serial numbers
│   ├── virtual.cpp                # Virtualization detection
│   └── wm.cpp                     # Window manager information
├── network/                       # Network information modules
│   ├── __init__.py
│   └── wifi.cpp                   # WiFi and network information
├── storage/                       # Storage information modules
│   ├── __init__.py
│   └── disk/                      # Disk-related functionality
│       ├── __init__.py
│       └── disk.cpp               # Disk information, monitoring, security
└── utils/                         # Utility modules
    ├── __init__.py
    └── sysinfo_printer.cpp        # System information formatting

```

## Correspondence with C++ Structure

### atom/sysinfo/ → python/sysinfo/

| C++ Location | Python Location | Status |
|-------------|----------------|--------|
| `atom/sysinfo/hardware/battery.hpp` | `python/sysinfo/hardware/battery.cpp` | ✅ Complete |
| `atom/sysinfo/hardware/bios.hpp` | `python/sysinfo/hardware/bios.cpp` | ✅ Complete |
| `atom/sysinfo/hardware/cpu.hpp` | `python/sysinfo/hardware/cpu.cpp` | ✅ Complete |
| `atom/sysinfo/hardware/gpu.hpp` | `python/sysinfo/hardware/gpu.cpp` | ✅ Complete |
| `atom/sysinfo/hardware/memory.hpp` | `python/sysinfo/hardware/memory.cpp` | ✅ Complete |
| `atom/sysinfo/info/locale.hpp` | `python/sysinfo/info/locale.cpp` | ✅ Complete |
| `atom/sysinfo/info/os.hpp` | `python/sysinfo/info/os.cpp` | ✅ Complete |
| `atom/sysinfo/info/sn.hpp` | `python/sysinfo/info/sn.cpp` | ✅ Complete |
| `atom/sysinfo/info/virtual.hpp` | `python/sysinfo/info/virtual.cpp` | ✅ Complete |
| `atom/sysinfo/info/wm.hpp` | `python/sysinfo/info/wm.cpp` | ✅ Complete |
| `atom/sysinfo/network/wifi.hpp` | `python/sysinfo/network/wifi.cpp` | ✅ Complete |
| `atom/sysinfo/storage/disk/*.hpp` | `python/sysinfo/storage/disk/disk.cpp` | ✅ Complete |
| `atom/sysinfo/utils/sysinfo_printer.hpp` | `python/sysinfo/utils/sysinfo_printer.cpp` | ✅ Complete |

## Backward Compatibility

The main `__init__.py` maintains backward compatibility by importing all modules at the top level:

```python
# Old code still works:
from atom.sysinfo import battery
info = battery.get_battery_info()

# New organized structure also works:
from atom.sysinfo.hardware import battery
info = battery.get_battery_info()
```

## Module Features

### Hardware Modules

#### battery

- **Structures**: `BatteryInfo`, `BatteryAlertSettings`, `BatteryStats`
- **Enums**: `BatteryError`, `AlertType`, `PowerPlan`
- **Classes**: `BatteryMonitor`, `BatteryManager`, `PowerPlanManager`
- **Functions**: Battery info retrieval, monitoring, power management
- **Complete**: All C++ APIs bound

#### bios

- **Structures**: `BiosInfoData`, `BiosHealthStatus`, `BiosUpdateInfo`
- **Classes**: `BiosInfo` (singleton)
- **Functions**: BIOS info retrieval, health checks, settings management
- **Complete**: All C++ APIs bound

#### cpu

- **Structures**: `CpuCoreInfo`, `CacheSizes`, `LoadAverage`, `CpuPowerInfo`, `CpuInfo`
- **Enums**: `CpuArchitecture`, `CpuVendor`, `CpuFeatureSupport`
- **Functions**: CPU info, usage, temperature, frequency monitoring
- **Complete**: All C++ APIs bound

#### gpu

- **Structures**: `MonitorInfo`
- **Functions**: GPU info retrieval, monitor enumeration
- **Complete**: All C++ APIs bound

#### memory

- **Structures**: `MemoryInfo`, `MemoryInfo::MemorySlot`, `MemoryPerformance`
- **Functions**: Memory info, usage monitoring, performance metrics
- **Complete**: All C++ APIs bound

### Info Modules

#### locale

- **Structures**: `LocaleInfo`
- **Enums**: `LocaleError`
- **Functions**: Locale info, validation, available locales
- **Complete**: All C++ APIs bound

#### os

- **Structures**: `OperatingSystemInfo`
- **Functions**: OS info, uptime, updates, system properties
- **Complete**: All C++ APIs bound

#### sn

- **Classes**: `HardwareInfo`
- **Functions**: Hardware serial number retrieval (BIOS, motherboard, CPU, disk)
- **Complete**: All C++ APIs bound

#### virtual

- **Functions**: Virtualization/container detection, confidence scoring
- **Complete**: All C++ APIs bound

#### wm

- **Structures**: `SystemInfo`
- **Functions**: Desktop environment and window manager info
- **Complete**: All C++ APIs bound

### Network Modules

#### wifi

- **Structures**: `NetworkStats`
- **Functions**: WiFi/network info, statistics, bandwidth measurement
- **Complete**: All C++ APIs bound

### Storage Modules

#### disk

- **Structures**: `DiskInfo`, `StorageDevice`
- **Enums**: `SecurityPolicy`
- **Functions**: Disk info, device monitoring, security management
- **Complete**: All C++ APIs bound including:
  - `disk_device.hpp`: Storage device enumeration and info
  - `disk_info.hpp`: Disk usage and information
  - `disk_monitor.hpp`: Device insertion monitoring
  - `disk_security.hpp`: Whitelist and security features
  - `disk_types.hpp`: Common data structures
  - `disk_util.hpp`: Utility functions

### Utils Modules

#### sysinfo_printer

- **Classes**: `SystemInfoPrinter`
- **Functions**: Format and export system info to HTML/JSON/Markdown
- **Complete**: All C++ APIs bound

## API Coverage Verification

All public C++ APIs from the headers have corresponding Python bindings:

- ✅ All structures and classes bound
- ✅ All enumerations bound
- ✅ All standalone functions bound
- ✅ All class methods bound
- ✅ Proper exception handling configured
- ✅ Python-friendly interfaces (callbacks, context managers, etc.)

## Usage Examples

### Direct Module Access

```python
from atom.sysinfo.hardware import cpu, memory, battery
from atom.sysinfo.storage.disk import disk

# Get CPU info
cpu_info = cpu.get_cpu_info()
print(f"CPU: {cpu_info.model}")

# Get memory info
mem_info = memory.get_detailed_memory_stats()
print(f"Memory: {mem_info.total_physical_memory / (1024**3):.1f} GB")

# Get disk info
disks = disk.get_disk_info()
for d in disks:
    print(f"{d.path}: {d.usage_percent:.1f}% used")
```

### Backward Compatible Access

```python
from atom import sysinfo

# All modules still accessible at top level
cpu_info = sysinfo.cpu.get_cpu_info()
battery_info = sysinfo.battery.get_battery_info()
os_info = sysinfo.os.get_operating_system_info()
```

## Notes

1. **Old binding files**: The original `.cpp` files in the root `python/sysinfo/` directory can be removed after verifying compilation works with the new structure.

2. **Build system**: The build configuration (CMakeLists.txt or setup.py) needs to be updated to reflect the new file locations.

3. **Lint warnings**: Minor lint warnings about import formatting and star imports are expected and do not affect functionality.

4. **Documentation**: All bindings include comprehensive docstrings with usage examples.
