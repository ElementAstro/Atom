# Atom Framework Examples Build Guide

This guide provides comprehensive instructions for building and running Atom framework examples across different platforms and configurations.

## 🚀 Quick Start

### Prerequisites
- **CMake 3.20+**
- **C++20 compatible compiler**
  - GCC 10+ / Clang 12+ / MSVC 2019+
- **Git** (for cloning dependencies)

### Basic Build Process
```bash
# 1. Configure with examples enabled
cmake -B build -S . -DATOM_EXAMPLE_BUILD_ALL=ON

# 2. Build all examples
cmake --build build

# 3. Run a specific example
./build/example/containers/containers_high_performance_containers_example.exe
```

## 🛠️ Detailed Build Configuration

### CMake Configuration Options

#### **Example Build Control**
```bash
# Build all examples
-DATOM_EXAMPLE_BUILD_ALL=ON

# Build specific modules
-DATOM_EXAMPLE_BUILD_CONTAINERS=ON
-DATOM_EXAMPLE_BUILD_META=ON
-DATOM_EXAMPLE_BUILD_SECRET=ON
-DATOM_EXAMPLE_BUILD_SYSINFO=ON

# Disable specific modules
-DATOM_EXAMPLE_BUILD_ALGORITHM=OFF
```

#### **Build Type Configuration**
```bash
# Debug build (default)
-DCMAKE_BUILD_TYPE=Debug

# Release build (optimized)
-DCMAKE_BUILD_TYPE=Release

# Release with debug info
-DCMAKE_BUILD_TYPE=RelWithDebInfo
```

#### **Compiler-Specific Options**
```bash
# GCC/Clang
-DCMAKE_CXX_COMPILER=g++
-DCMAKE_CXX_FLAGS="-Wall -Wextra -std=c++20"

# MSVC
-DCMAKE_CXX_COMPILER=cl
-DCMAKE_CXX_FLAGS="/std:c++20 /W4"
```

### Platform-Specific Configuration

#### **Windows (MinGW/MSYS2)**
```bash
# Configure for MinGW
cmake -B build -S . -G "MinGW Makefiles" -DATOM_EXAMPLE_BUILD_ALL=ON

# Build
cmake --build build

# Run examples
.\build\example\containers\containers_high_performance_containers_example.exe
```

#### **Windows (Visual Studio)**
```bash
# Configure for Visual Studio
cmake -B build -S . -G "Visual Studio 16 2019" -DATOM_EXAMPLE_BUILD_ALL=ON

# Build
cmake --build build --config Release

# Run examples
.\build\example\containers\Release\containers_high_performance_containers_example.exe
```

#### **Linux**
```bash
# Configure
cmake -B build -S . -DATOM_EXAMPLE_BUILD_ALL=ON

# Build with parallel jobs
cmake --build build -j$(nproc)

# Run examples
./build/example/containers/containers_high_performance_containers_example
```

#### **macOS**
```bash
# Configure
cmake -B build -S . -DATOM_EXAMPLE_BUILD_ALL=ON

# Build
cmake --build build -j$(sysctl -n hw.ncpu)

# Run examples
./build/example/containers/containers_high_performance_containers_example
```

## 🎯 Building Specific Examples

### Individual Module Building
```bash
# Build only containers examples
cmake -B build -S . -DATOM_EXAMPLE_BUILD_CONTAINERS=ON
cmake --build build

# Build only meta examples
cmake -B build -S . -DATOM_EXAMPLE_BUILD_META=ON
cmake --build build
```

### Individual Target Building
```bash
# List available targets
cmake --build build --target help | grep example

# Build specific target
cmake --build build --target containers_high_performance_containers_example
cmake --build build --target meta_comprehensive_meta_example
cmake --build build --target secret_basic_test
```

### Parallel Building
```bash
# Use all available cores
cmake --build build -j

# Use specific number of cores
cmake --build build -j4

# Windows parallel build
cmake --build build --parallel 4
```

## 📊 Example Status and Build Requirements

### ✅ **Fully Working Examples**
These examples build and run successfully:

```bash
# Containers module
cmake --build build --target containers_high_performance_containers_example
./build/example/containers/containers_high_performance_containers_example.exe

# Meta module
cmake --build build --target meta_comprehensive_meta_example
./build/example/meta/meta_comprehensive_meta_example.exe

# Secret module (basic test)
cmake --build build --target secret_basic_test
./build/example/secret/secret_basic_test.exe

# Sysinfo module (header test)
cmake --build build --target sysinfo_header_test
./build/example/sysinfo/sysinfo_header_test.exe
```

### 🔧 **Examples with Known Issues**
These examples build but may have runtime issues:

