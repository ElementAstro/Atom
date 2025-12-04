# MSYS2 Packaging Guide for Atom Project

## Overview

This guide documents the complete packaging workflow for the Atom project under MSYS2 MinGW64 environment, including all steps, configurations, and validation procedures.

## Environment Requirements

### Verified Build Environment

- **Operating System**: Windows with MSYS2
- **Compiler**: GCC 15.2.0 (Rev8, Built by MSYS2 project)
- **CMake**: 4.2.0-rc1
- **CPack**: 4.2.0-rc1 (included with CMake)
- **Generator**: MinGW Makefiles

### Required MSYS2 Packages

```bash
# Core build tools
pacman -S --needed base-devel mingw-w64-x86_64-toolchain

# CMake and build system
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja

# Dependencies
pacman -S mingw-w64-x86_64-spdlog mingw-w64-x86_64-fmt
pacman -S mingw-w64-x86_64-openssl mingw-w64-x86_64-zlib
```

## Packaging Workflow

### Step 1: Configure Build with Packaging Enabled

Configure the project with the MSYS2 preset and enable packaging:

```bash
cmake --preset release-msys2 -DATOM_ENABLE_PACKAGING=ON
```

**Configuration Output**:

- Build directory: `build-mingw64`
- All 19 Atom modules enabled
- Packaging configuration included
- Version: 0.0.0-d1058a5-dirty (Git-derived)

### Step 2: Build All Modules

Build the entire project with parallel jobs:

```bash
cmake --build --preset release-msys2 -j8
```

**Build Results**:

- All modules compiled successfully (100% completion)
- Libraries generated in `build-mingw64/atom/*/`
- Exit code: 0 (success)

### Step 3: Generate Distribution Package

Generate the distribution package using CPack:

```bash
# Generate ZIP package (recommended for MSYS2)
cpack --config build-mingw64/CPackConfig.cmake -G ZIP --verbose

# Alternative: Run from build directory
cd build-mingw64
cpack -G ZIP
```

**Package Generation Output**:

- Package name: `atom-0.0.0-d1058a5-dirty-windows-x64.zip`
- Package size: 5.74 MB
- Generator used: ZIP (NSIS fails without installation, which is expected)

### Step 4: Verify Package Contents

Extract and verify the generated package:

```bash
# Extract package
Expand-Archive -Path atom-0.0.0-d1058a5-dirty-windows-x64.zip -DestinationPath test_package

# Verify contents
Get-ChildItem test_package/atom-*/include -Recurse -File | Measure-Object
Get-ChildItem test_package/atom-*/lib -Recurse -File | Measure-Object
```

## Package Structure

The generated package contains the following structure:

```
atom-0.0.0-d1058a5-dirty-windows-x64/
├── bin/
│   └── [1 executable]
├── include/
│   └── atom/
│       ├── algorithm/
│       ├── async/
│       ├── components/
│       ├── connection/
│       ├── containers/
│       ├── error/
│       ├── image/
│       ├── io/
│       ├── log/
│       ├── memory/
│       ├── meta/
│       ├── search/
│       ├── secret/
│       ├── serial/
│       ├── sysinfo/
│       ├── system/
│       ├── type/
│       ├── utils/
│       ├── web/
│       └── atom_version_info.h
│       [Total: 729 header files - 664 .hpp + 65 .h]
├── lib/
│   ├── cmake/
│   │   └── [17 CMake configuration files]
│   ├── pkgconfig/
│   │   └── [2 pkg-config files]
│   ├── libatom-image.a
│   ├── libatom-utils.a
│   ├── libbase64.a
│   ├── libminizip.a
│   ├── libshortcut_detector.dll
│   ├── libshortcut_detector.dll.a
│   ├── libshortcut_detector_static.a
│   └── libtinyxml2.a
│   [Total: 7 static libraries + 1 DLL + supporting files]
└── share/
    └── [1 documentation file]
```

### Package Contents Summary

| File Type | Count | Description |
|-----------|-------|-------------|
| Header files (.hpp) | 664 | C++ header files |
| Header files (.h) | 65 | C header files |
| Static libraries (.a) | 7 | Linkable static libraries |
| Shared library (.dll) | 1 | Dynamic link library |
| CMake files (.cmake) | 17 | CMake configuration files |
| pkg-config files (.pc) | 2 | pkg-config metadata |
| JSON files | 1 | Package metadata |
| IPP files | 1 | Implementation file |

