# Coverage Analysis Guide

This guide explains how to use the comprehensive coverage analysis system for the Atom project, covering both C++ and Python code.

## Overview

The Atom project includes a sophisticated coverage analysis system that:
- Generates coverage reports for both C++ and Python code
- Creates unified HTML reports combining both languages
- Integrates with CI/CD pipelines
- Provides coverage badges for documentation
- Supports module-specific coverage analysis

## Quick Start

### Generate Full Coverage Report
```bash
# Build and run all tests with coverage
make test-coverage

# Generate unified coverage report
make coverage-unified

# Open unified report in browser
make coverage-unified-open
```

### Python-Only Coverage
```bash
# Generate Python coverage report
make coverage-python

# Or use the dedicated script
python scripts/python_coverage.py --open
```

### C++ Module-Specific Coverage
```bash
# Generate coverage for specific module
make coverage-module MODULE=algorithm

# Or use CMake directly
cd build && make coverage-algorithm
```

## Coverage Tools and Scripts

### 1. Unified Coverage Script (`scripts/unified_coverage.py`)
Generates comprehensive coverage reports combining C++ and Python results.

```bash
# Full analysis
python scripts/unified_coverage.py

# Skip C++ analysis
python scripts/unified_coverage.py --skip-cpp

# Skip Python analysis
python scripts/unified_coverage.py --skip-python

# Generate and open report
python scripts/unified_coverage.py --open
```

### 2. Python Coverage Script (`scripts/python_coverage.py`)
Dedicated Python coverage analysis with advanced options.

```bash
# Run all tests with coverage
python scripts/python_coverage.py

# Run specific test path
python scripts/python_coverage.py --test-path python/tests/test_algorithm.py

# Skip slow tests
python scripts/python_coverage.py --markers "not slow"

# Generate report only (no test execution)
python scripts/python_coverage.py --report-only --open
```

### 3. C++ Coverage Script (`scripts/coverage.sh`)
Comprehensive C++ coverage analysis with multiple options.

```bash
# Full coverage analysis
./scripts/coverage.sh

# Module-specific coverage
./scripts/coverage.sh -t module -m algorithm

# Clean build and coverage
./scripts/coverage.sh -c -t full -o
```

### 4. Coverage Badge Generator (`scripts/coverage_badge.py`)
Generates coverage badges for README and documentation.

```bash
# Update README with coverage badges
python scripts/coverage_badge.py

# Generate markdown only
python scripts/coverage_badge.py --output markdown

# Generate badge URLs
python scripts/coverage_badge.py --output urls
```

## Build System Integration

### CMake Configuration

Enable coverage in CMake:
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DATOM_ENABLE_COVERAGE=ON \
  -DATOM_COVERAGE_HTML=ON \
  -DATOM_BUILD_TESTS=ON
```

### Available CMake Targets

```bash
# Reset coverage counters
make coverage-reset

# Capture coverage data
make coverage-capture

# Generate HTML report
make coverage-html

# Full coverage workflow
make coverage

# Module-specific coverage
make coverage-algorithm
make coverage-memory
# ... etc for each module
```

### Makefile Targets

```bash
# Test with coverage
make test-coverage

# Coverage report generation
make coverage-report

# Reset coverage counters
make coverage-reset

# Unified coverage (C++ + Python)
make coverage-unified
make coverage-unified-open

# Python-only coverage
make coverage-python

# Module-specific coverage
make coverage-module MODULE=algorithm
```

## Coverage Reports

### Report Locations

- **Unified Report**: `coverage/unified/index.html`
- **C++ Report**: `build/coverage/html/index.html`
- **Python Report**: `coverage/python/html/index.html`
- **JSON Data**: `coverage/unified/coverage.json`

### Report Features

#### Unified HTML Report
- Combined C++ and Python coverage statistics
- Interactive charts and graphs
- Module-by-module breakdown
- Links to detailed language-specific reports
- Responsive design for mobile viewing

#### Language-Specific Reports
- **C++ (lcov/genhtml)**: Line and branch coverage with source highlighting
- **Python (coverage.py)**: Line and branch coverage with missing line indicators

#### JSON Report Format
```json
{
  "timestamp": "2024-01-01T12:00:00",
  "overall": {
    "coverage_percentage": 85.2,
    "total_lines": 15000,
    "covered_lines": 12780
  },
  "cpp": {
    "coverage_percentage": 82.1,
    "total_lines": 10000,
    "covered_lines": 8210
  },
  "python": {
    "coverage_percentage": 91.4,
    "total_lines": 5000,
    "covered_lines": 4570
  }
}
```

## CI/CD Integration

### GitHub Actions

The project includes a comprehensive GitHub Actions workflow (`.github/workflows/coverage.yml`) that:

1. **Builds** the project with coverage enabled
2. **Runs** both C++ and Python tests
3. **Generates** unified coverage reports
4. **Uploads** reports to Codecov
5. **Comments** coverage results on pull requests
6. **Deploys** coverage reports to GitHub Pages
7. **Enforces** minimum coverage thresholds

### Coverage Thresholds

- **Overall Minimum**: 75%
- **C++ Target**: 80%
- **Python Target**: 75%
- **Critical Modules**: 90% (memory, error handling)

### Pull Request Integration

Coverage results are automatically commented on pull requests with:
- Coverage badges
- Detailed breakdown by language
- Comparison with base branch
- Pass/fail status based on thresholds

## Configuration Files

### CMake Coverage Configuration (`cmake/CoverageConfig.cmake`)
- Coverage tool detection
- Compiler flag setup
- Target creation functions
- Module-specific coverage targets

### Python Coverage Configuration (`pyproject.toml`)
```toml
[tool.coverage.run]
source = ["atom", "python"]
branch = true
omit = ["*/tests/*", "*/test_*"]

