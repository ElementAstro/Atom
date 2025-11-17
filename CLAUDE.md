# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Atom** is a foundational C++ library for astronomical software development. It provides a comprehensive set of modules for algorithmic operations, image processing, system integration, and more. The project is designed as a modular framework with optional components that can be selectively built based on requirements.

## Build System

### Primary Build Commands

The project uses CMake as the primary build system with enhanced build scripts:

- **Standard build**: `./scripts/build.sh` (Linux/macOS) or `scripts\build.bat` (Windows)
- **Debug build**: `./scripts/build.sh --debug`
- **Release with tests**: `./scripts/build.sh --release --tests --run-tests`
- **Build with Python bindings**: `./scripts/build.sh --python`
- **Build examples**: `./scripts/build.sh --examples`
- **Clean build**: `./scripts/build.sh --clean`

### Build System Features

The enhanced build scripts support:

- Multiple build types (debug, release, relwithdebinfo)
- Parallel compilation with automatic CPU detection
- System dependency installation
- Package creation for distribution
- Documentation generation with Doxygen
- Cross-platform support (Linux, macOS, Windows)

### Module-Based Building

You can selectively build modules using CMake options:

- `ATOM_BUILD_ALGORITHM=ON/OFF` - Algorithm and mathematical operations
- `ATOM_BUILD_IMAGE=ON/OFF` - Image processing and computer vision
- `ATOM_BUILD_ASYNC=ON/OFF` - Asynchronous operations
- `ATOM_BUILD_CONNECTION=ON/OFF` - Network and communication
- `ATOM_BUILD_ERROR=ON/OFF` - Error handling system
- `ATOM_BUILD_UTILS=ON/OFF` - Utility functions
- And more...

### Testing

- **Build tests**: `./scripts/build.sh --tests`
- **Run tests**: `./scripts/build.sh --tests --run-tests`
- **Run specific test categories**: Use CMake targets like `test_core_modules`, `test_io_modules`

## Architecture

### Module Structure

Atom is organized into modular components under `atom/`:

- **algorithm/**: Mathematical algorithms, cryptography, signal processing
- **image/**: Complete image processing pipeline with astronomical format support (FITS, SER)
- **async/**: Asynchronous programming primitives and concurrency utilities
- **connection/**: Network communication (TCP, UDP, SSH)
- **error/**: Comprehensive error handling and stack trace system
- **io/**: Input/output operations and file system utilities
- **system/**: System-level integration and platform-specific code
- **utils/**: General utility functions and helpers
- **web/**: HTTP client and web-related utilities

### Key Dependencies

- **OpenSSL**: Cryptographic operations (algorithm module)
- **OpenCV**: Computer vision and image processing (image module)
- **CFITSIO**: FITS file format support (image module, optional)
- **Tesseract**: OCR capabilities (image module, optional)
- **spdlog**: Logging framework
- **GTest**: Unit testing framework

### Python Bindings

Python bindings are available for most modules using pybind11:

- Enable with `--python` flag or `ATOM_BUILD_PYTHON_BINDINGS=ON`
- Bindings are located in `python/` directory
- Each module has corresponding Python binding files

## Development Workflow

### Adding New Code

1. **Module Selection**: Add code to appropriate module under `atom/`
2. **Dependencies**: Update `CMakeLists.txt` for any new dependencies
3. **Headers**: Place public headers in module root, implementation in subdirectories
4. **Tests**: Add corresponding tests under `tests/[module]/`
5. **Examples**: Consider adding examples under `example/[module]/`

### Build Options

The project supports extensive configuration via CMake options and command-line flags. Check the main `CMakeLists.txt` for complete list of available options.

### Error Handling

Atom uses a comprehensive error handling system centered in the `error` module. All modules should integrate with this system for consistent error reporting and stack trace generation.

## Important Notes

- **vcpkg**: Currently disabled due to network issues, but configuration is available
- **C++ Standard**: Uses C++20 by default, C++23 when available
- **Platform Support**: Windows (MSVC), Linux (GCC/Clang), macOS (Clang)
- **Modular Design**: Each module can be built independently to reduce binary size
- **Astronomical Focus**: Specialized support for astronomical image formats and processing

## Common Development Tasks

### Building a Single Module

```bash
cmake -B build -DATOM_BUILD_ALGORITHM=ON -DATOM_BUILD_TESTS=ON
cmake --build build
```

### Running Specific Tests

```bash
cd build
ctest -R "algorithm_*" --output-on-failure
```

### Building with All Features

```bash
./scripts/build.sh --release --python --examples --tests --docs --package
```
