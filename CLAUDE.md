# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Atom is a foundational C++23 library for astronomical software providing core utilities, algorithms, and system interfaces. The project is organized into modular components that can be built selectively.

## Build System

This project uses CMake as the primary build system with a unified Makefile interface.

### Common Build Commands

```bash
# Build entire project (default Release mode)
make build

# Build with different configurations
make debug                    # Debug build
make release                  # Release build
make python                   # Build with Python bindings
make all                      # Build everything (tests, examples, docs, Python)

# Testing
make test                     # Run all tests
make test-coverage           # Run tests with coverage analysis

# Development tools
make format                  # Format code with clang-format
make analyze                 # Run static analysis with clang-tidy
make clean                   # Clean build artifacts

# Single test execution (use ctest in build directory)
cd build && ctest -R <test_name> --output-on-failure
```

### CMake Build Options

Key configuration options:

- `ATOM_BUILD_TESTS=ON/OFF` - Build test suite
- `ATOM_BUILD_EXAMPLES=ON/OFF` - Build example programs  
- `ATOM_BUILD_PYTHON_BINDINGS=ON/OFF` - Build Python bindings
- `ATOM_BUILD_DOCS=ON/OFF` - Generate documentation
- `ATOM_BUILD_ALL=ON/OFF` - Build all modules
- `ATOM_BUILD_TESTS_SELECTIVE=ON/OFF` - Enable selective test building
- Individual module flags: `ATOM_BUILD_<MODULE>=ON/OFF` for ALGORITHM, ASYNC, etc.

### Selective Building

Use selective build options to build only specific modules:

```bash
cmake -DATOM_BUILD_ALL=OFF -DATOM_BUILD_ASYNC=ON -DATOM_BUILD_ALGORITHM=ON ..
```

## Architecture

### Core Modules

The library is organized into these primary modules:

- **algorithm** - Mathematical algorithms, cryptography, compression, pathfinding
- **async** - Asynchronous programming primitives (futures, promises, thread pools, message queues)
- **components** - Component system with dependency injection and registry
- **connection** - Network communication (TCP/UDP, FIFO, SSH clients/servers)
- **error** - Error handling, exception management, stack traces
- **image** - FITS file handling, image processing, OCR, SER format support
- **io** - File operations, compression, glob patterns, async I/O
- **log** - Logging infrastructure with async capabilities
- **memory** - Memory management utilities, pools, smart pointers
- **meta** - Template metaprogramming, reflection, type manipulation
- **search** - Search engines, caching (LRU, TTL), database interfaces
- **secret** - Encryption, password management, secure storage
- **serial** - Serial port communication, USB, Bluetooth interfaces
- **sysinfo** - System information (CPU, memory, disk, GPU, network)
- **system** - System utilities (processes, environment, crash handling, registry)
- **type** - Advanced type utilities (JSON, containers, string manipulation)
- **utils** - General utilities (time, conversion, validation, random generation)
- **web** - HTTP utilities, network addressing, time management

### Module Dependencies

The modules have interdependencies - check individual CMakeLists.txt files for specific requirements. Core modules like `error`, `type`, and `utils` are foundational dependencies for higher-level modules.

## Standards and Conventions

- **C++ Standard**: C++23 (CMAKE_CXX_STANDARD=23)
- **Coding Style**: Use `make format` to apply clang-format rules
- **Platform Support**: Linux (primary), Windows, macOS with platform-specific implementations
- **Dependencies**: See CMakeLists.txt for required packages (Asio, OpenSSL, SQLite3, fmt, etc.)

## Testing

- Tests are located in the `tests/` directory mirroring the module structure
- Use CTest for test execution: `cd build && ctest --parallel`
- Selective test building available via `ATOM_TEST_BUILD_<MODULE>` options
- Performance and benchmark tests available in tests/

## Development Environment

Required tools:

- CMake 3.21+
- C++23 compliant compiler
- Optional: clang-format, clang-tidy for code quality
- Platform-specific dependencies (X11 on Linux, etc.)

The build system auto-detects WSL environments and adjusts dependency handling accordingly.

## Continuous Integration

The project uses GitHub Actions for comprehensive multi-platform CI/CD with the following features:

### Supported Platforms
- **Linux**: Ubuntu 22.04 with GCC 12/13 and Clang 15/16
- **Windows**: MSVC 2022, MSYS2 MinGW64, and UCRT64 environments  
- **macOS**: Latest versions with Clang

### CI Features
- **Multi-compiler Support**: GCC, Clang, MSVC across different versions
- **MSYS2 Integration**: Full Windows MinGW64 support with native dependency management
- **Advanced Caching**: vcpkg dependencies, build artifacts, and ccache for faster builds
- **Test Matrix**: Debug/Release builds with sanitizers and coverage analysis
- **Python Wheels**: Multi-platform wheel generation for Python 3.9-3.12
- **Artifacts**: Automatic packaging (DEB, ZIP, MSI) and release deployment
- **Performance**: Benchmark execution and performance tracking

### Manual Workflow Triggers
Use GitHub's workflow_dispatch to trigger builds with custom parameters:
- Build type (Release/Debug/RelWithDebInfo)
- Enable/disable tests and examples
- Available in Actions tab of the repository

### CI Presets
The CI uses predefined CMake presets:
- `release`, `debug`, `relwithdebinfo` for standard builds
- `debug-full` for comprehensive testing with sanitizers
- `coverage` for code coverage analysis  
- `release-msys2`, `debug-msys2` for MSYS2 MinGW64 builds
- `release-vs`, `debug-vs` for Visual Studio builds
