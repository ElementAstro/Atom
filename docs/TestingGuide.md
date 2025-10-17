# Atom Testing Guide

## Overview

The Atom project features a comprehensive, unified testing infrastructure that provides consistent test execution across all modules. This guide explains how to use, extend, and maintain the testing system.

## Architecture

### Test Infrastructure Components

1. **Core Test Framework** (`atom/tests/test.hpp`) - Provides test utilities, assertions, and execution framework
2. **Test Runner** (`atom/tests/test_runner.hpp`) - Advanced test runner with parallel execution and reporting
3. **Unified Test Runner** (`tests/run_all_tests.cpp`) - Centralized entry point for all tests
4. **Standard Test Templates** (`tests/cmake/`) - Consistent CMake configuration for all modules
5. **Test Infrastructure** (`tests/tests/`) - Common testing utilities and validation

### Module Structure

```
tests/
├── CMakeLists.txt              # Main test configuration
├── run_all_tests.cpp           # Unified test runner executable
├── cmake/
│   ├── StandardTestTemplate.cmake    # Standardized test configuration function
│   └── ModuleTestTemplate.cmake      # Simplified template for module tests
├── tests/
│   └── CMakeLists.txt          # Test infrastructure configuration
├── algorithm/                  # Algorithm module tests
├── async/                      # Async module tests
├── components/                 # Components module tests
├── connection/                 # Connection module tests
├── containers/                 # Containers module tests
├── error/                      # Error module tests
├── extra/                      # Extra utilities tests
├── image/                      # Image module tests
├── io/                         # IO module tests
├── log/                        # Log module tests
├── memory/                     # Memory module tests
├── meta/                       # Metaprogramming tests
├── search/                     # Search module tests
├── secret/                     # Cryptographic tests
├── serial/                     # Serial communication tests
├── sysinfo/                    # System information tests
├── system/                     # System integration tests
├── type/                       # Type system tests
├── utils/                      # Utilities tests
└── web/                        # Web utilities tests
```

## Quick Start

### Running All Tests

```bash
# Using the unified test runner
./run_all_tests

# Using CTest (traditional method)
ctest --output-on-failure

# Using build script
./scripts/build.sh --tests --run-tests
```

### Running Specific Module Tests

```bash
# Using unified test runner
./run_all_tests --module=error

# Using CTest
ctest -L error --output-on-failure

# Using custom target
make test_error  # or ninja test_error
```

### Running Tests by Category

```bash
# Using unified test runner
./run_all_tests --category=unit

# Using CTest
ctest -L unit --output-on-failure
```

### Running Tests with Filtering

```bash
# Filter by pattern
./run_all_tests --filter=".*socket.*"

# Verbose output
./run_all_tests --verbose

# Parallel execution
./run_all_tests --parallel --threads=8

# Export results
./run_all_tests --output-format=json --output=test_results.json
```

## Test Execution Options

### Unified Test Runner Commands

The `run_all_tests` executable provides comprehensive test execution options:

```bash
Usage: run_all_tests [OPTIONS]

OPTIONS:
    -h, --help                    Show help message
    -l, --list                    List all available test modules
    -v, --verbose                 Enable verbose output
    -p, --parallel [N]            Run tests in parallel
    -m, --module <name>           Run tests from specific module
    -c, --category <name>         Run tests from specific category
    -f, --filter <pattern>        Run tests matching regex pattern
    --output-format <format>      Output format (json, xml, html, text)
    -o, --output <path>           Output file path
    --retry <N>                   Retry failed tests N times
    --fail-fast                   Stop on first failure
    --shuffle                     Shuffle test execution order
    --seed <N>                    Random seed for shuffling
    --timeout <MS>                Global timeout in milliseconds
```

### CMake Test Targets

```bash
# Run all tests
make test_all_modules

# Run specific categories
make test_core_modules     # error, log, meta, type, utils
make test_io_modules       # io, image, serial
make test_system_modules   # system, sysinfo
make test_network_modules  # web, connection

# Run individual module tests
make test_algorithm
make test_async
make test_error
# ... etc for all modules

# Performance tests
make test_algorithm_performance
make test_async_performance
# ... etc

# Unified runner
make test_unified
```

