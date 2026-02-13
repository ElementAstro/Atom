# Atom Project Overview

## Project Purpose

Atom is a foundational C++20/C++23 library for astronomical software development. It provides a comprehensive, modular framework with 18+ specialized domains designed for high-performance applications in astronomy, image processing, and system integration.

**Version:** 0.1.0
**License:** GPL-3.0
**Homepage:** <https://github.com/ElementAstro/Atom>

## Tech Stack

### Core Technologies

- **Language:** C++20 (C++23 features when available)
- **Build System:** CMake 3.21+ (primary), XMake (supported)
- **C++ Compilers:** GCC 11+, Clang 12+, MSVC 2022+
- **Python:** 3.8+ (for bindings and tests)

### Key Dependencies

- **spdlog** - Logging framework (compiled library)
- **fmt** - Formatting library (via spdlog)
- **OpenSSL** - Cryptographic operations
- **GoogleTest** - Unit testing framework
- **pybind11** - Python bindings (optional)

### Optional Dependencies

- **OpenCV** - Computer vision for image module
- **CFITSIO** - FITS format support for image module
- **Tesseract/Leptonica** - OCR capabilities for image module
- **Boost** - High-performance containers for containers module
- **ASIO** - Async I/O for connection and io modules
- **ZLIB/minizip-ng** - Compression for io and utils modules
- **SQLite/MySQL** - Cache storage for search module
- **libusb-1.0** - USB device support for system module

## Codebase Structure

```
Atom/
├── atom/                   # Core library modules (19 modules)
│   ├── algorithm/          # Math & crypto algorithms
│   ├── async/              # Async primitives
│   ├── components/         # Component system
│   ├── connection/         # Networking & IPC
│   ├── containers/         # Lock-free queues, graph helpers
│   ├── error/              # Error handling with stack traces
│   ├── image/              # Image processing (FITS/SER/OCR)
│   ├── io/                 # File operations, compression
│   ├── log/                # Async logging framework
│   ├── memory/             # Memory pools, arenas
│   ├── meta/               # Reflection, type traits
│   ├── search/             # LRU/TTL caches
│   ├── secret/             # Security helpers
│   ├── serial/             # Serial communication
│   ├── sysinfo/            # System introspection
│   ├── system/             # Process management, integration
│   ├── type/               # Type utilities, variant/any
│   ├── utils/              # General utilities
│   └── web/                # HTTP client, URL tools
├── tests/                  # GoogleTest test suite
├── example/                # Usage examples
├── python/                 # pybind11 Python bindings
├── scripts/                # Build and utility scripts
├── cmake/                  # CMake modules
└── extra/                  # Third-party libraries
```

## Module Dependencies

Modules have explicit dependencies declared in `cmake/ModuleDependenciesData.cmake`:

- **Core Modules** (no dependencies): error, type, containers
- **Low-Level Modules** depend on core modules
- **Mid-Level Modules** depend on low-level modules
- **High-Level Modules** depend on mid-level modules

Use `ATOM_AUTO_RESOLVE_DEPS=ON` to automatically enable required modules.

## Cross-Platform Support

- **Windows:** MSVC and MSYS2 MinGW64
- **Linux:** GCC and Clang
- **macOS:** Clang

Platform-specific code isolated in relevant modules.
