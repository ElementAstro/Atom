# Component System Test Suite

This directory contains comprehensive unit tests for the Atom Component System. The test suite provides extensive coverage of all public APIs, performance benchmarks, integration tests, and error handling scenarios.

## Test Structure

### Core Component Tests (`test_component.cpp`)

- **Constructor/Destructor Testing**: Validates component creation and cleanup
- **State Management**: Tests component lifecycle states and transitions
- **Variable Management**: Comprehensive testing of variable registration, type safety, and constraints
- **Command Registration/Dispatch**: Tests function registration, member functions, and dispatch mechanisms
- **Performance Statistics**: Validates performance tracking and statistics collection
- **Event System**: Tests event emission and handling (if enabled)
- **Thread Safety**: Concurrent access and modification tests
- **Memory Management**: Resource cleanup and leak detection

### Registry Tests (`test_registry.cpp`)

- **Singleton Pattern**: Validates singleton implementation and thread safety
- **Component Registration**: Tests component registration and retrieval
- **Dependency Management**: Dependency resolution and circular dependency detection
- **Lifecycle Integration**: Component initialization and cleanup coordination
- **Hot Reloading**: Dynamic component loading and reloading
- **Memory Pool Integration**: Pool-based component creation
- **Error Handling**: Invalid operations and edge cases
- **Performance**: Mass registration and retrieval benchmarks

### Component Pool Tests (`test_component_pool.cpp`)

- **Memory Pool Operations**: Allocation, deallocation, and pool management
- **Cache Alignment**: Memory alignment verification and performance
- **Statistics Tracking**: Pool usage statistics and monitoring
- **SIMD Container Operations**: Vectorized component processing
- **Fragmentation Handling**: Memory fragmentation detection and cleanup
- **Thread Safety**: Concurrent allocation and deallocation
- **Performance Benchmarks**: Pool vs standard allocation comparisons

### Lifecycle Management Tests (`test_lifecycle.cpp`)

- **Hook Registration**: Lifecycle hook registration and execution
- **Dependency Resolution**: Topological sorting and dependency chains
- **Circular Dependency Detection**: Detection and handling of circular dependencies
- **Event History**: Lifecycle event tracking and history management
- **Validation Mechanisms**: Dependency validation and constraint checking
- **Error Handling**: Hook execution errors and recovery

### Serialization Tests (`test_serialization.cpp`)

- **JSON Serialization**: JSON format serialization and deserialization
- **Binary Serialization**: Compact binary format support
- **Versioning**: Schema versioning and compatibility
- **Compression**: Data compression and decompression
- **Encryption**: Data encryption and decryption (if enabled)
- **Error Handling**: Corrupted data and invalid format handling
- **Performance**: Serialization speed and memory usage benchmarks

### Scripting API Tests (`test_scripting_api.cpp`)

- **Script Execution**: Lua, Python, and ChaiScript execution
- **Type Conversion**: Automatic type conversion between C++ and script types
- **Function Registration**: C++ function registration and calling from scripts
- **Global Variables**: Script global variable management
- **Component Integration**: Script access to component system
- **Error Handling**: Script syntax and runtime error handling
- **Memory Management**: Script memory limits and cleanup
- **Performance**: Script execution speed benchmarks

### Variable Manager Tests (`test_variable_manager.cpp`)

- **Variable Registration**: Type-safe variable registration and retrieval
- **Range Constraints**: Numeric range validation and enforcement
- **String Options**: String enumeration validation
- **Member Variable Binding**: Direct binding to C++ class members
- **Trackable Behavior**: Change notification and callback systems
- **Type Safety**: Type validation and error handling
- **Complex Data Types**: Vector, map, and custom type support

### Dispatch System Tests (`test_dispatch.cpp`)

- **Function Registration**: Lambda, function pointer, and member function registration
- **Argument Conversion**: Automatic type conversion for function arguments
- **Return Value Handling**: Type-safe return value processing
- **Function Overloading**: Multiple functions with same name, different signatures
- **Error Propagation**: Exception handling and error reporting
- **Performance Optimization**: Fast dispatch mechanisms and benchmarks
- **Thread Safety**: Concurrent function registration and execution

### Integration Tests (`test_integration.cpp`)

- **Component Interactions**: Multi-component communication and data flow
- **Real-world Scenarios**: Practical usage patterns and workflows
- **System Load Testing**: High-load scenarios and stress testing
- **Cross-system Integration**: Serialization, scripting, and lifecycle integration
- **Performance Benchmarks**: End-to-end system performance measurement
- **Memory Usage**: Large-scale memory usage and cleanup testing

