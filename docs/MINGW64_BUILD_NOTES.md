# MinGW64 Build Notes for Atom Project

## Overview

This document describes the changes made to successfully build the Atom project using MSYS2 MinGW64 toolchain (GCC 15.2.0) and maintain cross-compiler compatibility with MSVC.

## Build Environment

- **Toolchain**: MSYS2 MinGW64
- **Compiler**: GCC 15.2.0 (x86_64-w64-mingw32)
- **Generator**: MinGW Makefiles
- **Build Directory**: `build-mingw64`
- **CMake Preset**: `release-msys2` (or `debug-msys2`)

## Issues Fixed

### 1. Spdlog Linking Issue in atom-log Module

**Problem**:
The `atom-log` module was defining `SPDLOG_HEADER_ONLY` while attempting to link against the compiled spdlog library in MinGW64. This caused undefined reference errors:

```text
undefined reference to `__imp__ZN6spdlog7details7log_msgC1E...`
```

**Root Cause**:

- In MinGW64, spdlog is built as a compiled library (not header-only)
- The CMakeLists.txt was defining `SPDLOG_HEADER_ONLY` which expects inline implementations
- This created a mismatch between compile-time expectations and link-time reality

**Solution**:
Modified `atom/log/CMakeLists.txt`:

- Changed from `spdlog::spdlog` (compiled library) to `spdlog::spdlog_header_only` target
- Removed `SPDLOG_HEADER_ONLY` definition
- Added `SPDLOG_FMT_EXTERNAL` definition for consistency
- Removed `SPDLOG_USE_STD_FORMAT=1` to avoid potential compatibility issues

**Changes**:

```cmake
# Before (causing errors):
target_link_libraries(atom-log PUBLIC spdlog::spdlog ...)
target_compile_definitions(atom-log PUBLIC SPDLOG_USE_STD_FORMAT=1 SPDLOG_HEADER_ONLY)

# After (working):
target_link_libraries(atom-log PUBLIC spdlog::spdlog_header_only ...)
target_compile_definitions(atom-log PUBLIC SPDLOG_FMT_EXTERNAL)
```

## Cross-Compiler Compatibility

The codebase now supports both MSVC and MinGW64/GCC compilers. The following preprocessor macros can be used for compiler-specific code:

### Compiler Detection Macros

- **MSVC**: `#ifdef _MSC_VER`
- **MinGW**: `#ifdef __MINGW32__` or `#ifdef __MINGW64__`
- **GCC**: `#ifdef __GNUC__`

### Example Usage

```cpp
#ifdef _MSC_VER
    // MSVC-specific code
    #include <windows.h>
    #pragma warning(disable: 4996)
#elif defined(__MINGW64__) || defined(__MINGW32__)
    // MinGW-specific code
    #include <windows.h>
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#else
    // Other compilers (GCC/Clang on Linux, etc.)
    #include <unistd.h>
#endif
```

## Building with MinGW64

### Prerequisites

1. Install MSYS2 from <https://www.msys2.org/>
2. Open MSYS2 MinGW64 terminal
3. Install required packages:

```bash
pacman -S --needed base-devel mingw-w64-x86_64-toolchain
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
pacman -S mingw-w64-x86_64-spdlog mingw-w64-x86_64-fmt
```

### Build Commands

#### Using CMake Presets (Recommended)

```bash
# Configure
cmake --preset release-msys2

# Build
cmake --build --preset release-msys2 -j8

# Or for debug build
cmake --preset debug-msys2
cmake --build --preset debug-msys2 -j8
```

#### Manual CMake Configuration

```bash
# Configure
cmake -B build-mingw64 -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=D:/msys64/mingw64

# Build
cmake --build build-mingw64 -j8
```

## Build Output

After successful build, libraries are located in:

- **Shared Libraries (DLL)**: `build-mingw64/atom/*/lib*.dll`
- **Static Libraries**: `build-mingw64/atom/*/lib*.a`
- **Import Libraries**: `build-mingw64/atom/*/lib*.dll.a`

Example built libraries:

- `libatom-component.dll` (38.9 MB)
- `libatom-error.dll` (12.9 MB)
- `libatom-log.dll` (26.2 MB)
- Various static libraries: `libatom-utils.a`, `libatom-async.a`, etc.

## Known Issues and Limitations

1. **Asio Warning**: CMake shows a warning that asio is not found. This can be safely ignored if async features using Asio are not required, or install `mingw-w64-x86_64-asio`.

2. **Line Ending Warnings**: Git warnings about CRLF/LF conversions are informational and don't affect the build.

3. **Static vs Shared Libraries**: By default, most modules build as static libraries except:
   - `atom-error` (SHARED)
   - `atom-log` (SHARED)
   - `atom-component` (SHARED)

## Compatibility Notes

### MSVC vs MinGW Differences

1. **Standard Library**:
   - MSVC uses Microsoft STL implementation
   - MinGW uses libstdc++ (GCC's STL implementation)

2. **Windows API**:
   - Both support Windows API, but some extensions differ
   - Use `#ifdef _MSC_VER` for MSVC-specific extensions

3. **Exception Handling**:
   - MSVC uses SEH (Structured Exception Handling)
   - MinGW uses Dwarf-2 or SEH (depending on build)

4. **DLL Export/Import**:
   - Use `__declspec(dllexport)` and `__declspec(dllimport)` for both
   - MinGW also supports GCC visibility attributes

## Verification

To verify the build was successful:

```bash
# Check if all modules built
ls -lh build-mingw64/atom/*/libatom-*.{dll,a}

# Check DLL dependencies (example)
objdump -p build-mingw64/atom/log/libatom-log.dll | grep "DLL Name"

# Expected dependencies:
# - libgcc_s_seh-1.dll
# - libstdc++-6.dll
# - libwinpthread-1.dll
# - KERNEL32.dll, msvcrt.dll
```

## Future Improvements

1. Consider creating MinGW-specific presets with vcpkg support
2. Add CI/CD integration for automated MinGW builds
3. Document any platform-specific behavior differences
4. Add unit tests to verify cross-platform compatibility

## References

- MSYS2 Documentation: <https://www.msys2.org/docs/what-is-msys2/>
- CMake Presets: <https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html>
- Spdlog Documentation: <https://github.com/gabime/spdlog>
- MinGW-w64: <https://www.mingw-w64.org/>

## Changelog

### 2025-01-XX

- Fixed spdlog linking issue by switching to header-only target
- Documented MinGW64 build process
- Verified successful build with GCC 15.2.0
