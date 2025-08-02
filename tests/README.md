# Test Directory Structure

This directory contains the test suite for the Atom project, organized by module for better maintainability and navigation.

## Directory Organization

### C++ Tests (`tests/`)
Each module has its own subdirectory containing:
- `CMakeLists.txt` - Build configuration for the module tests
- `test_<module>.cpp` - Main test file for the module
- Additional test files as needed

### Python Tests (`python/tests/`)
- `test_<module>.py` - Test files for Python bindings
- `conftest.py` - Pytest configuration and fixtures
- `__init__.py` - Package initialization

## Running Tests

### C++ Tests
```bash
# Build and run all tests
make test

# Run tests with coverage
make test-coverage

# Run specific module tests
cd build && ctest -R "algorithm"
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
