# Memory Module Testing Framework

This directory contains a comprehensive testing framework for the Atom memory module. The testing suite covers all aspects of memory functionality including basic operations, advanced features, performance testing, and integration tests.

## Overview

The memory module testing framework provides:

- **Comprehensive Unit Tests**: Tests for all public APIs and functionality
- **Performance Benchmarks**: Memory performance and latency testing
- **Integration Tests**: End-to-end testing of complex workflows
- **Stress Tests**: System stability under memory pressure
- **Memory Leak Detection**: Validation of leak detection algorithms
- **NUMA Topology Testing**: Multi-node memory architecture testing
- **Utility Function Testing**: Validation of helper functions

## Test Structure

### Test Files

- `test_memory_comprehensive.cpp` - Main test suite with all test cases
- `test_CMakeLists.txt` - CMake configuration for building tests
- `run_tests.sh` - Convenient test runner script
- `TEST_README.md` - This documentation file

### Test Categories

1. **Basic Memory Information Tests**
   - Total memory size validation
   - Available memory checks
   - Memory usage percentage verification
   - Memory load calculations

2. **Memory Performance Tests**
   - Read/write speed measurements
   - Bandwidth utilization testing
   - Latency profiling
   - Cache performance analysis

3. **Memory Pressure Tests**
   - Pressure level detection
   - Pressure score calculation
   - Threshold validation
   - Recommendation generation

4. **Memory Compression Tests**
   - Compression detection
   - Algorithm identification
   - Performance benchmarking
   - Efficiency measurements

5. **NUMA Topology Tests**
   - Node detection
   - Topology analysis
   - Performance characteristics
   - Optimization recommendations

6. **Memory Allocation Tracking Tests**
   - Allocation monitoring
   - Heap analysis
   - Fragmentation detection
   - Pool management

7. **Memory Leak Detection Tests**
   - Basic leak detection
   - Advanced pattern recognition
   - Historical analysis
   - Confidence scoring

8. **Utility Function Tests**
   - Byte size formatting
   - Bandwidth formatting
   - Duration formatting
   - String parsing

## Building and Running Tests

### Prerequisites

- CMake 3.16 or higher
- Google Test framework
- C++20 compatible compiler
- spdlog library
- pthread library

### Quick Start

```bash
# Build and run all tests
./run_tests.sh build
./run_tests.sh test

# Or in one command
./run_tests.sh build && ./run_tests.sh test
```

### Available Test Commands

```bash
# Build the test suite
./run_tests.sh build

# Run all tests
./run_tests.sh test

# Run specific test categories
./run_tests.sh performance    # Performance tests only
./run_tests.sh leak          # Leak detection tests only
./run_tests.sh numa          # NUMA topology tests only
./run_tests.sh utility       # Utility function tests only

# Run benchmarks
./run_tests.sh benchmark

# Run with memory checking
./run_tests.sh valgrind

# Generate coverage report
./run_tests.sh coverage

# Run stress tests
./run_tests.sh stress

# Clean build directory
./run_tests.sh clean

# Show help
./run_tests.sh help
```

### Manual Building

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
./memory_tests
```

## Test Configuration

### Environment Variables

- `GTEST_COLOR=1` - Enable colored test output
- `GTEST_BRIEF=1` - Use brief output format
- `GTEST_FILTER=pattern` - Run specific tests matching pattern

### CMake Options

- `CMAKE_BUILD_TYPE=Debug` - Enable debug symbols and coverage
- `CMAKE_BUILD_TYPE=Release` - Optimized build for performance testing

## Performance Testing

The framework includes comprehensive performance testing:

### Memory Benchmarks
- Sequential read/write performance
- Random access patterns
- Cache hierarchy analysis
- Bandwidth utilization measurement

### Latency Profiling
- Memory access latency across different sizes
- Cache level identification
- NUMA node access characteristics

### Stress Testing
- Memory allocation under pressure
- Fragmentation behavior
- System stability validation

## Memory Leak Detection Testing

The leak detection tests validate:

### Detection Algorithms
- Gradual leak pattern recognition
- Sudden allocation spike detection
- Periodic leak identification
- Allocation tracking accuracy

### Pattern Analysis
- Confidence scoring validation
- Severity classification
- Recommendation generation
- Historical trend analysis

## NUMA Testing

NUMA-specific tests cover:

### Topology Detection
- Node enumeration
- CPU-memory binding
- Distance matrix validation
- Bandwidth characteristics

### Performance Analysis
- Local vs remote access patterns
- Hit ratio calculations
- Migration statistics
- Optimization recommendations

## Coverage Analysis

Generate test coverage reports:

```bash
./run_tests.sh coverage
```

This will:
1. Rebuild with coverage flags
2. Run all tests
3. Generate coverage data
4. Create HTML coverage report (if lcov/genhtml available)

## Memory Checking

Run tests with Valgrind for memory error detection:

```bash
./run_tests.sh valgrind
```

This performs:
- Memory leak detection
- Buffer overflow checking
- Use-after-free detection
- Uninitialized memory access detection

## Continuous Integration

For CI/CD pipelines, use:

```bash
# Quick validation
./run_tests.sh build && ./run_tests.sh test

# Full validation with coverage
./run_tests.sh build && ./run_tests.sh coverage

# Performance regression testing
./run_tests.sh build && ./run_tests.sh benchmark
```

## Troubleshooting

### Common Issues

1. **Build Failures**
   - Ensure all dependencies are installed
   - Check CMake version compatibility
   - Verify C++20 compiler support

2. **Test Failures**
   - Some tests may be platform-specific
   - NUMA tests require multi-node systems
   - Performance tests may vary with system load

3. **Permission Issues**
   - Some memory operations require elevated privileges
   - NUMA statistics may need root access
   - Memory compression detection varies by system

### Debug Mode

For debugging test failures:

```bash
# Build in debug mode
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Run with GDB
gdb ./memory_tests
```

## Contributing

When adding new tests:

1. Follow existing test naming conventions
2. Add appropriate assertions and error messages
3. Include performance benchmarks for new features
4. Update this README with new test categories
5. Ensure tests work across different platforms

## Test Results Interpretation

### Performance Metrics
- Read/write speeds should be reasonable for the hardware
- Latency should increase with memory hierarchy levels
- Bandwidth utilization should not exceed 100%

### Memory Pressure
- Pressure scores range from 0-100
- Levels: NONE(0), LOW(1), MEDIUM(2), HIGH(3), CRITICAL(4)
- Recommendations should be actionable

### Leak Detection
- Confidence scores range from 0-100%
- Higher confidence indicates more reliable detection
- Severity levels guide response urgency

## Support

For issues with the testing framework:

1. Check the troubleshooting section
2. Review test logs for specific error messages
3. Ensure system meets all prerequisites
4. Consider platform-specific limitations
