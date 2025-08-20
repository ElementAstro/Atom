# Atom Web Module

This directory contains the web and networking components for the Atom framework.

## Directory Structure

The web module has been refactored to follow a clean, organized structure:

```
atom/web/
├── CMakeLists.txt              # CMake build configuration
├── xmake.lua                   # XMake build configuration
├── README.md                   # This file
├── [compatibility headers]     # Backward compatibility headers (deprecated)
├── address/                    # Network address handling
│   ├── CMakeLists.txt         # Address module build config
│   ├── address.hpp            # Base address interface
│   ├── address.cpp            # Address factory implementation
│   ├── ipv4.hpp               # IPv4 address implementation
│   ├── ipv4.cpp               # IPv4 address implementation
│   ├── ipv6.hpp               # IPv6 address implementation
│   ├── ipv6.cpp               # IPv6 address implementation
│   ├── unix_domain.hpp        # Unix domain socket addresses
│   ├── unix_domain.cpp        # Unix domain socket implementation
│   └── main.hpp               # Unified address interface
├── http/                       # HTTP client functionality
│   ├── curl.hpp               # CURL wrapper and HTTP client
│   ├── curl.cpp               # CURL implementation
│   ├── downloader.hpp         # Download manager
│   ├── downloader.cpp         # Download manager implementation
│   ├── httpparser.hpp         # HTTP header parser
│   └── httpparser.cpp         # HTTP parser implementation
├── mime/                       # MIME type handling
│   ├── minetype.hpp           # MIME type detection and handling
│   └── minetype.cpp           # MIME type implementation
├── time/                       # Time management utilities
│   ├── CMakeLists.txt         # Time module build config
│   ├── xmake.lua              # Time module XMake config
│   ├── time_manager.hpp       # Time management interface
│   ├── time_manager.cpp       # Time manager implementation
│   ├── time_manager_impl.hpp  # Time manager implementation details
│   ├── time_manager_impl.cpp  # Time manager implementation
│   ├── time_utils.hpp         # Time utility functions
│   ├── time_utils.cpp         # Time utilities implementation
│   └── time_error.hpp         # Time-related error definitions
└── utils/                      # Network utility functions
    ├── common.hpp             # Common network definitions
    ├── addr_info.hpp          # Address info utilities
    ├── addr_info.cpp          # Address info implementation
    ├── dns.hpp                # DNS resolution utilities
    ├── dns.cpp                # DNS implementation
    ├── ip.hpp                 # IP address utilities
    ├── ip.cpp                 # IP utilities implementation
    ├── network.hpp            # Network connectivity utilities
    ├── network.cpp            # Network implementation
    ├── port.hpp               # Port scanning and utilities
    ├── port.cpp               # Port utilities implementation
    ├── socket.hpp             # Socket utilities
    └── socket.cpp             # Socket implementation
```

## Backward Compatibility

All existing header file paths continue to work without modification. The root-level headers are now compatibility headers that forward to the new locations:

- `curl.hpp` → `http/curl.hpp`
- `downloader.hpp` → `http/downloader.hpp`
- `httpparser.hpp` → `http/httpparser.hpp`
- `minetype.hpp` → `mime/minetype.hpp`
- `address.hpp` → `address/` (includes all address types)
- `time.hpp` → `time/` (includes all time utilities)
- `utils.hpp` → `utils/` (includes all network utilities)

## Migration Guide

### For New Code

Use the new structured paths:

```cpp
#include "atom/web/http/curl.hpp"
#include "atom/web/address/ipv4.hpp"
#include "atom/web/time/time_manager.hpp"
```

### For Existing Code

No changes required! Existing includes will continue to work:

```cpp
#include "atom/web/curl.hpp"        // Still works
#include "atom/web/downloader.hpp"  // Still works
#include "atom/web/address.hpp"     // Still works
```

## Key Components

### HTTP Client Functionality

- **CURL Wrapper**: Modern C++ wrapper around libcurl with RAII and exception safety
- **Download Manager**: Multi-threaded download manager with progress tracking
- **HTTP Parser**: C++20 HTTP header parser with comprehensive feature support

### Network Address Handling

- **IPv4/IPv6**: Full support for IPv4 and IPv6 address parsing and validation
- **Unix Domain Sockets**: Support for Unix domain socket addresses
- **Address Factory**: Automatic address type detection and creation

### MIME Type Support

- **MIME Detection**: File extension to MIME type mapping
- **Content Type Handling**: HTTP content type parsing and generation

### Time Management

- **Time Manager**: High-precision time management with multiple clock sources
- **Time Utilities**: Date/time parsing, formatting, and conversion utilities
- **Error Handling**: Comprehensive time-related error definitions

### Network Utilities

- **DNS Resolution**: Asynchronous DNS resolution with caching
- **Port Scanning**: Network port scanning and connectivity testing
- **Socket Utilities**: Low-level socket operations and management
- **Network Detection**: Internet connectivity and network interface detection

## Build System

The module supports both CMake and XMake build systems. The build files have been updated to reflect the new directory structure while maintaining compatibility.

### Dependencies

- **Core**: C++20 compiler support, loguru (logging)
- **HTTP**: libcurl for HTTP client functionality
- **Network**: Platform-specific networking libraries (Winsock on Windows)
- **Optional**: Boost.Asio for enhanced networking features

## Features

### HTTP Client

- Modern C++ interface to libcurl
- Automatic memory management and cleanup
- Support for various HTTP methods and authentication
- Progress callbacks and timeout handling

### Address Management

- Type-safe address handling with polymorphic design
- Automatic address type detection
- Comprehensive validation and error handling
- Cross-platform compatibility

### Network Utilities

- High-performance DNS resolution
- Port scanning with timeout control
- Network connectivity monitoring
- Socket-level operations

## Notes

This refactoring maintains 100% backward compatibility while providing a cleaner, more maintainable codebase structure that follows established patterns from other Atom modules. The existing subdirectories (address, time, utils) were preserved as they already followed good organizational principles, while root-level files were moved into appropriate new subdirectories (http, mime).