## Adding Tests to Modules

### For Regular Modules (with .cpp files)

1. Create test files in `tests/<module>/` following naming convention `test_<feature>.cpp`
2. Use GoogleTest framework for assertions:

   ```cpp
   #include <gtest/gtest.h>
   #include "atom/yourmodule/header.hpp"

   TEST(YourModuleTest, BasicFunctionality) {
       // Your test code
       EXPECT_EQ(your_function(), expected_value);
   }
   ```

3. Update the module's `CMakeLists.txt` using the standardized template:

   ```cmake
   # =============================================================================
   # Standardized Test Configuration for Atom <Module> Module
   # =============================================================================
   cmake_minimum_required(VERSION 3.20)
   project(atom-<module>-tests VERSION 1.0.0 LANGUAGES C CXX)

   set(MODULE_NAME "<module>")
   set(MODULE_DEPENDENCIES "dependency1;dependency2")
   set(EXTRA_LIBRARIES "library1;library2")
   set(HEADER_ONLY OFF)

   include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/StandardTestTemplate.cmake)

   file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
   configure_standard_module_tests(${MODULE_NAME} "${TEST_SOURCES}")
   ```

### For Header-Only Modules

1. Create test headers in `tests/<module>/` following naming convention `test_<feature>.hpp`
2. Use the header-only template configuration:

   ```cmake
   set(MODULE_NAME "<module>")
   set(HEADER_ONLY ON)
   include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/StandardTestTemplate.cmake)
   configure_standard_module_tests(${MODULE_NAME} "")
   ```

### Using the Atom Test Framework

The project provides an enhanced test framework with additional features:

```cpp
#include "atom/tests/test.hpp"

// Using the enhanced test framework
TEST(AdvancedTest, Example) {
    // Standard assertions
    expect(your_condition);
    expect_eq(value1, value2);
    expect_ne(value1, value2);
    expect_gt(value1, value2);
    expect_lt(value1, value2);
    expect_contains(string, substring);
    expect_throws([]() { throw std::runtime_error("test"); });

    // Floating point comparison
    expect_approx(double1, double2, 0.001);

    // Set equality for vectors
    expect_set_eq(vector1, vector2);
}

// Using custom test registration
"Custom Test Name"_test([]() {
    // Test implementation
});

// Using test suite builder
auto suite = atom::test::TestSuiteBuilder("MyTestSuite")
    .addTest("Test1", []() { /* test code */ })
    .addTest("Test2", []() { /* test code */ });
```

## Test Categories and Labels

### Standard Labels

- `unit` - Unit tests
- `integration` - Integration tests
- `performance` - Performance benchmarks
- `stress` - Stress tests
- `<module>` - Module-specific label (e.g., `error`, `async`, `algorithm`)

### Custom Labels

You can add custom labels to tests using CTest properties:

```cmake
set_tests_properties(your_test_name PROPERTIES
    LABELS "custom_label;module_name"
)
```

## Continuous Integration

### GitHub Actions Configuration

```yaml
name: Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2

    - name: Configure and Build
      run: |
        cmake -B build -DATOM_BUILD_TESTS=ON
        cmake --build build --parallel

    - name: Run Tests
      run: |
        cd build
        ./run_all_tests --verbose --output-format=json --output=test_results.json

    - name: Upload Test Results
      uses: actions/upload-artifact@v2
      with:
        name: test-results
        path: build/test_results.json
```

## Best Practices

### Test Organization

1. **One test file per feature** - Keep tests focused and organized
2. **Descriptive test names** - Use clear, descriptive names that explain what's being tested
3. **Arrange-Act-Assert pattern** - Structure tests with clear setup, execution, and verification
4. **Independent tests** - Ensure tests don't depend on each other

### Test Coverage

1. **Aim for high coverage** - Target 80%+ code coverage for critical modules
2. **Test edge cases** - Don't just test happy paths
3. **Error conditions** - Test error handling and edge cases
4. **Integration points** - Test module interactions

### Performance Testing

