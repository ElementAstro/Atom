# Atom Components Test Suite

This directory contains comprehensive tests for the `atom/components` module, providing complete test coverage for all major functionality.

## Test Structure

### Core Infrastructure Tests
- **component_pool.cpp** - Memory pool management, threading, statistics
- **lifecycle.cpp** - Component lifecycle phases, dependency resolution, hooks
- **serialization.cpp** - JSON/Binary serialization, versioning, error handling

### Scripting System Tests
- **scripting_api.cpp** - Script value conversion, API registration, engine management
- **script_sandbox.cpp** - Security sandboxing, resource limits, isolation
- **advanced_bindings.cpp** - Class binding, property binding, exception handling

### Engine-Specific Tests
- **lua_engine.cpp** - Lua scripting functionality (conditional compilation)
- **python_engine.cpp** - Python scripting functionality (conditional compilation)

### Utility Tests
- **iteration.cpp** - SIMD-optimized iteration, cache optimization, performance
- **type_conversion.cpp** - Automatic type conversion between C++ and script types
- **types_and_macros.cpp** - Component type definitions and macro utilities

## Building and Running Tests

### Prerequisites
- C++20 compatible compiler
- CMake 3.20 or higher
- Google Test framework
- Atom project dependencies (atom-component, atom-error, loguru)

### Build Configuration

```bash
# From the tests/components directory
cmake -B build -S .
cmake --build build
```

### Running Tests

```bash
# Run all tests
./build/atom_iocomponent.test.exe

# Run specific test suite
./build/atom_iocomponent.test.exe --gtest_filter="ComponentPoolTest.*"

# Run with verbose output
./build/atom_iocomponent.test.exe --gtest_verbose
```

## Test Features

### Comprehensive Coverage
- **Unit Tests**: All public methods and functions
- **Edge Cases**: Boundary conditions, null inputs, invalid parameters
- **Error Scenarios**: Exception handling, failure modes, recovery
- **Integration Tests**: Component interactions and workflows
- **Performance Tests**: Memory usage, execution time, threading

### Testing Patterns Used
- **Test Fixtures**: Consistent setup/teardown for complex scenarios
- **Parameterized Tests**: Data-driven testing for multiple input combinations
- **Mock Objects**: Isolated testing of complex dependencies
- **Thread Safety**: Concurrent access and race condition testing
- **Memory Safety**: Leak detection and proper resource cleanup

### Conditional Compilation
Tests for optional features (Lua, Python engines) use conditional compilation:

```cpp
#if ATOM_ENABLE_LUA
// Lua-specific tests
#else
TEST(LuaEngineTest, LuaNotEnabled) {
    GTEST_SKIP() << "Lua engine is not enabled in this build";
}
#endif
```

## Test Organization

### Naming Conventions
- Test files: `{component_name}.cpp`
- Test classes: `{ComponentName}Test`
- Test methods: `{ComponentName}Test_{Functionality}_Test`

### Test Structure
```cpp
class ComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup
    }

    void TearDown() override {
        // Test cleanup
    }

    // Test helper methods and data
};

TEST_F(ComponentTest, BasicFunctionality) {
    // Test implementation
}
```

## Coverage Goals

The test suite aims for:
- **100% Function Coverage**: All public methods tested
- **95%+ Line Coverage**: Most code paths exercised
- **100% Branch Coverage**: All conditional logic tested
- **Edge Case Coverage**: Boundary conditions and error scenarios

## Integration with CI/CD

### Automated Testing
```yaml
# Example GitHub Actions workflow
- name: Build and Test
  run: |
    cd tests/components
    cmake -B build -S .
    cmake --build build
    ./build/atom_iocomponent.test.exe --gtest_output=xml:test_results.xml
```

### Coverage Reporting
```bash
# Generate coverage report
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
cmake --build build
./build/atom_iocomponent.test.exe
gcov build/CMakeFiles/atom_iocomponent.test.dir/*.gcno
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

## Troubleshooting

### Common Issues

1. **Missing Dependencies**
   ```
   Error: cannot find -latom-component
   ```
   Solution: Ensure atom project is built and libraries are in the link path.

2. **C++20 Compilation Errors**
   ```
   Error: #error "No C++20 support"
   ```
   Solution: Use a C++20 compatible compiler and set CMAKE_CXX_STANDARD=20.

3. **Header Not Found**
   ```
   Error: atom/components/component.hpp: No such file or directory
   ```
   Solution: Verify include directories are correctly set in CMakeLists.txt.

### Debug Build
For debugging test failures:
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
gdb ./build/atom_iocomponent.test.exe
```

## Contributing

When adding new tests:
1. Follow existing naming conventions
2. Use appropriate test fixtures
3. Include edge cases and error scenarios
4. Add performance tests for critical paths
5. Document complex test scenarios
6. Ensure thread safety testing where applicable

## Performance Benchmarks

Critical performance tests included:
- Memory pool allocation/deallocation speed
- SIMD operation throughput
- Serialization/deserialization performance
- Script execution overhead
- Component iteration efficiency

## Security Testing

Security-focused tests cover:
- Script sandbox isolation
- Input validation and sanitization
- Memory safety and bounds checking
- Resource limit enforcement
- Exception safety guarantees
