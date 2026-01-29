# System Module Examples

This directory contains comprehensive examples for the `atom/system` module, organized to mirror the source code structure.

## Quick Start

### Build All Examples

```bash
cmake -B build -DATOM_EXAMPLE_BUILD_ALL=ON
cmake --build build
```

### Run an Example

```bash
# Windows
.\build\example\system\system_clipboard_clipboard_example.exe

# Linux/macOS
./build/example/system/system_clipboard_clipboard_example
```

## Directory Structure

The examples are organized into subdirectories that match `atom/system/`:

```text
example/system/
├── clipboard/          # Clipboard operations (text, binary, monitoring)
├── core/              # Core system functionality (priority management)
├── debug/             # Debugging and crash handling
├── hardware/          # Hardware interaction (devices, GPIO, voltage)
├── info/              # System information (env, software, stats, users)
├── network/           # Network management
├── power/             # Power management
├── process/           # Process management
├── registry/          # Registry operations (Windows/Linux)
├── scheduling/        # Task scheduling
├── shortcut/          # Keyboard shortcut detection
├── signals/           # Signal handling
└── storage/           # Storage monitoring
```

## Examples by Category

### Clipboard (1 example)

- **clipboard_example.cpp** - Text operations, binary data, change monitoring

### Core (1 example)

- **priority_example.cpp** - Process/thread priority and CPU affinity

### Debug (3 examples)

- **crash_example.cpp** - Crash logging with system information
- **crash_quotes_example.cpp** - Crash message handling
- **nodebugger_example.cpp** - Anti-debugging and debugger detection

### Hardware (3 examples)

- **device_example.cpp** - USB, serial, Bluetooth device enumeration
- **gpio_example.cpp** - GPIO operations and control
- **voltage_example.cpp** - Voltage reading and monitoring

### Info (4 examples)

- **env_example.cpp** - Environment variable operations
- **software_example.cpp** - Software detection
- **stat_example.cpp** - File and system statistics
- **user_example.cpp** - User information

### Network (2 examples)

- **network_manager_example.cpp** - Network interface management
- **virtual_network_example.cpp** - Virtual network and statistics

### Power (1 example)

- **power_example.cpp** - Shutdown, reboot, hibernate, sleep

### Process (4 examples)

- **command_example.cpp** - Command execution
- **pidwatcher_example.cpp** - Process monitoring
- **process_example.cpp** - Process creation and control
- **process_manager_example.cpp** - Process pool management

### Registry (2 examples)

- **lregistry_example.cpp** - Linux configuration (Linux only)
- **wregistry_example.cpp** - Windows registry (Windows only)

### Scheduling (1 example)

- **crontab_example.cpp** - Cron-like task scheduling

### Shortcut (1 example)

- **shortcut_detector_example.cpp** - Keyboard shortcut detection (Windows only)

### Signals (1 example)

- **signal_example.cpp** - Signal handling and management

### Storage (1 example)

- **storage_example.cpp** - Storage monitoring and disk usage

## Platform Support

- **Cross-Platform**: Most examples work on Windows, Linux, and macOS
- **Windows-Only**: `shortcut_detector_example`, `wregistry_example`
- **Linux-Only**: `lregistry_example`

## Documentation

- [EXAMPLES_SUMMARY.md](EXAMPLES_SUMMARY.md) - Detailed examples overview
- [REORGANIZATION_SUMMARY.md](REORGANIZATION_SUMMARY.md) - Reorganization details and component mapping

## Coverage

✅ **100% Coverage** - All public-facing components in `atom/system/` have corresponding examples.

Total: **25 example files** covering **13 modules**
