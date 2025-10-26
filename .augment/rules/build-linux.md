---
type: "manual"
---

# Linux-Style Build Configuration

Build the entire Atom project using Linux-style build configuration
(GCC/Clang toolchain) on the current Windows environment. During the build
process:

1. Configure the project to use a Linux-compatible build approach (e.g.,
   using MinGW, WSL, or similar Unix-like environment)
2. Attempt a complete build of all modules and components
3. Identify and fix ALL compilation errors, linking errors, and build
   failures that occur
4. Pay special attention to platform-specific and compiler-specific
   compatibility issues between different toolchains (MSVC vs GCC/MinGW)
5. When encountering compiler-specific incompatibilities, use preprocessor
   macros to conditionally compile code based on the compiler and platform:
   - Use `#ifdef _MSC_VER` for MSVC-specific code
   - Use `#ifdef __GNUC__` for GCC/MinGW-specific code
   - Use `#ifdef __clang__` for Clang-specific code
   - Use `#ifdef _WIN32` or `#ifdef __linux__` for platform-specific code
6. Ensure that the fixes maintain cross-platform compatibility and don't
   break builds on other platforms/compilers
7. Document any significant changes or workarounds needed for Linux-style
   build compatibility

## Goal

Achieve a successful, complete build of the Atom project using Linux-compatible
build tools while maintaining compatibility with other build environments
(especially MSVC) through appropriate use of conditional compilation.
