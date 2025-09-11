# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Project Architecture

The **Atom** library is a modular C++20/C++23 foundational library for astronomical software projects, organized as 12+ independent modules with explicit dependency management.

### Module Structure & Dependencies

Each module follows this standardized pattern:
```
atom/<module>/
├── CMakeLists.txt           # Module build config with dependency checks
├── <module>.hpp             # Compatibility header (may redirect to core/)
├── core/<module>.hpp        # Actual implementation (newer pattern)
└── xmake.lua               # XMake build configuration
```

**Key architectural principle**: Many root-level headers like `algorithm.hpp` are compatibility redirects to `core/algorithm.hpp`. Always check for the `core/` subdirectory when examining module structure.

### Dependency Hierarchy

The build system enforces a strict dependency hierarchy defined in `cmake/module_dependencies.cmake`:

- **Foundation**: `atom-error` (base, no dependencies)
- **Core**: `atom-log` → `atom-meta`/`atom-utils` 
- **Specialized**: `atom-web`, `atom-async`, `atom-system`, etc.

Build order: `atom-error` → `atom-log` → `atom-meta`/`atom-utils` → specialized modules

### Component Architecture Pattern

The library uses a sophisticated component registry system for dependency injection and lifecycle management:
- **Registry Pattern**: Central `Registry` class manages all components with thread-safe operations
- **Lifecycle Management**: `LifecycleManager` handles component initialization order and dependency resolution
- **Dependency Injection**: Components can declare required/optional dependencies that are auto-resolved
- **Hot Reload**: Components support runtime reloading for development efficiency

## Build Commands

### CMake (Primary)
```bash
# Configure with preset (recommended)
cmake --preset release
cmake --build --preset release -j

# Available presets: debug, release, relwithdebinfo
# Platform-specific: debug-msys2, release-msys2, debug-make, release-make, debug-vs, release-vs
cmake --preset debug
cmake --build --preset debug -j

# Manual configuration with common options
cmake -B build -DATOM_BUILD_EXAMPLES=ON -DATOM_BUILD_TESTS=ON -DATOM_BUILD_PYTHON_BINDINGS=ON
cmake --build build --target atom-algorithm  # Build specific module
cmake --build build --parallel 8             # Parallel build
```

### Cross-Platform Scripts (Recommended)
```bash
# Unix/Linux/macOS - Enhanced build script
./build.sh --release --tests --examples --jobs 8
./build.sh --debug --run-tests --docs --python
./build.sh --clean --install-deps --package    # Full clean build with packaging

# Windows
build.bat --release --tests --examples
build.bat --debug --run-tests --docs
```

### XMake (Alternative)
```bash
xmake f --build_examples=y --build_tests=y --python=y
xmake build
xmake test     # Run tests
xmake install  # Install built libraries
```

### Python Development
```bash
pip install -e .[dev]
pytest -q  # Run Python tests
```

## Module-Specific Development

### Adding New Modules
1. Create module directory under `atom/`
2. Add dependency entry in `cmake/module_dependencies.cmake`
3. Update `ATOM_MODULE_BUILD_ORDER`
4. Create corresponding test directory in `tests/`
5. Add example in `example/` if public-facing

### Dependency Management
Dependencies are auto-resolved via CMake. Each module's `CMakeLists.txt` includes:
```cmake
foreach(dep ${ATOM_<MODULE>_DEPENDS})
  string(REPLACE "atom-" "ATOM_BUILD_" dep_var_name ${dep})
  # Auto-enables missing dependencies or warns
endforeach()
```

## Testing

### C++ Tests
```bash
# Debug build with tests
cmake --preset debug && cmake --build --preset debug -j
ctest --preset default --output-on-failure

# Run specific test module
cmake --build build --target test_<module>

# Using build script (runs tests automatically)
./build.sh --debug --run-tests

# XMake testing
xmake test
```

### Test Organization
- **Unit Tests**: `tests/<module>/test_*.hpp` with GoogleTest framework
- **Integration Tests**: Uses `atom/tests/test.hpp` custom registration system
- **Examples**: `example/<module>/*.cpp` - one executable per file

