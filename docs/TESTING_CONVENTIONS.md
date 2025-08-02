# Testing Conventions for Atom Project

This document outlines the standardized testing conventions for the Atom project, covering both C++ and Python test suites.

## Directory Structure

### C++ Tests
```
tests/
├── algorithm/          # Algorithm module tests
├── async/             # Async module tests
├── components/        # Components module tests
├── connection/        # Connection module tests
├── memory/            # Memory module tests
├── search/            # Search module tests
├── sysinfo/           # System info module tests
├── type/              # Type module tests
├── utils/             # Utils module tests
└── web/               # Web module tests
```

### Python Tests
```
python/tests/
├── __init__.py        # Test package initialization
├── conftest.py        # Pytest configuration and fixtures
├── test_algorithm.py  # Algorithm module tests
├── test_connection.py # Connection module tests
├── test_search.py     # Search module tests
├── test_sysinfo.py    # System info module tests
└── data/              # Test data files
```

## File Naming Conventions

### C++ Test Files
- **Pattern**: `test_<module_name>.cpp` or `test_<specific_feature>.cpp`
- **Examples**: 
  - `test_algorithm.cpp`
  - `test_memory_pool.cpp`
  - `test_hash.cpp`
- **Header files**: `test_<module_name>.hpp` (for header-only tests)

### Python Test Files
- **Pattern**: `test_<module_name>.py`
- **Examples**:
  - `test_algorithm.py`
  - `test_connection.py`
  - `test_search.py`

## Class Naming Conventions

### C++ Test Classes
- **Pattern**: `<ModuleName>Test` or `<FeatureName>Test`
- **Examples**:
  - `class AlgorithmTest : public ::testing::Test`
  - `class MemoryPoolTest : public ::testing::Test`
  - `class KMPTest : public ::testing::Test`

### Python Test Classes
- **Pattern**: `Test<ModuleName>` or `Test<FeatureName>`
- **Examples**:
  - `class TestAlgorithmModule:`
  - `class TestMemoryPool:`
  - `class TestHashFunctions:`

## Test Method Naming Conventions

### C++ Test Methods
- **Pattern**: `TEST_F(ClassName, MethodName)` where MethodName describes the test
- **Naming style**: PascalCase for method names
- **Examples**:
  - `TEST_F(MemoryPoolTest, Constructor)`
  - `TEST_F(MemoryPoolTest, AllocateAndDeallocate)`
  - `TEST_F(KMPTest, BasicPatternMatching)`

### Python Test Methods
- **Pattern**: `test_<description>` using snake_case
- **Examples**:
  - `def test_module_import(self):`
  - `def test_memory_allocation(self):`
  - `def test_hash_function_correctness(self):`

## Test Categories and Markers

### C++ Test Categories
Tests should be organized into logical groups within test files:
- Basic functionality tests
- Edge case tests
- Performance tests
- Integration tests

### Python Test Markers
Use pytest markers to categorize tests:
- `@pytest.mark.unit` - Unit tests
- `@pytest.mark.integration` - Integration tests
- `@pytest.mark.slow` - Slow-running tests
- `@pytest.mark.benchmark` - Performance benchmarks
- `@pytest.mark.functional` - Functional tests

## Documentation Standards

### Test Documentation
- Each test class should have a docstring explaining its purpose
- Complex test methods should have docstrings explaining the test scenario
- Use clear, descriptive names that explain what is being tested

### Examples

#### C++ Test Documentation
```cpp
/**
 * @brief Test fixture for memory pool functionality
 * @details Tests various aspects of memory pool including allocation,
 * deallocation, and memory management edge cases.
 */
class MemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }
};

TEST_F(MemoryPoolTest, AllocateExceedingBlockSize) {
    // Test that allocation exceeding block size throws exception
    MemoryPool<int> pool;
    EXPECT_THROW(pool.allocate(4097), MemoryPoolException);
}
```

#### Python Test Documentation
```python
class TestMemoryPool:
    """Test cases for memory pool functionality.
    
    Tests various aspects of memory pool including allocation,
    deallocation, and memory management edge cases.
    """
    
    def test_allocate_exceeding_block_size(self):
        """Test that allocation exceeding block size raises exception."""
        # Test implementation
        pass
```

## Build Integration

### C++ Tests
- Each module should have its own CMakeLists.txt in the test directory
- Tests should be registered with CTest using `add_test()`
- Coverage should be enabled for all test targets

### Python Tests
- Tests should be discoverable by pytest
- Coverage should be configured in pyproject.toml
- Tests should run in CI/CD pipelines

## Coverage Requirements

### Minimum Coverage Targets
- **C++ modules**: 80% line coverage, 70% branch coverage
- **Python modules**: 75% line coverage, 65% branch coverage
- **Critical modules** (memory, error handling): 90% coverage

### Coverage Exclusions
- Test files themselves
- Generated code
- Platform-specific code that cannot be tested
- Debug-only code paths

## Best Practices

### General Guidelines
1. **One concept per test**: Each test should verify one specific behavior
2. **Descriptive names**: Test names should clearly indicate what is being tested
3. **Arrange-Act-Assert**: Structure tests with clear setup, execution, and verification phases
4. **Independent tests**: Tests should not depend on each other
5. **Deterministic**: Tests should produce consistent results

### C++ Specific
1. Use RAII for resource management in tests
2. Prefer EXPECT_* over ASSERT_* unless test cannot continue
3. Use test fixtures for common setup/teardown
4. Mock external dependencies

### Python Specific
1. Use fixtures for common test data and setup
2. Parametrize tests when testing multiple similar scenarios
3. Use appropriate markers for test categorization
4. Mock external dependencies and I/O operations

## Continuous Integration

### Automated Testing
- All tests must pass before code can be merged
- Coverage reports should be generated for each PR
- Performance regression tests should be run for critical paths

### Test Execution
- Unit tests: Run on every commit
- Integration tests: Run on PR creation/update
- Performance tests: Run nightly or on release branches
- Full test suite: Run before releases

## Tools and Dependencies

### C++ Testing Stack
- **Framework**: Google Test (GTest) with GMock
- **Coverage**: gcov/lcov for coverage analysis
- **Build**: CMake with CTest integration
- **CI**: GitHub Actions or similar

### Python Testing Stack
- **Framework**: pytest with pytest-cov
- **Coverage**: coverage.py for coverage analysis
- **Benchmarking**: pytest-benchmark for performance tests
- **CI**: GitHub Actions or similar

## Migration Guide

When updating existing tests to follow these conventions:

1. **Rename files** to follow the standard patterns
2. **Update class names** to match conventions
3. **Refactor test methods** to use standard naming
4. **Add appropriate markers** and documentation
5. **Update build files** to reflect new structure
6. **Verify coverage** is maintained or improved

## Enforcement

These conventions should be enforced through:
- Code review processes
- Automated linting and formatting tools
- CI/CD pipeline checks
- Documentation reviews

For questions or clarifications about these conventions, please refer to the project maintainers or create an issue in the project repository.