[tool.coverage.report]
exclude_lines = ["pragma: no cover", "def __repr__"]
show_missing = true
precision = 2
```

### Pytest Configuration
```toml
[tool.pytest.ini_options]
addopts = [
    "--cov=atom",
    "--cov-report=html:coverage/python/html",
    "--cov-branch",
    "--cov-fail-under=75"
]
```

## Best Practices

### Writing Testable Code

1. **Keep functions small** and focused
2. **Separate concerns** to enable isolated testing
3. **Use dependency injection** for external dependencies
4. **Avoid global state** that makes testing difficult
5. **Write tests first** (TDD) when possible

### Coverage Guidelines

1. **Aim for high coverage** but focus on quality over quantity
2. **Test edge cases** and error conditions
3. **Don't ignore uncovered code** - either test it or mark as no-cover
4. **Review coverage reports** regularly to identify gaps
5. **Use coverage to guide** refactoring efforts

### Exclusions

Use coverage exclusions sparingly and document reasons:

```cpp
// C++ - Use LCOV_EXCL_LINE for single lines
if (unlikely_condition) { // LCOV_EXCL_LINE
    handle_rare_case();   // LCOV_EXCL_LINE
}

// C++ - Use LCOV_EXCL_START/STOP for blocks
// LCOV_EXCL_START
void debug_only_function() {
    // Debug code not covered in release builds
}
// LCOV_EXCL_STOP
```

```python
# Python - Use pragma: no cover
def debug_function():  # pragma: no cover
    # Debug code
    pass

if TYPE_CHECKING:  # pragma: no cover
    # Type checking imports
    from typing import Optional
```

## Troubleshooting

### Common Issues

1. **No coverage data generated**
   - Ensure coverage flags are enabled in build
   - Check that tests are actually running
   - Verify gcov/lcov tools are installed

2. **Low coverage numbers**
   - Check for excluded directories
   - Verify source paths in coverage configuration
   - Ensure all test files are being executed

3. **Build failures with coverage**
   - Coverage adds overhead - may need longer timeouts
   - Some optimizations are disabled in coverage builds
   - Debug symbols increase binary size

### Debug Commands

```bash
# Check coverage tools
which gcov lcov genhtml

# Verify coverage flags
cmake -B build -DATOM_ENABLE_COVERAGE=ON
grep -r "coverage" build/

# Manual coverage generation
cd build
lcov --directory . --capture --output-file coverage.info
lcov --list coverage.info
```

## Advanced Usage

### Custom Coverage Targets

Create custom coverage targets for specific scenarios:

```cmake
# In CMakeLists.txt
add_custom_target(coverage-integration
    COMMAND ${CMAKE_CTEST_COMMAND} -L integration
    COMMAND ${LCOV_PROGRAM} --capture --output-file integration_coverage.info
    COMMENT "Running integration tests with coverage"
)
```

### Coverage Diff Analysis

Compare coverage between branches:

```bash
# Generate coverage for current branch
python scripts/unified_coverage.py

# Switch to base branch and generate coverage
git checkout main
python scripts/unified_coverage.py

# Compare results (manual process - could be automated)
```

### Performance Impact

Coverage analysis adds overhead:
- **Build time**: +20-30% due to instrumentation
- **Test execution**: +10-20% due to data collection
- **Binary size**: +50-100% due to debug symbols

For performance-critical testing, consider:
- Running coverage analysis separately from regular CI
- Using sampling-based coverage tools
- Focusing coverage on critical code paths

## Support and Resources

- **Documentation**: `docs/TESTING_CONVENTIONS.md`
- **Scripts**: `scripts/` directory
- **Configuration**: `cmake/CoverageConfig.cmake`, `pyproject.toml`
- **CI/CD**: `.github/workflows/coverage.yml`

For questions or issues with coverage analysis, please:
1. Check this documentation
2. Review existing GitHub issues
3. Create a new issue with detailed information about the problem