## Building and Running Tests

### Prerequisites

- CMake 3.16 or higher
- Google Test framework
- C++20 compatible compiler
- Optional: Lua, Python development libraries for scripting tests

### Build Commands

```bash
# Configure build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# Build tests
cmake --build build --target atom_component_tests

# Run all tests
cd build && ctest --verbose

# Or run test executable directly
./build/atom_component_tests
```

### Test Categories

#### Run Specific Test Categories

```bash
# Core component functionality
./atom_component_tests --gtest_filter="ComponentTest.*"

# Registry and dependency management
./atom_component_tests --gtest_filter="RegistryTest.*"

# Memory pool and performance
./atom_component_tests --gtest_filter="ComponentPoolTest.*"

# Scripting integration
./atom_component_tests --gtest_filter="ScriptingAPITest.*"

# Integration and end-to-end tests
./atom_component_tests --gtest_filter="IntegrationTest.*"

# Performance benchmarks only
./atom_component_tests --gtest_filter="*PerformanceTest.*"

# Error handling tests
./atom_component_tests --gtest_filter="*ErrorTest.*"
```

#### Custom CMake Targets

```bash
# Run all tests
make run_tests

# Run performance benchmarks
make run_benchmarks

# Run scripting tests
make run_scripting_tests

# Run core functionality tests
make run_core_tests

# Run integration tests
make run_integration_tests

# Run error handling tests
make run_error_tests
```

### Memory Leak Detection

```bash
# Run with Valgrind (Linux only)
make run_tests_valgrind

# Check Valgrind output
cat valgrind-out.txt
```

### Code Coverage

```bash
# Generate coverage report (GCC Debug build)
make coverage

# View coverage report
open coverage_html/index.html
```

## Test Configuration

### Compile-time Options

- `ATOM_ENABLE_LUA`: Enable Lua scripting tests
- `ATOM_ENABLE_PYTHON`: Enable Python scripting tests
- `ENABLE_EVENT_SYSTEM`: Enable event system tests
- `INSTALL_TESTS`: Install test executable

### Environment Variables

- `GTEST_COLOR`: Enable colored test output
- `GTEST_BRIEF`: Brief test output format
- `GTEST_FILTER`: Filter tests by pattern

## Performance Benchmarks

The test suite includes comprehensive performance benchmarks that measure:

- **Component Creation**: Time to create and initialize components
- **Variable Access**: Variable get/set operation speed
- **Command Dispatch**: Function call overhead and execution time
- **Memory Pool**: Allocation/deallocation performance vs standard allocators
- **Serialization**: JSON/Binary serialization speed and compression ratios
- **Script Execution**: Scripting engine performance and overhead
- **Concurrent Access**: Multi-threaded performance and scalability

### Benchmark Results

Benchmark results are printed to stdout during test execution and can be used to:

- Identify performance regressions
- Compare different implementation approaches
- Validate optimization efforts
- Establish performance baselines

## Test Data

The `test_data/` directory contains:

- Sample component configurations
- Test scripts for scripting engines
- Serialization test data
- Performance test datasets

## Contributing

When adding new tests:

1. **Follow Naming Conventions**: Use descriptive test names that clearly indicate what is being tested
2. **Test Categories**: Group related tests in appropriate test fixtures
3. **Error Scenarios**: Include both positive and negative test cases
4. **Performance Tests**: Add benchmarks for performance-critical functionality
5. **Documentation**: Update this README when adding new test categories
6. **Thread Safety**: Include concurrent access tests for shared resources

### Test Quality Guidelines

- **Comprehensive Coverage**: Test all public APIs and edge cases
- **Isolated Tests**: Each test should be independent and repeatable
- **Clear Assertions**: Use descriptive assertion messages
- **Performance Awareness**: Include timing assertions for critical paths
- **Resource Cleanup**: Ensure proper cleanup in test fixtures
- **Cross-platform**: Tests should work on Windows, Linux, and macOS

## Troubleshooting

### Common Issues

1. **Missing Dependencies**: Ensure all required libraries are installed
2. **Compilation Errors**: Check C++20 compiler compatibility
3. **Test Failures**: Run individual test categories to isolate issues
4. **Memory Issues**: Use Valgrind or AddressSanitizer for debugging
5. **Performance Variations**: Run benchmarks multiple times for stable results

### Debug Builds

Debug builds include additional assertions and memory checking that may affect performance benchmarks but provide better error detection.

### Release Builds

Release builds provide accurate performance measurements but may hide certain types of errors due to optimizations.
