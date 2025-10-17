# Atom Serial Module

This directory contains the serial communication components for the Atom framework.

## Directory Structure

The serial module has been refactored to follow a clean, organized structure:

```
atom/serial/
├── CMakeLists.txt              # CMake build configuration
├── xmake.lua                   # XMake build configuration
├── README.md                   # This file
├── [compatibility headers]     # Backward compatibility headers (deprecated)
├── core/                       # Core serial functionality
│   ├── scanner.hpp            # Serial port scanner
│   ├── scanner.cpp            # Scanner implementation
│   ├── serial_port.hpp        # Main serial port interface
│   └── serial_port.cpp        # Serial port implementation
├── bluetooth/                  # Bluetooth serial communication
│   ├── bluetooth_serial.hpp   # Main Bluetooth serial interface
│   ├── bluetooth_serial.cpp   # Bluetooth serial implementation
│   ├── bluetooth_serial_win.hpp    # Windows Bluetooth implementation
│   ├── bluetooth_serial_unix.hpp   # Unix/Linux Bluetooth implementation
│   ├── bluetooth_serial_mac.hpp    # macOS Bluetooth interface
│   └── bluetooth_serial_mac.mm     # macOS Bluetooth implementation (Objective-C++)
├── usb/                        # USB device communication
│   ├── usb.hpp                # USB communication interface
│   └── usb.cpp                # USB implementation using libusb
└── platform/                  # Platform-specific implementations
    ├── serial_port_win.hpp    # Windows serial port implementation
    └── serial_port_unix.hpp   # Unix/Linux/macOS serial port implementation
```

## Backward Compatibility

All existing header file paths continue to work without modification. The root-level headers are now compatibility headers that forward to the new locations:

- `scanner.hpp` → `core/scanner.hpp`
- `serial_port.hpp` → `core/serial_port.hpp`
- `bluetooth_serial.hpp` → `bluetooth/bluetooth_serial.hpp`
- `usb.hpp` → `usb/usb.hpp`
- `serial_port_win.hpp` → `platform/serial_port_win.hpp`
- `serial_port_unix.hpp` → `platform/serial_port_unix.hpp`
- `bluetooth_serial_*.hpp` → `bluetooth/bluetooth_serial_*.hpp`

## Migration Guide

### For New Code

Use the new structured paths:

```cpp
#include "atom/serial/core/serial_port.hpp"
#include "atom/serial/bluetooth/bluetooth_serial.hpp"
#include "atom/serial/usb/usb.hpp"
```

### For Existing Code

No changes required! Existing includes will continue to work:

```cpp
#include "atom/serial/serial_port.hpp"      // Still works
#include "atom/serial/bluetooth_serial.hpp" // Still works
#include "atom/serial/usb.hpp"              // Still works
```

## Key Components

### Core Serial Functionality

- **SerialPort**: Main serial port communication class with cross-platform support
- **SerialPortScanner**: Utility for discovering and enumerating available serial ports

### Bluetooth Communication

- **BluetoothSerial**: Bluetooth serial communication with platform-specific implementations
- Platform-specific implementations for Windows, Unix/Linux, and macOS

### USB Communication

- **UsbContext/UsbDevice**: USB device communication using libusb-1.0
- Asynchronous operations with C++20 coroutine support
- Hotplug detection capabilities

### Platform Support

- **Windows**: Native Windows API implementation
- **Unix/Linux**: POSIX-compliant implementation with udev support
- **macOS**: IOKit framework integration with Objective-C++ support

## Build System

The module supports both CMake and XMake build systems. The build files have been updated to reflect the new directory structure while maintaining compatibility.

### Dependencies

- **Core**: C++20 compiler support, loguru (logging)
- **Windows**: SetupAPI, Cfgmgr32, BluetoothApis
- **Unix/Linux**: libudev, libusb-1.0, bluez (optional)
- **macOS**: IOKit, Foundation, IOBluetooth frameworks

## Features

### Serial Port Scanner

- Cross-platform serial port enumeration
- CH340 device detection
- Performance monitoring and statistics
- Caching and background monitoring support

### Serial Port Communication

- Asynchronous read/write operations
- Configurable baud rates, data bits, stop bits, parity
- Flow control support (RTS/CTS, XON/XOFF)
- Timeout handling and error recovery

### Bluetooth Serial

- Device discovery and pairing
- RFCOMM channel communication
- Connection management and monitoring
- Platform-specific optimizations

### USB Communication

- libusb-1.0 integration
- Asynchronous transfer support
- Device hotplug detection
- C++20 coroutine-based API

## Notes

This refactoring maintains 100% backward compatibility while providing a cleaner, more maintainable codebase structure that follows established patterns from other Atom modules. The platform-specific implementations are properly isolated, making the codebase easier to maintain and extend.
