# Atom Library AI Coding Instructions

This is the **Atom** library - a modular C++20 foundational library for astronomical software projects. It follows a strict dependency hierarchy and build system patterns.

## Architecture Overview

- **Modular Design**: 12+ independent modules (`algorithm`, `async`, `components`, `io`, `log`, `system`, etc.) with explicit dependencies defined in `cmake/module_dependencies.cmake`
- **Build Order**: `atom-error` (base) → `atom-log` → `atom-meta`/`atom-utils` → specialized modules like `atom-web`, `atom-async`
- **Cross-Platform**: Windows/Linux/macOS with platform-specific conditionals in `atom/macro.hpp`
- **Multi-Build System**: Both CMake and XMake support with feature parity

## Critical Patterns

### Module Structure Convention

Each module follows this pattern:

```
atom/<module>/
├── CMakeLists.txt           # Module build config with dependency checks
├── <module>.hpp             # May be compatibility header pointing to core/
└── core/<module>.hpp        # Actual implementation (newer pattern)
```

**Key**: Many headers like `algorithm.hpp` are compatibility redirects to `core/algorithm.hpp`. Always check for the core/ subdirectory.

### Dependency System

- Dependencies are **hierarchical**: `ATOM_<MODULE>_DEPENDS` in `cmake/module_dependencies.cmake`
- Dependency verification happens in each module's CMakeLists.txt:

```cmake
foreach(dep ${ATOM_ALGORITHM_DEPENDS})
  string(REPLACE "atom-" "ATOM_BUILD_" dep_var_name ${dep})
  # Auto-enables missing dependencies or warns
endforeach()
```

### Macro System (`atom/macro.hpp`)

- Platform detection: `ATOM_PLATFORM_WINDOWS/LINUX/APPLE`
- C++20 enforcement with fallback checks
- Boost integration controlled by `ATOM_USE_BOOST*` flags
- Use existing macros rather than raw `#ifdef`

## Build System Specifics

### CMake Workflow

```bash
# Configure with options
cmake -B build -DATOM_BUILD_EXAMPLES=ON -DATOM_BUILD_TESTS=ON
# Build specific modules
cmake --build build --target atom-algorithm
```

### XMake Workflow

```bash
# Configure options
xmake f --build_examples=y --build_tests=y
# Build all or specific targets
xmake build
```

**Build Scripts**: Use `build.bat` on Windows or `build.sh` on Unix. They parse options like `--examples`, `--tests`, `--python` and configure the appropriate build system.

## Testing Patterns

### Test Organization

- **Unit Tests**: `tests/<module>/test_*.hpp` with GoogleTest framework
- **Integration Tests**: `atom/tests/test.hpp` provides custom test registration with dependency tracking
- **Examples**: `example/<module>/*.cpp` - one executable per file, automatic CMake discovery

### Test Registration Pattern

```cpp
// In atom/tests/test.hpp system
ATOM_INLINE void registerTest(std::string name, std::function<void()> func,
                              bool async = false, double time_limit = 0.0,
                              bool skip = false,
                              std::vector<std::string> dependencies = {},
                              std::vector<std::string> tags = {});
```

## Development Workflows

### Adding New Modules

1. Create module directory under `atom/`
2. Add dependency entry in `cmake/module_dependencies.cmake`
3. Update `ATOM_MODULE_BUILD_ORDER`
4. Create corresponding test directory in `tests/`
5. Add example in `example/` if public-facing

### Key File Locations

- **Version Info**: `cmake/version_info.h.in` → `build/atom_version_info.h`
- **Platform Config**: `cmake/PlatformSpecifics.cmake`
- **Compiler Options**: `cmake/compiler_options.cmake`
- **External Deps**: `vcpkg.json` and XMake `add_requires()` statements

### Python Bindings

- Located in `python/` with pybind11
- Auto-detects module types from directory structure
- Each module gets its own Python binding file

## Module Integration Points

- **Error Handling**: All modules depend on `atom-error` - use its result types, not raw exceptions
- **Logging**: `atom-log` provides structured logging - prefer it over std::cout
- **Async Operations**: `atom-async` provides the async primitives - don't reinvent
- **Utilities**: `atom-utils` has common helpers - check before adding duplicates

## Code Conventions

- **C++20 Required**: Use concepts, ranges, source_location
- **RAII Everywhere**: Smart pointers, automatic resource management
- **Template Heavy**: Meta-programming in `atom/meta/` - extensive concept usage
- **Error Propagation**: Use `Result<T>` types from `atom-error`, not exceptions in normal flow
- **Documentation**: Doxygen format with `@brief`, `@param`, `@return`

When working on this codebase, always check module dependencies first, respect the build order, and follow the established patterns for testing and examples.
