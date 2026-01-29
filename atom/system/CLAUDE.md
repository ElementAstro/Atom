# atom/system - System Integration Module

> **Module Version:** 1.0.0
> **Documentation Version:** 1.0.0
> **Last Updated:** 2025-01-15

---

## Navigation

[Root Directory](../../CLAUDE.md) > **system**

---

## Module Overview

The **atom::system** module provides comprehensive system-level integration utilities for the Atom framework. It offers cross-platform APIs for process management, hardware interaction, system information access, and platform-specific operations.

### Key Features

- **Process Management**: Create, monitor, and control system processes
- **Hardware Interaction**: GPIO, voltage monitoring, device management
- **System Information**: Environment variables, user information, software detection
- **Platform Integration**: Registry access (Windows), clipboard, signal handling
- **Network Management**: Virtual network interfaces and monitoring
- **Power Management**: System power state monitoring
- **Scheduling**: Crontab-style task scheduling
- **Storage Management**: Disk operations and file system monitoring

---

## Directory Structure

```
atom/system/
├── core/              # Core system functionality
│   ├── platform.hpp   # Platform detection utilities
│   ├── priority.hpp   # Thread/process priority management
│   └── _constant.hpp  # System constants
├── process/           # Process management
│   ├── process.hpp
│   ├── process_manager.hpp
│   ├── command.hpp
│   ├── pidwatcher.hpp
│   └── process_info.hpp
├── hardware/          # Hardware interaction
│   ├── device.hpp
│   ├── gpio.hpp
│   └── voltage.hpp
├── power/             # Power management
│   └── power.hpp
├── info/              # System information
│   ├── env.hpp
│   ├── software.hpp
│   ├── stat.hpp
│   └── user.hpp
├── registry/          # Registry access (Windows/Linux)
│   ├── wregistry.hpp
│   └── lregistry.hpp
├── network/           # Network utilities
│   ├── network_manager.hpp
│   └── virtual_network.hpp
├── storage/           # Storage operations
│   └── storage.hpp
├── signals/           # Signal handling
│   ├── signal.hpp
│   ├── signal_monitor.hpp
│   └── signal_utils.hpp
├── scheduling/        # Task scheduling
│   └── crontab.hpp
├── clipboard/         # Clipboard operations
│   └── clipboard.hpp
├── debug/             # Debug utilities
│   ├── crash.hpp
│   ├── crash_quotes.hpp
│   └── nodebugger.hpp
└── shortcut/          # Keyboard shortcuts
    ├── detector.hpp
    └── shortcut.cpp
```

---

## Core Components

### Process Management

#### `ProcessManager`

Manages multiple processes with lifecycle control:

```cpp
#include "atom/system/process/process_manager.hpp"

using namespace atom::system;

ProcessManager manager;

// Start a process
auto pid = manager.startProcess("my_app", {"--arg1", "--arg2"});

// Monitor process status
if (manager.isRunning(pid)) {
    auto info = manager.getProcessInfo(pid);
    std::cout << "CPU: " << info.cpuUsage << "%\n";
}

// Stop a process
manager.stopProcess(pid);
```

#### `Command`

Execute shell commands safely:

```cpp
#include "atom/system/process/command.hpp"

Command cmd;
auto result = cmd.execute("ls -la");
std::cout << result.output << std::endl;
```

### Hardware Interaction

#### GPIO Control

```cpp
#include "atom/system/hardware/gpio.hpp"

using namespace atom::system;

GPIO gpio;
gpio.exportPin(17);
gpio.setDirection(17, GPIODirection::Out);
gpio.writePin(17, GPIOValue::High);
```

#### Voltage Monitoring

```cpp
#include "atom/system/hardware/voltage.hpp"

VoltageMonitor monitor;
auto voltages = monitor.readAllVoltages();
for (const auto& [rail, voltage] : voltages) {
    std::cout << rail << ": " << voltage << "V\n";
}
```

### System Information

#### Environment Access

```cpp
#include "atom/system/info/env.hpp"

using namespace atom::system::info;

Env env;
env.set("MY_VAR", "value");
auto value = env.get("MY_VAR");
auto all = env.getAll();
```

#### User Information

```cpp
#include "atom/system/info/user.hpp"

User user;
std::cout << "Username: " << user.getUserName() << "\n";
std::cout << "Home: " << user.getHomeDirectory() << "\n";
```

---

## Public Interfaces

### ProcessManager Class

```cpp
class ProcessManager {
public:
    // Process lifecycle
    pid_t startProcess(const std::string& command,
                      const std::vector<std::string>& args);
    bool stopProcess(pid_t pid);
    bool restartProcess(pid_t pid);

    // Monitoring
    bool isRunning(pid_t pid) const;
    ProcessInfo getProcessInfo(pid_t pid) const;
    std::vector<pid_t> getAllProcesses() const;

    // Configuration
    void setAutoRestart(bool enabled);
    void setMaxRestarts(int count);
};
```

