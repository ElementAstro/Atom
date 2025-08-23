# Atom Library Build Guide

This comprehensive guide covers building the Atom library from source using various build systems and configurations.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Quick Start](#quick-start)
- [Build Systems](#build-systems)
- [Build Options](#build-options)
- [Platform-Specific Instructions](#platform-specific-instructions)
- [Advanced Configuration](#advanced-configuration)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### System Requirements

- **C++ Compiler**: GCC 9+, Clang 10+, or MSVC 2019+
- **C++ Standard**: C++20 or later
- **CMake**: 3.21 or later
- **Git**: For version control and dependency management

### Required Dependencies

- **OpenSSL**: Cryptographic library
- **ZLIB**: Compression library
- **SQLite3**: Database engine
- **fmt**: Formatting library

### Optional Dependencies

- **Python 3.8+**: For Python bindings
- **pybind11**: Python binding generator
- **Doxygen**: For documentation generation
- **Boost**: For advanced data structures (optional)
- **CFITSIO**: For astronomical data formats
- **libssh**: For SSH connectivity

## Quick Start

### Using the Enhanced Build Script

The easiest way to build Atom is using our enhanced build scripts:

```bash
# Linux/macOS
./build.sh --release --examples --tests --python

# Windows
build.bat --release --examples --tests --python
```

### Manual CMake Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DATOM_BUILD_EXAMPLES=ON \
    -DATOM_BUILD_TESTS=ON \
    -DATOM_BUILD_PYTHON_BINDINGS=ON

# Build
cmake --build build --parallel

# Install (optional)
sudo cmake --install build
```

## Build Systems

### CMake (Recommended)

CMake is the primary build system with full feature support:

```bash
# Basic configuration
cmake -B build -DCMAKE_BUILD_TYPE=Release

# With all features
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DATOM_BUILD_EXAMPLES=ON \
    -DATOM_BUILD_TESTS=ON \
    -DATOM_BUILD_PYTHON_BINDINGS=ON \
    -DATOM_BUILD_DOCS=ON \
    -DATOM_USE_BOOST=ON

# Build
cmake --build build --config Release --parallel
```

### XMake (Alternative)

XMake provides a simpler configuration syntax:

```bash
# Configure
xmake config -m release --build_examples=y --build_tests=y --build_python=y

# Build
xmake build

# Install
xmake install
```

## Build Options

### Core Options

| Option | Description | Default |
|--------|-------------|---------|
| `ATOM_BUILD_EXAMPLES` | Build example applications | ON |
| `ATOM_BUILD_TESTS` | Build test suite | OFF |
| `ATOM_BUILD_PYTHON_BINDINGS` | Build Python bindings | OFF |
| `ATOM_BUILD_DOCS` | Generate documentation | OFF |
| `BUILD_SHARED_LIBS` | Build shared libraries | OFF |

### Module Options

Control which modules to build:

```cmake
-DATOM_BUILD_ALGORITHM=ON    # Algorithm utilities
-DATOM_BUILD_ASYNC=ON        # Async programming
-DATOM_BUILD_COMPONENTS=ON   # Component system
-DATOM_BUILD_CONNECTION=ON   # Network/IPC
-DATOM_BUILD_CONTAINERS=ON   # Container utilities
-DATOM_BUILD_ERROR=ON        # Error handling
-DATOM_BUILD_IMAGE=ON        # Image processing
-DATOM_BUILD_IO=ON           # Input/output
-DATOM_BUILD_LOG=ON          # Logging
-DATOM_BUILD_MEMORY=ON       # Memory management
-DATOM_BUILD_META=ON         # Metaprogramming
-DATOM_BUILD_SEARCH=ON       # Search algorithms
-DATOM_BUILD_SECRET=ON       # Cryptography
-DATOM_BUILD_SERIAL=ON       # Serial communication
-DATOM_BUILD_SYSINFO=ON      # System information
-DATOM_BUILD_SYSTEM=ON       # System utilities
-DATOM_BUILD_TYPE=ON         # Type utilities
-DATOM_BUILD_UTILS=ON        # General utilities
-DATOM_BUILD_WEB=ON          # Web utilities
```

### Feature Options

```cmake
-DATOM_USE_BOOST=ON              # Enable Boost support
-DATOM_USE_BOOST_LOCKFREE=ON     # Boost lockfree structures
-DATOM_USE_BOOST_CONTAINER=ON    # Boost containers
-DATOM_USE_BOOST_GRAPH=ON        # Boost graph library
-DATOM_USE_CFITSIO=ON            # CFITSIO support
-DATOM_USE_SSH=ON                # SSH support
```

## Platform-Specific Instructions

### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake ninja-build \
    libssl-dev zlib1g-dev libsqlite3-dev \
    libfmt-dev libreadline-dev \
    python3-dev python3-pip \
    doxygen graphviz

# Build with dependency installation
./build.sh --install-deps --release --examples --tests --python
```

### Linux (CentOS/RHEL/Fedora)

```bash
# Install dependencies
sudo dnf install -y \
    gcc-c++ cmake ninja-build \
    openssl-devel zlib-devel sqlite-devel \
    fmt-devel readline-devel \
    python3-devel python3-pip \
    doxygen graphviz

# Build
./build.sh --release --examples --tests --python
```

### macOS

```bash
# Install dependencies with Homebrew
brew install cmake ninja openssl zlib sqlite3 fmt readline python3 doxygen graphviz

# Build
./build.sh --release --examples --tests --python
```

### Windows

#### Using vcpkg (Recommended)

```cmd
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Install dependencies
.\vcpkg install openssl zlib sqlite3 fmt readline pybind11 --triplet x64-windows

# Build
build.bat --release --examples --tests --python
```

#### Using Visual Studio

1. Open Visual Studio 2019/2022
2. File → Open → CMake → Select `CMakeLists.txt`
3. Configure CMake settings in `CMakeSettings.json`
4. Build → Build All

## Advanced Configuration

### Using vcpkg

```bash
# Setup vcpkg
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh

# Configure with vcpkg
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux \
    -DUSE_VCPKG=ON
```

### Cross-Compilation

```bash
# For ARM64
cmake -B build-arm64 \
    -DCMAKE_TOOLCHAIN_FILE=toolchains/arm64-linux.cmake \
    -DCMAKE_BUILD_TYPE=Release

# For Windows from Linux
cmake -B build-windows \
    -DCMAKE_TOOLCHAIN_FILE=toolchains/mingw-w64.cmake \
    -DCMAKE_BUILD_TYPE=Release
```

### Custom Installation Prefix

```bash
cmake -B build \
    -DCMAKE_INSTALL_PREFIX=/opt/atom \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build
sudo cmake --install build
```

### Development Build

```bash
# Debug build with all features
./build.sh --debug --examples --tests --python --docs --verbose

# With sanitizers
cmake -B build-debug \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address -fsanitize=undefined" \
    -DATOM_BUILD_TESTS=ON

cmake --build build-debug
cd build-debug && ctest
```

## Build Script Options

The enhanced build scripts support many options:

### Build Types
- `--debug`: Debug build with symbols
- `--release`: Optimized release build (default)
- `--relwithdebinfo`: Release with debug info

### Features
- `--python`: Enable Python bindings
- `--shared`: Build shared libraries
- `--examples`: Build examples
- `--tests`: Build and optionally run tests
- `--docs`: Generate documentation

### Build Management
- `--clean`: Clean build directory
- `--install-deps`: Install system dependencies
- `--package`: Create distribution packages
- `--run-tests`: Build and run tests
- `--jobs N`: Use N parallel jobs
- `--prefix PATH`: Set installation prefix
- `--verbose`: Enable verbose output
- `--dry-run`: Show commands without executing

### Examples

```bash
# Full development build
./build.sh --debug --clean --install-deps --examples --tests --run-tests --docs --verbose

# Release build with packaging
./build.sh --release --examples --python --package --jobs 8

# Quick test build
./build.sh --debug --tests --run-tests --jobs $(nproc)
```

## Troubleshooting

### Common Issues

#### Missing Dependencies
```bash
# Error: Could not find OpenSSL
sudo apt-get install libssl-dev  # Ubuntu/Debian
sudo dnf install openssl-devel   # CentOS/RHEL/Fedora
brew install openssl             # macOS
```

#### CMake Version Too Old
```bash
# Install newer CMake
pip install cmake --upgrade
# Or use snap on Ubuntu
sudo snap install cmake --classic
```

#### C++20 Support Issues
```bash
# Ensure modern compiler
gcc --version  # Should be 9+
clang --version  # Should be 10+

# Set compiler explicitly
export CC=gcc-10
export CXX=g++-10
```

#### Python Binding Issues
```bash
# Install pybind11
pip install pybind11

# Set Python path
export PYTHONPATH=$PWD/build/python:$PYTHONPATH
```

### Build Performance

#### Parallel Building
```bash
# Use all CPU cores
cmake --build build --parallel $(nproc)

# Limit parallel jobs
cmake --build build --parallel 4
```

#### Ninja Generator
```bash
# Use Ninja for faster builds
cmake -B build -G Ninja
ninja -C build
```

#### ccache
```bash
# Install ccache for faster rebuilds
sudo apt-get install ccache
export CC="ccache gcc"
export CXX="ccache g++"
```

### Getting Help

- **Documentation**: Check `docs/` directory
- **Issues**: Report bugs on GitHub
- **Discussions**: Use GitHub Discussions for questions
- **Build Logs**: Check `logs/build_*.log` files

For more detailed information, see:
- [CI/CD Guide](CI_CD_GUIDE.md)
- [Distribution Guide](DISTRIBUTION_GUIDE.md)
- [Development Guide](DEVELOPMENT_GUIDE.md)
