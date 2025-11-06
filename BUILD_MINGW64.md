# MSYS2 MinGW64 Build Guide

This guide explains how to build the Atom project on Windows using the MSYS2 MinGW64 toolchain.

## Prerequisites

- Windows 10/11 x64
- MSYS2 installed (suggested path: `D:\msys64` or `C:\msys64`)
- CMake ≥ 3.21 (we use presets)
- Ninja or mingw32-make is not required explicitly; presets use the "MinGW Makefiles" generator

### Install MSYS2 and required packages

1. Download and install MSYS2: <https://www.msys2.org/>
2. Open the "MSYS2 MinGW x64" shell and run:

   ```bash
   pacman -Syu            # Full system update (may require restarting the shell)
   pacman -S --needed \
       mingw-w64-x86_64-gcc \
       mingw-w64-x86_64-cmake \
       mingw-w64-x86_64-make \
       mingw-w64-x86_64-openssl \
       mingw-w64-x86_64-zlib \
       mingw-w64-x86_64-pkgconf \
       mingw-w64-x86_64-libuv \
       mingw-w64-x86_64-sqlite3 \
       mingw-w64-x86_64-libusb \
       mingw-w64-x86_64-curl \
       mingw-w64-x86_64-opencv \
       mingw-w64-x86_64-tbb \
       mingw-w64-x86_64-leptonica \
       mingw-w64-x86_64-tesseract
   ```

   Notes:
   - OpenSSL and Zlib are required by this project; the preset passes `OPENSSL_ROOT_DIR` and `ZLIB_ROOT` to CMake.
   - Additional packages (OpenCV, libuv, sqlite3, libusb, curl, TBB) are used by optional modules and will be picked up automatically when present.

## Configure and build using CMake Presets

From a regular Windows PowerShell (or inside the MSYS2 MinGW64 shell), in the repository root:

```powershell
# Optional: ensure MSYS2 MinGW64 is on PATH for PowerShell sessions
if (Test-Path 'D:\msys64\mingw64\bin') { $env:Path = 'D:\msys64\mingw64\bin;D:\msys64\usr\bin;' + $env:Path }
if (Test-Path 'C:\msys64\mingw64\bin') { $env:Path = 'C:\msys64\mingw64\bin;C:\msys64\usr\bin;' + $env:Path }

# Configure (RelWithDebInfo) and build
cmake --preset relwithdebinfo-msys2
cmake --build --preset relwithdebinfo-msys2 -j 8
```

Artifacts are generated under `build-mingw64/` to avoid mixing with MSVC builds.

### Enable tests/examples

```powershell
cmake --preset relwithdebinfo-msys2 -DATOM_BUILD_TESTS=ON -DATOM_BUILD_EXAMPLES=ON
cmake --build --preset relwithdebinfo-msys2 -j 8
ctest --test-dir build-mingw64 --output-on-failure
```

## Notes on cross-compiler compatibility

- The codebase uses conditional compilation to handle MSVC vs GCC/MinGW differences via `atom/macro.hpp` and standard compiler macros (`_MSC_VER`, `__GNUC__`, `__MINGW64__`).
- The build system selects appropriate flags for GCC/Clang using `cmake/CompilerOptions.cmake`.

## Troubleshooting

- OpenSSL or Zlib not found: Ensure the MSYS2 `mingw64` packages for OpenSSL and Zlib are installed. The preset sets `OPENSSL_ROOT_DIR` and `ZLIB_ROOT` to `D:/msys64/mingw64`. If your MSYS2 is installed elsewhere, set them explicitly:

  ```powershell
  cmake --preset relwithdebinfo-msys2 -DOPENSSL_ROOT_DIR=E:/Tools/msys64/mingw64 -DZLIB_ROOT=E:/Tools/msys64/mingw64
  ```

- Missing `mingw32-make.exe`: Install `mingw-w64-x86_64-make` in MSYS2.
- Generator cache conflicts: We use a dedicated `build-mingw64/` directory for MSYS2 to avoid generator clashes.

## What the preset does

- Uses the "MinGW Makefiles" generator
- Builds into `build-mingw64/`
- Turns off vcpkg and uses MSYS2 libraries
- Points CMake to the MSYS2 `mingw64` prefix to discover OpenSSL/Zlib automatically