1. **Benchmark critical paths** - Identify and performance-test critical algorithms
2. **Regression testing** - Track performance over time
3. **Resource usage** - Monitor memory and CPU usage
4. **Consistent environment** - Run performance tests in consistent environments

### CI/CD Integration

1. **Fast feedback** - Run quick tests first, slower tests later
2. **Parallel execution** - Use parallel test execution to reduce CI time
3. **Test artifacts** - Save test results and coverage reports
4. **Failure notifications** - Set up proper failure notifications

## Troubleshooting

### Common Issues

1. **Tests not discovered**: Check CMakeLists.txt configuration and ensure `enable_testing()` is called
2. **Linking errors**: Verify all required libraries are linked and module dependencies are correct
3. **Timeout failures**: Increase timeout values for long-running tests
4. **Missing dependencies**: Ensure all required external libraries are available

### Debugging Test Failures

1. **Verbose output**: Use `--verbose` flag for detailed test execution information
2. **Individual test execution**: Run specific tests to isolate failures
3. **Debug builds**: Use debug builds with additional assertions
4. **Logging**: Add logging to test code to trace execution

### Performance Issues

1. **Parallel execution**: Enable parallel test execution
2. **Test ordering**: Use `--shuffle` to identify test dependency issues
3. **Resource monitoring**: Monitor system resources during test execution
4. **Test profiling**: Profile slow tests to identify bottlenecks

## Migration Guide

### Migrating Existing Tests

1. **Update CMakeLists.txt**: Replace existing configuration with standardized template
2. **Update includes**: Change to use `atom/tests/test.hpp` for enhanced features
3. **Update assertions**: Optionally use enhanced assertion macros
4. **Test verification**: Ensure all tests still pass after migration

### Adding New Modules

1. **Create test directory**: `tests/<module>/`
2. **Create CMakeLists.txt**: Use the standardized template
3. **Add test files**: Follow naming conventions and best practices
4. **Update main CMakeLists.txt**: Add module condition to main test CMakeLists.txt

## Advanced Features

### Custom Test Runners

You can create custom test runners by extending the base framework:

```cpp
#include "atom/tests/test_runner.hpp"

class CustomTestRunner : public atom::test::TestRunner {
public:
    CustomTestRunner() : TestRunner() {
        // Custom configuration
    }

    void runCustomTests() {
        // Custom test execution logic
    }
};
```

### Test Reporting

Generate comprehensive test reports in multiple formats:

```bash
# JSON report
./run_all_tests --output-format=json --output=report.json

# XML report (for CI systems)
./run_all_tests --output-format=xml --output=report.xml

# HTML report
./run_all_tests --output-format=html --output=report.html
```

### Test Sharding

For large test suites, you can shard tests across multiple machines:

```bash
# Shard 1 of 4
./run_all_tests --shard-index=0 --shard-total=4

# Shard 2 of 4
./run_all_tests --shard-index=1 --shard-total=4
# ... etc
```

## Reference

### CMake Functions

- `configure_standard_module_tests()` - Configures standardized test setup for a module
- `add_atom_test()` - Creates a standard test executable
- `add_atom_performance_test()` - Creates a performance test executable
- `add_atom_stress_test()` - Creates a stress test executable

### Test Macros

- `expect(condition)` - Basic assertion
- `expect_eq(lhs, rhs)` - Equality assertion
- `expect_ne(lhs, rhs)` - Inequality assertion
- `expect_gt(lhs, rhs)` - Greater than assertion
- `expect_lt(lhs, rhs)` - Less than assertion
- `expect_contains(str, substr)` - String contains assertion
- `expect_throws(expr)` - Exception assertion
- `expect_approx(lhs, rhs, epsilon)` - Floating point approximation

### Environment Variables

- `ATOM_TEST_FILTER` - Default test filter pattern
- `ATOM_TEST_PARALLEL` - Enable parallel execution by default
- `ATOM_TEST_THREADS` - Default thread count for parallel execution
- `ATOM_TEST_OUTPUT_DIR` - Default output directory for test reports

This guide provides comprehensive information for working with the Atom testing infrastructure. For specific implementation details, refer to the source code and existing test examples.