```bash
# Algorithm examples (some have runtime crashes)
cmake --build build --target algorithm_md5
# ./build/example/algorithm/algorithm_md5.exe  # May crash

# Secret examples (dependency issues)
cmake --build build --target secret_secure_storage_example
# ./build/example/secret/secret_secure_storage_example.exe  # May crash

# Sysinfo examples (initialization issues)
cmake --build build --target sysinfo_basic_sysinfo_example
# ./build/example/sysinfo/sysinfo_basic_sysinfo_example.exe  # May crash
```

## 🔍 Troubleshooting Build Issues

### Common Build Problems

#### **1. CMake Configuration Errors**
```
Error: CMake 3.20 or higher is required
```
**Solution**: Update CMake to version 3.20 or higher

#### **2. Compiler Compatibility**
```
Error: C++20 features not supported
```
**Solution**: Use a C++20 compatible compiler:
- GCC 10+, Clang 12+, MSVC 2019+

#### **3. Missing Dependencies**
```
Error: Could not find required package
```
**Solution**: Install missing dependencies or disable optional features

#### **4. Linking Errors**
```
Error: undefined reference to 'symbol'
```
**Solution**: Check module dependencies and linking configuration

### Platform-Specific Issues

#### **Windows Issues**
```bash
# Path length issues
git config --system core.longpaths true

# MinGW path issues
export PATH="/mingw64/bin:$PATH"

# Visual Studio generator issues
cmake -B build -S . -G "Visual Studio 16 2019" -A x64
```

#### **Linux Issues**
```bash
# Missing development packages
sudo apt-get install build-essential cmake git

# Library path issues
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

#### **macOS Issues**
```bash
# Xcode command line tools
xcode-select --install

# Homebrew dependencies
brew install cmake git
```

## 🎨 Build Customization

### Custom Build Configurations

#### **Development Build**
```bash
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Debug \
  -DATOM_EXAMPLE_BUILD_ALL=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

#### **Performance Testing Build**
```bash
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DATOM_EXAMPLE_BUILD_ALL=ON \
  -DCMAKE_CXX_FLAGS="-O3 -DNDEBUG -march=native"
```

#### **Minimal Build**
```bash
cmake -B build -S . \
  -DATOM_EXAMPLE_BUILD_CONTAINERS=ON \
  -DATOM_EXAMPLE_BUILD_META=ON \
  -DATOM_EXAMPLE_BUILD_SECRET=OFF \
  -DATOM_EXAMPLE_BUILD_SYSINFO=OFF
```

### Dependency Management

#### **Optional Dependencies**
```bash
# Boost (for enhanced container features)
-DBOOST_ROOT=/path/to/boost

# OpenSSL (for cryptographic features)
-DOPENSSL_ROOT_DIR=/path/to/openssl

# Custom library paths
-DCMAKE_PREFIX_PATH="/path/to/libs1;/path/to/libs2"
```

#### **Dependency Fallbacks**
The build system automatically handles missing optional dependencies:
- Boost containers → Standard library containers
- OpenSSL → Built-in implementations
- Platform-specific libraries → Generic implementations

## 📈 Build Performance Optimization

### Compilation Speed
```bash
# Use ccache (if available)
-DCMAKE_CXX_COMPILER_LAUNCHER=ccache

# Parallel compilation
cmake --build build -j$(nproc)

# Ninja generator (faster than Make)
cmake -B build -S . -G Ninja -DATOM_EXAMPLE_BUILD_ALL=ON
ninja -C build
```

### Build Size Optimization
```bash
# Strip debug symbols in release
-DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -s"

# Link-time optimization
-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

## 🧪 Testing and Validation

### Build Verification
```bash
# Build all examples
cmake --build build

# Run working examples to verify
./build/example/containers/containers_high_performance_containers_example.exe
./build/example/meta/meta_comprehensive_meta_example.exe
./build/example/secret/secret_basic_test.exe
./build/example/sysinfo/sysinfo_header_test.exe
```

### Automated Testing
```bash
# Future: CTest integration
ctest --test-dir build --output-on-failure
```

## 📚 Additional Resources

### Build System Documentation
- **CMake Documentation**: https://cmake.org/documentation/
- **Atom Framework Build System**: See main project CMakeLists.txt
- **Module-Specific Build**: Check individual module CMakeLists.txt files

### Platform-Specific Guides
- **Windows Development**: Visual Studio and MinGW setup guides
- **Linux Development**: Package manager and dependency installation
- **macOS Development**: Xcode and Homebrew setup

---

This build guide ensures consistent and successful compilation of Atom framework examples across all supported platforms and configurations.
