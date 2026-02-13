# Enhanced Testing Guide for Atom System and Sysinfo Modules

This guide provides comprehensive information about the enhanced testing infrastructure for the Atom system and sysinfo modules, covering the latest implementation updates and new testing capabilities.

## Overview

The enhanced testing suite covers:

- **AdvancedExecutor**: Environment variable handling, concurrent execution, caching, security
- **Enhanced Crontab System**: Modular components including cache, monitor, security
- **Sysinfo Enhanced Features**: New API features, export functionality, caching mechanisms
- **Integration Testing**: Cross-component interactions and end-to-end workflows

## Test Structure

### System Tests (`/tests/system/`)

#### Core Test Files

- `test_command.cpp` - Basic command execution tests
- `test_advanced_executor.cpp` - **NEW** Advanced executor functionality
- `test_cron_components.cpp` - **NEW** Enhanced crontab components
- `test_crash_quotes.cpp` - Crash handling tests
- `test_voltage.cpp` - Voltage monitoring tests
- `test_wregistry.cpp` - Windows registry tests

#### Advanced Executor Tests

**File**: `test_advanced_executor.cpp`

**Coverage**:

- CancellationToken functionality
- ExecutionResourcePool management
- Environment variable handling
- Concurrent execution
- Timeout and cancellation
- Retry mechanisms
- Parallel execution
- Resource pool constraints
- Error handling and edge cases
- Performance benchmarks

**Key Test Cases**:

```cpp
TEST_F(AdvancedExecutorTest, EnvironmentVariableHandling)
TEST_F(AdvancedExecutorTest, CommandCancellation)
TEST_F(AdvancedExecutorTest, ParallelExecution)
TEST_F(AdvancedExecutorTest, ResourcePoolConcurrentExecution)
```

#### Crontab Component Tests

**File**: `test_cron_components.cpp`

**Coverage**:

- CronCache: TTL, statistics, cleanup
- CronMonitor: Event logging, performance metrics, alerts
- CronSecurity: Permissions, command validation, resource limits
- Integration scenarios

**Key Test Cases**:

```cpp
TEST_F(CronCacheTest, CacheExpiration)
TEST_F(CronMonitorTest, PerformanceMetrics)
TEST_F(CronSecurityTest, CommandValidation)
TEST_F(CronIntegrationTest, EndToEndJobExecution)
```

### Sysinfo Tests (`/tests/sysinfo/`)

#### Core Test Files

- `battery.cpp` - Battery information tests
- `bios.cpp` - BIOS information tests
- `cpu.cpp` - CPU information tests
- `disk.cpp` - Disk information tests
- `gpu.cpp` - GPU information tests
- `locale.cpp` - Locale information tests
- `memory.cpp` - Memory information tests
- `os.cpp` - Operating system tests
- `sn.cpp` - Serial number tests
- `virtual.cpp` - Virtualization tests
- `wifi.cpp` - WiFi information tests
- `wm.cpp` - Window manager tests
- `test_enhanced_features.cpp` - **NEW** Enhanced sysinfo features

#### Enhanced Features Tests

**File**: `test_enhanced_features.cpp`

**Coverage**:

- Comprehensive system information API
- Caching mechanisms and invalidation
- JSON/XML export functionality
- File export capabilities
- System fingerprinting
- Data anonymization
- System change detection
- Backward compatibility
- Error handling

**Key Test Cases**:

```cpp
TEST_F(EnhancedSysinfoTest, ComprehensiveSystemInfo)
TEST_F(EnhancedSysinfoTest, CachingFunctionality)
TEST_F(EnhancedSysinfoTest, JsonExport)
TEST_F(EnhancedSysinfoTest, SystemFingerprinting)
TEST_F(EnhancedSysinfoTest, DataAnonymization)
```

## Running Tests

### Quick Start

```bash
# Build and run all tests
./scripts/run_enhanced_tests.sh

# Build tests only
./scripts/run_enhanced_tests.sh build

# Run tests only
./scripts/run_enhanced_tests.sh run

# Run specific categories
./scripts/run_enhanced_tests.sh system
./scripts/run_enhanced_tests.sh sysinfo
./scripts/run_enhanced_tests.sh advanced
```

### Manual Test Execution

#### Building Tests

```bash
cd build

# Build all system tests
make atom_system.test

# Build all sysinfo tests
make atom_sysinfo.test

# Build individual test targets
make test_advanced_executor_individual
make test_enhanced_features_test
```

#### Running Tests