### SerialConfig Class

```cpp
class SerialConfig {
public:
    enum class Parity { None, Odd, Even, Mark, Space };
    enum class StopBits { One, OnePointFive, Two };
    enum class FlowControl { None, Software, Hardware };

    SerialConfig& withBaudRate(int rate);
    SerialConfig& withDataBits(int bits);
    SerialConfig& withParity(Parity p);
    SerialConfig& withStopBits(StopBits sb);
    SerialConfig& withFlowControl(FlowControl flow);
    SerialConfig& withReadTimeout(std::chrono::milliseconds timeout);

    static SerialConfig standardConfig(int baudRate);
};
```

---

## Dependencies

### Required Dependencies

- **atom::sysinfo**: System information queries
- **atom::meta**: Reflection and metaprogramming utilities
- **atom::utils**: General utility functions
- **atom::error**: Error handling framework

### Optional Dependencies

- **libusb-1.0**: USB device support
- **spdlog**: Enhanced logging

### Platform-Specific Dependencies

**Windows:**

- pdh, wlanapi, userenv, version, advapi32, hid, setupapi

**Linux:**

- libudev, bluez (for Bluetooth)

**macOS:**

- IOKit, Foundation, IOBluetooth

---

## Build Options

### CMake Options

```cmake
# Build the system module
-DBUILD_SYSTEM=ON

# Enable USB support (requires libusb-1.0)
-DBUILD_SYSTEM_USB=ON
```

### Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Windows (MSVC) | Full | Full feature support |
| Windows (MinGW) | Full | Full feature support |
| Linux | Full | Requires udev dev files |
| macOS | Full | Requires Xcode tools |

---

## Usage Examples

### Process Launching and Monitoring

```cpp
#include "atom/system/process/process_manager.hpp"

using namespace atom::system;

int main() {
    ProcessManager manager;

    // Start with auto-restart
    manager.setAutoRestart(true);
    manager.setMaxRestarts(3);

    auto pid = manager.startProcess("my_service", {"--config", "app.json"});

    while (manager.isRunning(pid)) {
        auto info = manager.getProcessInfo(pid);
        std::cout << "Memory: " << info.memoryUsage << " MB\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
```

### Scheduled Task Execution

```cpp
#include "atom/system/scheduling/crontab.hpp"

using namespace atom::system;

int main() {
    Crontab crontab;

    // Schedule task every 5 minutes
    crontab.addTask("*/5 * * * *", []() {
        // Your task here
        std::cout << "Executing scheduled task\n";
    });

    crontab.start();
    std::this_thread::sleep_for(std::chrono::hours(1));
    crontab.stop();

    return 0;
}
```

### Signal Monitoring

```cpp
#include "atom/system/signals/signal_monitor.hpp"

using namespace atom::system;

int main() {
    SignalMonitor monitor;

    monitor.onSignal(SIGINT, [](int signal) {
        std::cout << "Received SIGINT, shutting down...\n";
    });

    monitor.onSignal(SIGTERM, [](int signal) {
        std::cout << "Received SIGTERM, cleaning up...\n";
    });

    monitor.start();

    // Your application logic here

    monitor.stop();
    return 0;
}
```

---

## Testing

### Test Organization

Tests are located in `tests/system/`:

- `test_process.cpp`: Process management tests
- `test_hardware.cpp`: Hardware interaction tests
- `test_signals.cpp`: Signal handling tests
- `test_scheduling.cpp`: Crontab tests

### Running Tests

```bash
# Build tests
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run system tests
ctest -R system_ --output-on-failure
```

---

## Platform-Specific Notes

### Windows

- Registry access uses Windows Registry API
- Hardware device detection via SetupAPI
- Signal handling uses Windows console events

### Linux

- GPIO requires root privileges or appropriate udev rules
- Device detection via sysfs and udev
- Signal handling uses POSIX signals

### macOS

- Hardware interaction requires IOKit framework
- Bluetooth uses IOBluetooth framework
- Some features may require elevated privileges

---

## Common Patterns

### Safe Process Execution

```cpp
// Always check if process is running before operations
if (manager.isRunning(pid)) {
    manager.stopProcess(pid);
    // Wait for graceful shutdown
    manager.waitFor(pid, std::chrono::seconds(5));
}
```

### Error Handling

```cpp
try {
    gpio.exportPin(17);
} catch (const atom::error::Exception& e) {
    ATOM_ERROR("GPIO export failed: {}", e.what());
}
```

---

## Related Modules

- **atom::sysinfo**: System information queries
- **atom::async**: Asynchronous process operations
- **atom::io**: File and I/O operations
- **atom::connection**: Network communication

---

## Change Log

### 2025-01-15

- Initial module documentation
- Documented all core system components
- Added usage examples and patterns

---

**Maintained By:** Atom Framework Team
