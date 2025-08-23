# Atom Framework Examples

This directory contains comprehensive examples demonstrating the capabilities of the Atom framework. Each module has its own subdirectory with examples showcasing different features and use cases.

## 🚀 Quick Start

### Building Examples

To build all examples:
```bash
# Configure with examples enabled
cmake -B build -S . -DATOM_EXAMPLE_BUILD_ALL=ON

# Build all examples
cmake --build build

# Or build specific examples
cmake --build build --target <example_target>
```

### Running Examples

After building, examples are located in `build/example/<module>/`:
```bash
# Run a specific example
./build/example/<module>/<example_name>.exe

# Example:
./build/example/containers/containers_high_performance_containers_example.exe
```

## 📁 Module Examples

### ✅ **Containers Module** (`example/containers/`)
**Status**: Fully functional ✅

- **`high_performance_containers_example.cpp`**: Comprehensive demonstration of high-performance containers
  - Flat containers (flat_map, flat_set) with better cache locality
  - Small containers (small_vector) with stack optimization
  - Lock-free containers for concurrent programming
  - Performance comparisons and benchmarks
  - Boost container integration with fallbacks

**Build & Run**:
```bash
cmake --build build --target containers_high_performance_containers_example
./build/example/containers/containers_high_performance_containers_example.exe
```

### ✅ **Meta Module** (`example/meta/`)
**Status**: Fully functional ✅

- **`comprehensive_meta_example.cpp`**: Complete metaprogramming and reflection demonstration
  - Type information and introspection using TypeInfo
  - Function traits and signature analysis
  - BoxedValue (Any) for type-safe runtime value storage
  - Template metaprogramming with SFINAE
  - Standard library type traits integration

**Build & Run**:
```bash
cmake --build build --target meta_comprehensive_meta_example
./build/example/meta/meta_comprehensive_meta_example.exe
```

### 🔧 **Secret Module** (`example/secret/`)
**Status**: Basic test working ✅, comprehensive examples have runtime issues

- **`basic_test.cpp`**: Basic module loading and functionality test ✅
- **`secure_storage_example.cpp`**: Comprehensive secure storage demonstration (has runtime issues)
- **`simple_secret_example.cpp`**: Simplified secure storage example (has runtime issues)

**Build & Run**:
```bash
# Working basic test
cmake --build build --target secret_basic_test
./build/example/secret/secret_basic_test.exe
```

### 🔧 **Sysinfo Module** (`example/sysinfo/`)
**Status**: Header test working ✅, runtime examples have dependency issues

- **`header_test.cpp`**: Basic header inclusion test ✅
- **`basic_sysinfo_example.cpp`**: Basic system information gathering (has runtime issues)
- **`system_info_example.cpp`**: Comprehensive system information demo (has linking issues)

**Build & Run**:
```bash
# Working header test
cmake --build build --target sysinfo_header_test
./build/example/sysinfo/sysinfo_header_test.exe
```

### 📚 **Algorithm Module** (`example/algorithm/`)
**Status**: Many examples available, some enhanced

- **Hash algorithms**: MD5, SHA1, general hashing
- **Mathematical algorithms**: Matrix operations, optimization, signal processing
- **Data structures**: Big numbers, fractions, Huffman coding
- **Graphics**: Perlin noise, pathfinding, flood fill
- **Utilities**: Base64 encoding, pattern matching

**Build & Run**:
```bash
# Example: Enhanced MD5 demonstration
cmake --build build --target algorithm_md5
./build/example/algorithm/algorithm_md5.exe
```

### ⚡ **Async Module** (`example/async/`)
**Status**: Comprehensive examples available

- **Concurrency**: Thread pools, executors, futures, promises
- **Synchronization**: Locks, mutexes, thread-safe types
- **Messaging**: Message queues, event systems, pub-sub
- **Timers**: Scheduling, delays, periodic tasks
- **Utilities**: Thread wrappers, parallel algorithms

**Build & Run**:
```bash
# Example: Timer functionality
cmake --build build --target async_timer
./build/example/async/async_timer.exe
```

