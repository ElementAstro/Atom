# Atom: MSVC + vcpkg Build Guide (Windows)

This document records the exact steps and source changes required to build Atom with the MSVC toolchain and vcpkg on Windows, while preserving cross‑platform compatibility.

## Prerequisites

- Visual Studio 2022 with C++ workload (MSVC 19.44+)
- CMake (use the VS-bundled CMake recommended)
- vcpkg (manifest mode; we vendor vcpkg in this repo or use external VCPKG_ROOT)

## One‑time setup

1. Clone or point vcpkg (manifest mode is already enabled via `vcpkg.json`):
   - Optional: `git clone https://github.com/microsoft/vcpkg.git vcpkg && vcpkg\bootstrap-vcpkg.bat`

2. Configure using Visual Studio generator and vcpkg toolchain:
   - Use VS-bundled CMake to avoid MSYS2/MinGW picking:
     "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe"

```pwsh
# Configure (x64)
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' `
  -S . -B out/build-msvc-vcpkg -G 'Visual Studio 17 2022' -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_FEATURE_FLAGS=manifests -DUSE_VCPKG=ON

# Build (Release)
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' `
  --build out/build-msvc-vcpkg --config Release --parallel
```

Notes:

- Ensure PATH does not inject MSYS2 tools during configure/build (they can break vcpkg port builds and Windows SDK detection).

## Notable source/cmake adjustments for MSVC

All changes are protected by compiler/OS guards to preserve cross‑platform builds.

1. vcpkg manifest (Windows compatibility)

- `vcpkg.json`: `readline` is not available on Windows. We gated it with platform condition:
  - `{ "name": "readline", "platform": "!windows" }`

1. Disable dbghelp usage on MSVC to avoid SDK header conflicts

- Global CMake (root `CMakeLists.txt`): `add_compile_definitions(ATOM_DISABLE_DBGHELP)`
- `atom/system/debug/crash.cpp`, `atom/meta/abi.hpp`, `atom/log/loguru.cpp`:
  - On MSVC, do not include/use `<dbghelp.h>`; guard minidump/demangle paths with `#if !defined(ATOM_DISABLE_DBGHELP)`.
  - Keep Windows includes lean (`NOMINMAX`, `WIN32_LEAN_AND_MEAN`).

1. Fix Windows SDK arch detection and include order

- Root `CMakeLists.txt`: add `_AMD64_` for certain SDK detection edge cases: `add_compile_definitions(_AMD64_)`.
- Prefer including `<windows.h>` before other Win headers and set `NOMINMAX` and `WIN32_LEAN_AND_MEAN` in Windows TU’s that hit SDK headers.

1. C++20 portability fixes

- `atom/io/core/io.hpp`:
  - Include `<format>` for MSVC and use a C++20-safe conversion from `std::filesystem::file_time_type` to `system_clock` (avoid `std::chrono::file_clock::to_sys` which is C++23).
- `atom/system/debug/crash.cpp` and other files using `std::format`: include `<format>` explicitly for MSVC.
- `atom/system/clipboard/clipboard.hpp`:
  - Relaxed several `constexpr` methods to non-constexpr for MSVC’s current evaluation rules on classes with `std::optional`/error handling.

1. Windows POSIX/shim portability

- `atom/system/process/pidwatcher.hpp`: define `using pid_t = int;` under `_WIN32`.
- `atom/system/scheduling/crontab.cpp`: use `_popen/_pclose` on Windows; keep `popen/pclose` elsewhere.
- `atom/system/debug/nodebugger.cpp`: use `__debugbreak()` instead of `__builtin_trap()` on MSVC.

1. Serial port GUID and SetupAPI usage

- `atom/serial/core/scanner.cpp` and `atom/serial/platform/serial_port_win.hpp`:
  - Include `<ntddser.h>` (with `<initguid.h>` before it when needed) to get `GUID_DEVINTERFACE_COMPORT`.
  - Link `setupapi.lib` (pragma or target link) and keep lean Windows includes.

1. Import libraries for DLLs (linking in dependent targets)

- Some DLL targets lacked explicit `__declspec(dllexport)`; MSVC then didn’t emit `.lib` import libraries by default.
- Enabled import lib generation:
  - `atom/error/CMakeLists.txt`: `set_target_properties(atom-error PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)`
  - `atom/log/CMakeLists.txt`: `set_target_properties(loguru PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)`

1. Env utilities headers for MSVC

- `atom/system/info/env.hpp`: include `<filesystem>`, `<algorithm>`, `<sstream>` and declare the relevant `std::mutex` members (already present) to fix missing-type errors on MSVC when templates are instantiated.

## Build status

- Full solution builds successfully on MSVC (Visual Studio 2022) with vcpkg in Release configuration after the above adjustments.
- Artifacts are under `out/build-msvc-vcpkg/atom/**/Release/`.

## Cross‑platform notes

- All changes are guarded with `#ifdef _WIN32` or `#ifdef _MSC_VER` and do not affect Linux/macOS builds.
- Where newer C++23 APIs were used previously, fallbacks are supplied to keep code valid under C++20 across compilers.

## Troubleshooting

- If you see SDK parse errors like "No Target Architecture" (winnt.h) or odd typedef errors in `shared/ifdef.h`:
  - Verify Windows headers appear before secondary SDK headers, define `NOMINMAX`, `WIN32_LEAN_AND_MEAN`, and ensure `_AMD64_` is defined.
- If a dependent MSVC target can’t link a local DLL (LNK1181 missing .lib):
  - Add `WINDOWS_EXPORT_ALL_SYMBOLS ON` or annotate exported symbols with `__declspec(dllexport)`.
- If vcpkg fails to restore ports due to `builtin-baseline` in shallow clone:
  - `git -C vcpkg fetch origin <baseline-commit> --depth=1` and rerun configure.

## Rebuild commands

```pwsh
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' `
  -S . -B out/build-msvc-vcpkg -G 'Visual Studio 17 2022' -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_FEATURE_FLAGS=manifests -DUSE_VCPKG=ON

& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' `
  --build out/build-msvc-vcpkg --config Release --parallel
```

## Summary of edited files

- CMake: `CMakeLists.txt`, `atom/error/CMakeLists.txt`, `atom/log/CMakeLists.txt`, `vcpkg.json`
- Windows/MSVC guards and includes: multiple in `atom/system/**`, `atom/io/core/io.hpp`, `atom/system/debug/crash.*`, `atom/meta/abi.hpp`, `atom/serial/**`
- Portability fixes: `atom/system/clipboard/clipboard.hpp`, `atom/system/scheduling/crontab.cpp`, `atom/system/process/pidwatcher.hpp`, `atom/system/process/process_manager.cpp`

If you want these changes committed, let me know and I’ll prepare a clean commit set.
