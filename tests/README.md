# Test Directory Structure

This directory contains the comprehensive test suite for the Atom project, organized by module for better maintainability and navigation.

## Directory Organization

### C++ Tests (`tests/`)

Each module has its own subdirectory containing:

- `CMakeLists.txt` - Standardized build configuration for the module tests
- `test_<module>.cpp` - Main test file for the module
- Additional test files as needed

**Available Test Modules:**

- `algorithm/` - Algorithm and mathematical function tests
- `async/` - Asynchronous programming and concurrency tests
- `components/` - Component system tests
- `connection/` - Network and communication tests
- `error/` - Error handling and exception tests
- `extra/` - External library integration tests
- `image/` - Image processing tests
- `io/` - Input/output operation tests
- `log/` - Logging system tests
- `memory/` - Memory management tests
- `meta/` - Metaprogramming and reflection tests
- `search/` - Search and indexing tests
- `secret/` - Security and encryption tests
- `serial/` - Serial communication tests
- `sysinfo/` - System information tests
- `system/` - System-level operation tests
- `type/` - Type system and container tests
- `utils/` - Utility function tests
- `web/` - Web-related functionality tests

### Python Tests (`python/tests/`)

- `test_<module>.py` - Test files for Python bindings
- `conftest.py` - Pytest configuration and fixtures
- `__init__.py` - Package initialization

## Running Tests

### Quick Start

```bash
# Run all tests with the comprehensive test runner
./tests/run_all_tests.sh

# Build and run specific module
./tests/run_all_tests.sh --module memory

# Use CTest runner
./tests/run_all_tests.sh --ctest
```

### C++ Tests

```bash
# Configure and build all tests
cmake -B build -DATOM_BUILD_TESTS=ON -DATOM_TEST_BUILD_ALL=ON
cd build && make -j$(nproc)

# Run all tests with CTest
ctest --output-on-failure

# Run specific module tests
ctest -R "algorithm"

# Run individual test executable
./tests/algorithm/atom_algorithm.test
```

### Python Tests

```bash
# Run all Python tests
python -m pytest python/tests/

# Run with coverage
python scripts/python_coverage.py

# Run specific module tests
python -m pytest python/tests/test_algorithm.py
```

## Adding New Tests

### For C++ Modules

1. Create test files in the appropriate module directory
2. Follow the naming convention: `test_<feature>.cpp`
3. Use the existing CMakeLists.txt or create a new one if needed

### For Python Modules

1. Create test files following the pattern: `test_<module>.py`
2. Use appropriate pytest markers for categorization
3. Follow the established class and method naming conventions

## Test Categories

- **Unit Tests**: Test individual functions/classes in isolation
- **Integration Tests**: Test module interactions
- **Performance Tests**: Benchmark critical functionality
- **Functional Tests**: Test end-to-end scenarios

Refer to `docs/TESTING_CONVENTIONS.md` for detailed guidelines.