```bash
# Run all system tests
./tests/system/atom_system.test

# Run all sysinfo tests
./tests/sysinfo/atom_sysinfo.test

# Run specific test with filters
./tests/system/atom_system.test --gtest_filter="AdvancedExecutorTest.*"
./tests/sysinfo/atom_sysinfo.test --gtest_filter="EnhancedSysinfoTest.*"

# Run with verbose output
./tests/system/atom_system.test --gtest_verbose

# Generate XML output
./tests/system/atom_system.test --gtest_output=xml:results.xml
```

### Using CTest

```bash
cd build

# Run all tests
ctest

# Run tests with verbose output
ctest -V

# Run specific test patterns
ctest -R "system"
ctest -R "sysinfo"
ctest -R "advanced"

# Run tests in parallel
ctest -j$(nproc)
```

## Test Categories

### Unit Tests

- Individual component functionality
- API contract validation
- Error condition handling
- Edge case coverage

### Integration Tests

- Cross-component interactions
- End-to-end workflows
- System-level integration
- Resource sharing scenarios

### Performance Tests

- Execution time benchmarks
- Memory usage validation
- Concurrent access performance
- Cache efficiency metrics

### Stress Tests

- High-load scenarios
- Resource exhaustion handling
- Long-running operations
- Memory leak detection

## Test Configuration

### Environment Variables

```bash
# Enable debug logging
export ATOM_TEST_DEBUG=1

# Set test timeout
export ATOM_TEST_TIMEOUT=30

# Configure test data directory
export ATOM_TEST_DATA_DIR=/tmp/atom_tests

# Enable performance profiling
export ATOM_TEST_PROFILE=1
```

### CMake Options

```bash
# Enable testing
cmake -DBUILD_TESTING=ON ..

# Enable coverage
cmake -DENABLE_COVERAGE=ON ..

# Debug build for testing
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Enable sanitizers
cmake -DENABLE_SANITIZERS=ON ..
```

## Test Data and Mocking

### Mock System Information

Tests use mock data when real system information is not available or for consistent testing across platforms.

### Temporary Files

Tests that create files use temporary directories that are automatically cleaned up.

### Cross-Platform Considerations

Tests are designed to work across Windows, Linux, and macOS with platform-specific command variations.

## Continuous Integration

### GitHub Actions

The test suite integrates with GitHub Actions for automated testing on multiple platforms.

### Coverage Reports

Code coverage reports are generated and can be viewed to identify untested code paths.

### Performance Regression Detection

Performance benchmarks help detect regressions in critical code paths.

## Troubleshooting

### Common Issues

1. **Tests not building**
   - Check CMake configuration
   - Verify dependencies are installed
   - Ensure C++20 compiler support

2. **Tests failing on specific platforms**
   - Check platform-specific implementations
   - Verify system permissions
   - Review platform-specific test conditions

3. **Performance tests unstable**
   - Run on dedicated test machines
   - Adjust timing thresholds
   - Consider system load

### Debug Mode

```bash
# Build with debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Run with debugger
gdb ./tests/system/atom_system.test
```

### Logging

```bash
# Enable verbose logging
export SPDLOG_LEVEL=debug
./tests/system/atom_system.test
```

## Contributing

### Adding New Tests

1. **Create test file** in appropriate directory
2. **Follow naming convention**: `test_<component>.cpp`
3. **Use Google Test framework**
4. **Include in CMakeLists.txt**
5. **Add to test runner script**

### Test Guidelines

- **Comprehensive coverage**: Test normal, edge, and error cases
- **Platform independence**: Use cross-platform constructs
- **Resource cleanup**: Always clean up temporary resources
- **Deterministic**: Tests should be repeatable
- **Fast execution**: Keep tests efficient
- **Clear assertions**: Use descriptive test names and assertions

### Code Review Checklist

- [ ] Tests cover new functionality
- [ ] Tests are platform-independent
- [ ] Error cases are tested
- [ ] Performance impact is considered
- [ ] Documentation is updated
- [ ] CI passes on all platforms

## Future Enhancements

### Planned Improvements

1. **Automated Performance Benchmarking**
2. **Enhanced Mock System**
3. **Visual Test Reports**
4. **Integration with External Tools**
5. **Advanced Coverage Analysis**

### Test Infrastructure Roadmap

- **Q1**: Enhanced mocking framework
- **Q2**: Performance regression detection
- **Q3**: Visual test reporting
- **Q4**: Advanced integration testing

This enhanced testing infrastructure ensures the reliability and maintainability of the Atom system and sysinfo modules while supporting continued development and feature enhancement.