### 🌐 **Web Module** (`example/web/`)
**Status**: HTTP and networking examples

- **HTTP**: Client/server implementations, parsing
- **Networking**: Address handling, MIME types
- **Utilities**: Time handling, web utilities
- **CURL integration**: HTTP requests and responses

### 🔍 **Search Module** (`example/search/`)
**Status**: Caching and database examples

- **Caching**: LRU cache, TTL cache, general caching strategies
- **Database**: SQLite integration, search algorithms
- **Core**: Search engine components

### 💾 **Memory Module** (`example/memory/`)
**Status**: Memory management examples

- **Memory pools**: Custom allocators, object pools
- **Smart pointers**: Shared pointers, memory tracking
- **Utilities**: Memory utilities, ring buffers

## 🛠️ Build System Integration

### CMake Configuration

The examples are integrated into the main CMake build system:

- **`example/CMakeLists.txt`**: Main examples configuration
- **`cmake/ExamplesBuildOptions.cmake`**: Build options for examples
- **Individual module CMakeLists.txt**: Per-module example configuration

### Build Options

- **`ATOM_EXAMPLE_BUILD_ALL=ON`**: Build all examples
- **`ATOM_EXAMPLE_BUILD_<MODULE>=ON`**: Build specific module examples
- **`ATOM_EXAMPLE_BUILD_<MODULE>=OFF`**: Disable specific module examples

### Dependencies

Examples automatically handle dependencies:
- **Required dependencies**: Automatically linked
- **Optional dependencies**: Graceful fallbacks when missing
- **Platform-specific**: Conditional compilation for different platforms

## 🎯 Example Categories

### **Beginner Examples**
- Basic functionality demonstrations
- Simple API usage
- Getting started guides

### **Intermediate Examples**
- Feature combinations
- Real-world use cases
- Performance considerations

### **Advanced Examples**
- Complex integrations
- Performance optimization
- Edge cases and error handling

## 📖 Documentation Standards

Each example follows these standards:

### **File Structure**
```cpp
/**
 * @file example_name.cpp
 * @brief Brief description of what the example demonstrates
 *
 * This example shows how to:
 * - Feature 1
 * - Feature 2
 * - Feature 3
 *
 * @author Author Name
 * @date Date
 */

#include "required/headers.hpp"

// Demonstration functions
void demonstrateFeature1() { /* ... */ }
void demonstrateFeature2() { /* ... */ }

int main() {
    // Main demonstration
    return 0;
}
```

### **Documentation Requirements**
- **Header comments**: File purpose and feature list
- **Function documentation**: What each function demonstrates
- **Inline comments**: Explain complex operations
- **Error handling**: Proper exception handling
- **Output formatting**: Clear, readable output

### **Code Quality**
- **Modern C++**: Use C++20 features appropriately
- **Best practices**: Follow Atom framework conventions
- **Error handling**: Graceful failure modes
- **Performance**: Demonstrate efficient usage patterns

## 🔧 Troubleshooting

### Common Issues

1. **Build Failures**
   - Ensure all dependencies are installed
   - Check CMake configuration
   - Verify compiler compatibility

2. **Runtime Crashes**
   - Some examples have known runtime issues
   - Check module dependencies
   - Verify platform compatibility

3. **Missing Features**
   - Some features require optional dependencies
   - Check build configuration
   - Review module documentation

### Getting Help

- **Documentation**: Check individual module README files
- **Source Code**: Examples are well-commented
- **Build System**: Review CMakeLists.txt files for configuration

## 🎉 Contributing

When adding new examples:

1. **Follow naming conventions**: `module_feature_example.cpp`
2. **Add CMake targets**: Update CMakeLists.txt
3. **Document thoroughly**: Include comprehensive comments
4. **Test functionality**: Ensure examples build and run
5. **Update documentation**: Add to relevant README files

---

**Note**: This examples collection demonstrates the comprehensive capabilities of the Atom framework. While some examples may have runtime issues due to dependency requirements, the working examples provide excellent demonstrations of the framework's features and capabilities.
