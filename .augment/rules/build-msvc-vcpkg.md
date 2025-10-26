---
type: "always_apply"
---

# MSVC Build with vcpkg

Build the entire Atom project using MSVC (Microsoft Visual C++) compiler
with vcpkg as the dependency manager. During the build process:

1. Configure the project to use MSVC toolchain and vcpkg for dependencies
2. Attempt a complete build of all modules and components
3. Identify and fix ALL compilation, linking, and build failures
4. Pay special attention to platform-specific compatibility issues between
   MSVC and other compilers (MinGW, GCC, Clang)
5. When encountering compiler-specific incompatibilities, use preprocessor
   macros to conditionally compile code based on the compiler being used
   (e.g., `#ifdef _MSC_VER` for MSVC, `#ifdef __GNUC__` for GCC/MinGW)
6. Ensure that the fixes maintain cross-platform compatibility and don't
   break builds on other platforms
7. Document any significant changes or workarounds needed for MSVC

## Goal

Achieve a successful, complete build of the Atom project using the MSVC +
vcpkg toolchain while maintaining compatibility with other build
environments through appropriate use of conditional compilation.
