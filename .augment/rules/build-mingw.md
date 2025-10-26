---
type: "manual"
---

# MinGW64 Build Configuration

Build the entire Atom project using the MSYS2 (MinGW64) toolchain and resolve
all compilation/linking issues that arise during the build process.

## Specific Requirements

1. **Build Configuration:**
   - Use the MSYS2 MinGW64 environment (not MSVC)
   - Configure CMake to use the MinGW64 compiler toolchain
   - Ensure all build presets and scripts work correctly with MinGW64

2. **Issue Resolution:**
   - Fix ALL compilation errors, warnings, and linking issues encountered
   - Address any platform-specific incompatibilities between MSVC and MinGW/GCC
   - Resolve any missing dependencies or library linking problems

3. **Cross-Compiler Compatibility:**
   - When encountering code that is incompatible between MSVC and MinGW/GCC,
     use preprocessor macros to distinguish between environments
   - Use appropriate compiler detection macros such as:
     - `#ifdef _MSC_VER` for MSVC-specific code
     - `#ifdef __GNUC__` or `#ifdef __MINGW64__` for MinGW/GCC-specific code
   - Ensure the codebase remains compatible with both MSVC and MinGW64

4. **Testing:**
   - Verify that the build completes successfully without errors
   - Ensure all modules compile and link correctly
   - Test that the built binaries function properly

## Expected Outcome

A fully functional build of the Atom project using MSYS2 MinGW64, with all
compiler-specific issues resolved through appropriate conditional compilation
directives, maintaining cross-platform compatibility.
