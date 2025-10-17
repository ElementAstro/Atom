# Atom System Module Examples

This directory contains comprehensive examples demonstrating all capabilities of the Atom System module. The examples are organized from basic usage to advanced integration patterns, covering process management, hardware control, network operations, storage monitoring, and system services.

## 🚀 Quick Start

### Prerequisites

- CMake 3.15 or higher
- C++17 compatible compiler
- Platform-specific dependencies (see [Platform Requirements](#platform-requirements))

### Building Examples

```bash
# Configure with examples enabled
cmake -B build -S . -DATOM_EXAMPLE_BUILD_ALL=ON

# Build all system examples
cmake --build build --target system_examples

# Build specific example
cmake --build build --target system_<example_name>
```

### Running Examples

```bash
# Run from build directory
./build/example/system/<example_name>

# Example:
./build/example/system/system_process_comprehensive_example
```

## 📚 Example Categories

### 🔧 **Core System Operations**

Essential system functionality and information gathering.

| Example | Description | Features | Difficulty |
|---------|-------------|----------|------------|
| [`env_comprehensive.cpp`](env_comprehensive.cpp) | Environment variables and system info | Variable management, path operations, system detection | ⭐ Basic |
| [`user_management.cpp`](user_management.cpp) | User and group information | User queries, permissions, authentication | ⭐ Basic |
| [`stat_file_analysis.cpp`](stat_file_analysis.cpp) | File system statistics | File attributes, permissions, timestamps | ⭐ Basic |
| [`software_inventory.cpp`](software_inventory.cpp) | Software detection and management | Installed software, versions, updates | ⭐⭐ Intermediate |
| [`priority_management.cpp`](priority_management.cpp) | Process priority control | Priority classes, scheduling policies | ⭐⭐ Intermediate |

### ⚙️ **Process Management**

Process creation, monitoring, and control.

| Example | Description | Features | Difficulty |
|---------|-------------|----------|------------|
| [`process_basic.cpp`](process_basic.cpp) | Basic process operations | Process info, listing, basic control | ⭐ Basic |
| [`process_comprehensive.cpp`](process_comprehensive.cpp) | Advanced process management | Resource monitoring, detailed info, filtering | ⭐⭐ Intermediate |
| [`process_manager_advanced.cpp`](process_manager_advanced.cpp) | Process manager with lifecycle | Creation, monitoring, termination, output capture | ⭐⭐ Intermediate |
| [`pidwatcher_monitoring.cpp`](pidwatcher_monitoring.cpp) | Process monitoring and events | Real-time monitoring, callbacks, performance tracking | ⭐⭐⭐ Advanced |
| [`command_execution_suite.cpp`](command_execution_suite.cpp) | Command execution patterns | Sync/async execution, pipes, timeouts, streaming | ⭐⭐⭐ Advanced |

### 🔌 **Hardware Control**

Hardware interfaces and device management.

| Example | Description | Features | Difficulty |
|---------|-------------|----------|------------|
| [`gpio_basic.cpp`](gpio_basic.cpp) | Basic GPIO operations | Pin control, direction setting, value read/write | ⭐ Basic |
| [`gpio_advanced.cpp`](gpio_advanced.cpp) | Advanced GPIO features | PWM, interrupts, groups, debouncing | ⭐⭐⭐ Advanced |
| [`device_enumeration.cpp`](device_enumeration.cpp) | Device discovery | USB devices, serial ports, hardware detection | ⭐⭐ Intermediate |
| [`voltage_monitoring.cpp`](voltage_monitoring.cpp) | Power and voltage monitoring | Battery status, power sources, voltage readings | ⭐⭐ Intermediate |
| [`power_management.cpp`](power_management.cpp) | System power control | Shutdown, reboot, hibernate, power policies | ⭐⭐ Intermediate |

### 🌐 **Network Operations**

Network interface management and monitoring.

| Example | Description | Features | Difficulty |
|---------|-------------|----------|------------|
| [`network_basic.cpp`](network_basic.cpp) | Basic network operations | Interface listing, status, basic configuration | ⭐ Basic |
| [`network_manager_advanced.cpp`](network_manager_advanced.cpp) | Advanced network management | DNS management, monitoring, connection tracking | ⭐⭐ Intermediate |
| [`virtual_network.cpp`](virtual_network.cpp) | Virtual network interfaces | Virtual interface creation, configuration, management | ⭐⭐⭐ Advanced |
| [`network_monitoring.cpp`](network_monitoring.cpp) | Network performance monitoring | Traffic analysis, connection monitoring, statistics | ⭐⭐⭐ Advanced |

### 💾 **Storage Management**

Storage monitoring and file system operations.

| Example | Description | Features | Difficulty |
|---------|-------------|----------|------------|
| [`storage_basic.cpp`](storage_basic.cpp) | Basic storage operations | Space monitoring, mount detection, basic callbacks | ⭐ Basic |
| [`storage_advanced.cpp`](storage_advanced.cpp) | Advanced storage management | Real-time monitoring, media detection, path management | ⭐⭐ Intermediate |
| [`storage_performance.cpp`](storage_performance.cpp) | Storage performance analysis | I/O monitoring, performance metrics, optimization | ⭐⭐⭐ Advanced |

### 🛠️ **System Services**

System-level services and utilities.

| Example | Description | Features | Difficulty |
|---------|-------------|----------|------------|
| [`clipboard_operations.cpp`](clipboard_operations.cpp) | Clipboard management | Text, HTML, image formats, monitoring, callbacks | ⭐⭐ Intermediate |
| [`signal_handling.cpp`](signal_handling.cpp) | Signal processing | Signal monitoring, custom handlers, inter-process communication | ⭐⭐ Intermediate |
| [`debug_crash_handling.cpp`](debug_crash_handling.cpp) | Debug and crash management | Crash detection, stack traces, debug utilities | ⭐⭐⭐ Advanced |
| [`scheduling_crontab.cpp`](scheduling_crontab.cpp) | Task scheduling | Cron-like scheduling, task management, automation | ⭐⭐ Intermediate |

### 🖥️ **Platform-Specific Features**

Platform-specific functionality and integrations.

| Example | Description | Features | Difficulty |
|---------|-------------|----------|------------|
| [`registry_windows.cpp`](registry_windows.cpp) | Windows registry operations | Registry access, modification, monitoring (Windows) | ⭐⭐ Intermediate |
| [`registry_linux.cpp`](registry_linux.cpp) | Linux configuration management | Configuration files, system settings (Linux) | ⭐⭐ Intermediate |
| [`shortcut_detection.cpp`](shortcut_detection.cpp) | Keyboard shortcut detection | Global hotkeys, key combinations, event handling | ⭐⭐⭐ Advanced |
| [`platform_features.cpp`](platform_features.cpp) | Platform-specific capabilities | OS-specific features, compatibility layers | ⭐⭐ Intermediate |

### 🔗 **Integration Examples**

Real-world scenarios combining multiple components.

| Example | Description | Features | Difficulty |
|---------|-------------|----------|------------|
| [`system_monitor.cpp`](system_monitor.cpp) | Complete system monitoring | Multi-component monitoring, dashboard, alerts | ⭐⭐⭐ Advanced |
| [`process_automation.cpp`](process_automation.cpp) | Process automation suite | Automated process management, workflows, scheduling | ⭐⭐⭐ Advanced |
| [`hardware_diagnostics.cpp`](hardware_diagnostics.cpp) | Hardware diagnostic tool | Comprehensive hardware testing, reporting | ⭐⭐⭐ Advanced |
| [`network_diagnostics.cpp`](network_diagnostics.cpp) | Network diagnostic suite | Network testing, troubleshooting, analysis | ⭐⭐⭐ Advanced |
| [`error_handling_patterns.cpp`](error_handling_patterns.cpp) | Error handling best practices | Exception handling, recovery patterns, logging | ⭐⭐ Intermediate |

## 🎯 Learning Paths

### **Beginner Path** (New to System Programming)

1. [`env_comprehensive.cpp`](env_comprehensive.cpp) - Learn environment basics
2. [`user_management.cpp`](user_management.cpp) - Understand user context
3. [`process_basic.cpp`](process_basic.cpp) - Basic process operations
4. [`storage_basic.cpp`](storage_basic.cpp) - Storage fundamentals
5. [`network_basic.cpp`](network_basic.cpp) - Network basics

### **Intermediate Path** (Familiar with System Concepts)

1. [`process_comprehensive.cpp`](process_comprehensive.cpp) - Advanced process management
2. [`gpio_advanced.cpp`](gpio_advanced.cpp) - Hardware control
3. [`network_manager_advanced.cpp`](network_manager_advanced.cpp) - Network management
4. [`clipboard_operations.cpp`](clipboard_operations.cpp) - System services
5. [`error_handling_patterns.cpp`](error_handling_patterns.cpp) - Best practices

### **Advanced Path** (System Programming Expert)

1. [`pidwatcher_monitoring.cpp`](pidwatcher_monitoring.cpp) - Real-time monitoring
2. [`virtual_network.cpp`](virtual_network.cpp) - Virtual interfaces
3. [`debug_crash_handling.cpp`](debug_crash_handling.cpp) - Debug systems
4. [`system_monitor.cpp`](system_monitor.cpp) - Complete monitoring
5. [`hardware_diagnostics.cpp`](hardware_diagnostics.cpp) - Diagnostic tools

## 🔧 Platform Requirements

### **Windows**

- Windows 10 or later
- Visual Studio 2019+ or MinGW-w64
- Windows SDK for registry and system APIs
- Administrator privileges for hardware access

### **Linux**

- Linux kernel 3.10+ (for GPIO sysfs interface)
- GCC 7+ or Clang 6+
- Development packages: `build-essential`, `cmake`
- Root privileges for hardware and system operations
- Optional: `libusb-1.0-dev` for USB device enumeration

### **macOS**

- macOS 10.15+ (Catalina)
- Xcode 11+ or Command Line Tools
- Administrator privileges for system operations

## 🚨 Safety Warnings

### **Hardware Examples**

⚠️ **GPIO and Hardware Control**: GPIO examples can damage hardware if used incorrectly. Always:

- Verify pin assignments before running examples
- Use appropriate resistors and protection circuits
- Test on development boards, not production systems
- Understand electrical specifications of your hardware

### **System Operations**

⚠️ **Process and Power Management**: Some examples can affect system stability:

- Process termination examples may kill important processes
- Power management examples can shutdown/reboot the system
- Registry examples can modify system configuration
- Always test in safe environments first

### **Permissions**

⚠️ **Elevated Privileges**: Many examples require elevated privileges:

- Run as administrator/root only when necessary
- Understand the security implications
- Use principle of least privilege
- Review code before execution

## 📖 Example Documentation

Each example includes:

- **Header comments** explaining purpose and features
- **Inline documentation** for complex operations
- **Error handling** demonstrations
- **Platform-specific code** with appropriate guards
- **Safety checks** and validation
- **Performance considerations** where relevant

### Example Structure

```cpp
/**
 * @file example_name.cpp
 * @brief Brief description of the example
 *
 * Detailed description of what the example demonstrates,
 * including key features, use cases, and learning objectives.
 *
 * @warning Safety warnings if applicable
 * @note Platform-specific requirements
 * @author Atom Framework
 * @date 2024
 */

#include "atom/system/module.hpp"
#include <iostream>
#include <exception>

// Platform-specific includes
#ifdef _WIN32
    // Windows-specific headers
#elif defined(__linux__)
    // Linux-specific headers
#elif defined(__APPLE__)
    // macOS-specific headers
#endif

using namespace atom::system;

int main() {
    try {
        // Example implementation with error handling

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

## 🔍 Current Example Status

### ✅ **Enhanced Examples** (Comprehensive Coverage)

- `command.cpp` → `command_execution_suite.cpp` - Complete command execution patterns
- `env.cpp` → `env_comprehensive.cpp` - Full environment management
- `process.cpp` → `process_comprehensive.cpp` - Advanced process operations
- `process_manager.cpp` → `process_manager_advanced.cpp` - Complete lifecycle management
- `storage.cpp` → `storage_advanced.cpp` - Full storage monitoring
- `network_manager.cpp` → `network_manager_advanced.cpp` - Complete network management

### 🆕 **New Examples** (Previously Missing)

- `gpio_advanced.cpp` - Advanced GPIO with PWM, interrupts, groups
- `device_enumeration.cpp` - USB and serial device discovery
- `voltage_monitoring.cpp` - Power and voltage monitoring
- `power_management.cpp` - System power control
- `clipboard_operations.cpp` - Clipboard management all formats
- `signal_handling.cpp` - Signal processing and monitoring
- `debug_crash_handling.cpp` - Debug and crash management
- `scheduling_crontab.cpp` - Task scheduling and automation
- `registry_windows.cpp` - Windows registry operations
- `registry_linux.cpp` - Linux configuration management
- `shortcut_detection.cpp` - Keyboard shortcut detection
- `virtual_network.cpp` - Virtual network interfaces
- `system_monitor.cpp` - Multi-component monitoring
- `hardware_diagnostics.cpp` - Hardware diagnostic tools
- `error_handling_patterns.cpp` - Best practices and patterns

### 📋 **Legacy Examples** (Basic Demonstrations)

These examples are preserved for backward compatibility but superseded by enhanced versions:

- `gpio.cpp` - Basic GPIO operations (see `gpio_basic.cpp` and `gpio_advanced.cpp`)
- `user.cpp` - Basic user info (see `user_management.cpp`)
- `stat.cpp` - Basic file stats (see `stat_file_analysis.cpp`)
- `software.cpp` - Basic software info (see `software_inventory.cpp`)
- `priority.cpp` - Basic priority (see `priority_management.cpp`)
- `pidwatcher.cpp` - Basic monitoring (see `pidwatcher_monitoring.cpp`)
- `crash_quotes.cpp` - Basic crash handling (see `debug_crash_handling.cpp`)
- `crontab.cpp` - Basic scheduling (see `scheduling_crontab.cpp`)
- `lregistry.cpp` - Basic Linux registry (see `registry_linux.cpp`)
- `wregistry.cpp` - Basic Windows registry (see `registry_windows.cpp`)
- `signal.cpp` - Basic signals (see `signal_handling.cpp`)

## 🛠️ Building and Testing

### Build Configuration

```bash
# Enable all system examples
cmake -B build -S . -DATOM_EXAMPLE_BUILD_ALL=ON -DATOM_SYSTEM_EXAMPLES=ON

# Enable specific categories
cmake -B build -S . -DATOM_SYSTEM_PROCESS_EXAMPLES=ON
cmake -B build -S . -DATOM_SYSTEM_HARDWARE_EXAMPLES=ON
cmake -B build -S . -DATOM_SYSTEM_NETWORK_EXAMPLES=ON

# Debug build for development
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DATOM_ENABLE_DEBUG=ON
```

### Running Tests

```bash
# Run all system example tests
ctest --test-dir build -R "system_.*_test"

# Run specific category tests
ctest --test-dir build -R "system_process_.*_test"
ctest --test-dir build -R "system_hardware_.*_test"

# Run with verbose output
ctest --test-dir build -R "system_.*_test" --verbose
```

### Example Targets

Each example has a corresponding CMake target:

```bash
# Core system examples
cmake --build build --target system_env_comprehensive_example
cmake --build build --target system_user_management_example
cmake --build build --target system_process_comprehensive_example

# Hardware examples
cmake --build build --target system_gpio_advanced_example
cmake --build build --target system_device_enumeration_example
cmake --build build --target system_voltage_monitoring_example

# Network examples
cmake --build build --target system_network_manager_advanced_example
cmake --build build --target system_virtual_network_example

# Integration examples
cmake --build build --target system_system_monitor_example
cmake --build build --target system_hardware_diagnostics_example
```

## 📚 Additional Resources

### **Related Documentation**

- [Atom System Module API Reference](../../docs/system/README.md)
- [Platform-Specific Implementation Notes](../../docs/system/platform-notes.md)
- [Performance Optimization Guide](../../docs/system/performance.md)
- [Security Best Practices](../../docs/system/security.md)

### **External Resources**

- [Linux GPIO Sysfs Interface](https://www.kernel.org/doc/Documentation/gpio/sysfs.txt)
- [Windows System Programming](https://docs.microsoft.com/en-us/windows/win32/system-services)
- [POSIX Process Management](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_types.h.html)

### **Community**

- [GitHub Issues](https://github.com/ElementAstro/Atom/issues) - Bug reports and feature requests
- [Discussions](https://github.com/ElementAstro/Atom/discussions) - Questions and community support
- [Contributing Guide](../../CONTRIBUTING.md) - How to contribute examples and improvements

## 🤝 Contributing

We welcome contributions to improve and expand the system examples:

### **Adding New Examples**

1. Follow the [example structure](#example-structure) template
2. Include comprehensive documentation and error handling
3. Add appropriate safety warnings and platform checks
4. Create corresponding CMake targets and tests
5. Update this README with the new example information

### **Improving Existing Examples**

1. Enhance error handling and edge case coverage
2. Add performance optimizations and best practices
3. Improve documentation and inline comments
4. Add platform-specific optimizations
5. Ensure backward compatibility

### **Example Guidelines**

- **Safety First**: Include appropriate warnings and safety checks
- **Cross-Platform**: Support Windows, Linux, and macOS where applicable
- **Error Handling**: Demonstrate proper exception handling and recovery
- **Documentation**: Provide clear, comprehensive documentation
- **Testing**: Include unit tests and integration tests
- **Performance**: Consider performance implications and optimizations

## 📄 License

These examples are part of the Atom Framework and are licensed under the same terms as the main project. See [LICENSE](../../LICENSE) for details.

---

**Note**: This documentation is continuously updated. For the latest information, please check the [GitHub repository](https://github.com/ElementAstro/Atom) and the individual example files.