### Test Registration Pattern
```cpp
// Custom test registration in atom/tests/test.hpp
ATOM_INLINE void registerTest(std::string name, std::function<void()> func,
                              bool async = false, double time_limit = 0.0,
                              bool skip = false,
                              std::vector<std::string> dependencies = {},
                              std::vector<std::string> tags = {});
```

## Key Development Patterns

### Platform Detection
Use macros from `atom/macro.hpp`:
- `ATOM_PLATFORM_WINDOWS/LINUX/APPLE` for platform detection
- `ATOM_USE_BOOST*` flags for Boost integration
- Prefer existing macros over raw `#ifdef`

### Error Handling
All modules depend on `atom-error`:
- Use `Result<T>` types from `atom-error`, not raw exceptions
- Follow RAII principles with smart pointers

### Logging
Use `atom-log` structured logging instead of `std::cout`

### Async Operations
`atom-async` provides async primitives - don't reinvent async functionality

### Module Integration Points
- **Error Handling**: `atom-error` - use result types
- **Logging**: `atom-log` - structured logging
- **Async Operations**: `atom-async` - async primitives
- **Utilities**: `atom-utils` - check before adding duplicates

## Build Configuration

### Key Build Options
- `ATOM_BUILD_EXAMPLES=ON` - Build example applications
- `ATOM_BUILD_TESTS=ON` - Build test suite
- `ATOM_BUILD_PYTHON_BINDINGS=ON` - Enable Python bindings
- `ATOM_BUILD_DOCS=ON` - Generate documentation
- Individual module flags: `ATOM_BUILD_<MODULE>=ON`

### Build System Features
- **Ninja Generator**: Automatically used if available for faster builds
- **Parallel Builds**: Scripts auto-detect CPU cores
- **Cross-Platform**: Windows (MSVC), Linux (GCC), macOS (Clang)
- **Dual Build System**: Both CMake and XMake supported

## Code Standards

### Language Requirements
- **C++20 minimum**, C++23 preferred (auto-detected based on compiler)
- Extensive use of concepts, ranges, source_location
- Template-heavy design with meta-programming in `atom/meta/`

### Naming Conventions (per STYLE_OF_CODE.md)
- **Variables/Functions**: camelCase
- **Classes/Namespaces**: PascalCase 
- **Constants**: UPPER_SNAKE_CASE
- **Files**: lower_snake_case.[cpp|hpp]
- **Class members**: m_prefix for private variables

### Documentation
- Prefer Doxygen format: `@brief`, `@param`, `@return`
- Comments should explain purpose and context

## File Structure Patterns

### Important Files
- **Version Info**: `cmake/version_info.h.in` → `build/atom_version_info.h`
- **Platform Config**: `cmake/PlatformSpecifics.cmake`
- **Compiler Options**: `cmake/compiler_options.cmake`
- **External Deps**: `vcpkg.json` and XMake `add_requires()`

### Python Bindings
- Located in `python/` with pybind11
- Auto-detects module types from directory structure
- Each module gets its own Python binding file

## Common Development Tasks

### Documentation Generation
```bash
doxygen Doxyfile  # C++ docs
sphinx-build -b html docs docs/_build  # Python docs
```

### Code Formatting
```bash
clang-format -i **/*.cpp **/*.hpp  # Use .clang-format config
pre-commit run -a  # Python formatting (Black, isort, Ruff, MyPy)
```

### Package Management & Installation
- **C++ Dependencies**: Via vcpkg/Conan (currently disabled by default)
- **Python Dependencies**: Via pip/conda
- **Modular Installation**: `scripts/modular-installer.py` for component-wise installation
- **System Dependencies**: `./build.sh --install-deps` auto-installs required packages

### Modular Installation System
```bash
# Install specific components with dependency resolution
python scripts/modular-installer.py install core networking
python scripts/modular-installer.py install algorithm async --force

# List available components and meta-packages
python scripts/modular-installer.py list --available

# Uninstall components
python scripts/modular-installer.py uninstall web connection
```

This codebase emphasizes modular design, cross-platform compatibility, and modern C++ practices. Always respect the dependency hierarchy and use existing utilities before creating new ones.