## Using the Package

### Method 1: CMake Integration

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyProject CXX)

# Set Atom package location
set(CMAKE_PREFIX_PATH "/path/to/atom-package")

# Include Atom headers
include_directories(${CMAKE_PREFIX_PATH}/include)

# Link against Atom libraries
add_executable(my_app main.cpp)
target_link_libraries(my_app 
    ${CMAKE_PREFIX_PATH}/lib/libatom-utils.a
    ${CMAKE_PREFIX_PATH}/lib/libatom-image.a
)
```

### Method 2: Direct Compilation

```bash
# Compile with Atom headers
g++ -std=c++20 -I/path/to/atom-package/include main.cpp \
    -L/path/to/atom-package/lib \
    -latom-utils -latom-image \
    -o my_app
```

### Method 3: pkg-config

```bash
# If pkg-config is configured
export PKG_CONFIG_PATH=/path/to/atom-package/lib/pkgconfig

# Compile using pkg-config
g++ main.cpp $(pkg-config --cflags --libs atom) -o my_app
```

## Known Issues and Limitations

### 1. Missing Library Install Rules

**Issue**: Not all Atom modules have install rules defined in their CMakeLists.txt files.

**Impact**: Only the following libraries are included in the package:

- libatom-image.a
- libatom-utils.a
- libbase64.a
- libminizip.a
- libtinyxml2.a
- libshortcut_detector (DLL and static)

**Modules with Install Rules**:

- atom/extra/* (most extra modules)
- atom/utils
- atom/sysinfo/hardware/memory

**Modules Missing Install Rules**:

- atom-error
- atom-log
- atom-algorithm
- atom-async
- atom-components
- atom-connection
- atom-containers
- atom-io
- atom-meta
- atom-memory
- atom-search
- atom-secret
- atom-serial
- atom-system
- atom-type
- atom-web

**Solution**: Add install rules to each module's CMakeLists.txt:

```cmake
# In each module's CMakeLists.txt
install(TARGETS atom-<module>
    EXPORT atom-<module>-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
```

### 2. NSIS Installer Not Available

**Issue**: CPack attempts to generate an NSIS installer but fails because NSIS is not installed in MSYS2.

**Error Message**:

```
CPack Error: Cannot find NSIS compiler makensis
CPack Error: Could not read NSIS registry value
CPack Error: Cannot initialize the generator NSIS
```

**Impact**: No `.exe` installer is generated, but this doesn't affect the ZIP package generation.

**Solution Options**:

1. Use ZIP packages (recommended for MSYS2)
2. Install NSIS separately for Windows installer generation
3. Disable NSIS generator in CPackConfig.cmake

### 3. Package Size Considerations

**Current package size**: 5.74 MB (compressed)

**Considerations**:

- Package size will increase significantly if all missing libraries are added
- Consider creating separate packages for different components
- Debug symbols should be stripped for release packages

## Verification Checklist

Use this checklist to verify a successful packaging workflow:

- [ ] Environment configured (GCC, CMake, CPack installed)
- [ ] All required dependencies installed via pacman
- [ ] CMake configuration successful with `-DATOM_ENABLE_PACKAGING=ON`
- [ ] Build completes with exit code 0
- [ ] All 19 modules compiled successfully
- [ ] CPack generates ZIP file without fatal errors
- [ ] Package file exists and has reasonable size (>1 MB)
- [ ] Package extracts without errors
- [ ] Package contains `bin/`, `include/`, `lib/`, and `share/` directories
- [ ] Header files present in `include/atom/` (should be 700+ files)
- [ ] Static libraries present in `lib/` (.a files)
- [ ] CMake config files present in `lib/cmake/`
- [ ] Version info header exists: `include/atom/atom_version_info.h`

## Automated Packaging Script

Create a script to automate the entire packaging workflow:

```bash
#!/bin/bash
# Package Atom project for MSYS2 MinGW64

set -e  # Exit on error

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

echo "=== Atom Project Packaging Script ==="
echo "Project root: $PROJECT_ROOT"

# Step 1: Clean previous builds
echo "Step 1: Cleaning previous builds..."
rm -rf build-mingw64 _CPack_Packages *.zip

# Step 2: Configure with packaging enabled
echo "Step 2: Configuring CMake..."
cmake --preset release-msys2 -DATOM_ENABLE_PACKAGING=ON

# Step 3: Build all modules
echo "Step 3: Building all modules..."
cmake --build --preset release-msys2 -j$(nproc)

# Step 4: Generate package
echo "Step 4: Generating distribution package..."
cpack --config build-mingw64/CPackConfig.cmake -G ZIP

# Step 5: Verify package
echo "Step 5: Verifying package..."
PACKAGE_FILE=$(ls atom-*.zip 2>/dev/null | head -1)
if [ -f "$PACKAGE_FILE" ]; then
    echo "✓ Package generated successfully: $PACKAGE_FILE"
    echo "  Size: $(du -h "$PACKAGE_FILE" | cut -f1)"
    
    # Extract and verify contents
    VERIFY_DIR="package_verify"
    rm -rf "$VERIFY_DIR"
    unzip -q "$PACKAGE_FILE" -d "$VERIFY_DIR"
    
    HEADER_COUNT=$(find "$VERIFY_DIR" -name "*.hpp" -o -name "*.h" | wc -l)
    LIB_COUNT=$(find "$VERIFY_DIR" -name "*.a" | wc -l)
    
    echo "  Headers: $HEADER_COUNT"
    echo "  Libraries: $LIB_COUNT"
    
    if [ $HEADER_COUNT -gt 600 ] && [ $LIB_COUNT -gt 5 ]; then
        echo "✓ Package verification passed"
    else
        echo "✗ Package verification failed"
        exit 1
    fi
else
    echo "✗ Package generation failed"
    exit 1
fi

echo "=== Packaging Complete ==="
```

## Troubleshooting

### Package is Empty or Has Few Files

**Symptoms**: Package extracts but contains empty directories or very few files.

**Causes**:

1. Install rules not defined in module CMakeLists.txt
2. CPack configuration issues
3. Build artifacts not generated

**Solutions**:

1. Verify build completed successfully
2. Check that libraries exist in `build-mingw64/atom/*/`
3. Run manual install: `cmake --install build-mingw64 --prefix test_install`
4. Add missing install rules to module CMakeLists.txt files

### CPack Fails with Configuration Errors

**Symptoms**: CPack exits with configuration errors.

**Solutions**:

1. Ensure `-DATOM_ENABLE_PACKAGING=ON` was set during configuration
2. Verify PackagingConfig.cmake exists in cmake directory
3. Check that atom_setup_cpack function is called in main CMakeLists.txt

### Libraries Not Linking Correctly

**Symptoms**: Downstream projects fail to link against packaged libraries.

**Solutions**:

1. Verify library files (.a) are not corrupted
2. Check that all dependencies are included or documented
3. Use objdump to inspect library symbols: `objdump -t libatom-utils.a`
4. Ensure correct library order in link command

## Best Practices

1. **Version Management**: Use semantic versioning and update version numbers before packaging
2. **Clean Builds**: Always start with a clean build directory for releases
3. **Verification**: Test the package on a clean system before distribution
4. **Documentation**: Include README and LICENSE files in the package
5. **Size Optimization**: Strip debug symbols from release libraries
6. **Dependencies**: Document all external dependencies required by the package
7. **Testing**: Create automated tests that use the packaged libraries

## Future Improvements

1. **Complete Install Rules**: Add install rules for all missing modules
2. **Component Packages**: Create separate packages for different components (e.g., atom-core, atom-utils, atom-async)
3. **Debug Packages**: Generate separate debug symbol packages
4. **Automated Testing**: Add CI/CD pipeline for automated package generation and testing
5. **Package Metadata**: Include version, dependencies, and changelog in package
6. **CMake Package Config**: Generate proper CMake package configuration files for easier integration
7. **Dependency Bundling**: Option to bundle all dependencies (static linking)

## References

- CMake CPack Documentation: <https://cmake.org/cmake/help/latest/module/CPack.html>
- MSYS2 Packages: <https://packages.msys2.org/>
- Atom Project Repository: <https://github.com/ElementAstro/Atom>

## Changelog

### 2025-11-18

- Initial packaging workflow established for MSYS2 MinGW64
- Successfully generated ZIP package with headers and libraries
- Documented known issues with missing install rules
- Created comprehensive packaging guide
- Verified package structure and contents
