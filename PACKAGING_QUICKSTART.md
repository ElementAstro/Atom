# Atom Project - MSYS2 Packaging Quick Start

## Prerequisites Check

```bash
# Verify tools are installed
gcc --version      # Should show GCC 15.2.0+
cmake --version    # Should show CMake 4.2.0+
cpack --version    # Should show CPack 4.2.0+
```

## One-Command Package Build

```bash
# From project root
cmake --preset release-msys2 -DATOM_ENABLE_PACKAGING=ON && \
cmake --build --preset release-msys2 -j8 && \
cpack --config build-mingw64/CPackConfig.cmake -G ZIP
```

## Expected Output

- **Package**: `atom-0.0.0-d1058a5-dirty-windows-x64.zip` (5.74 MB)
- **Contents**: 729 headers, 7 static libs, 1 DLL, CMake configs

## Package Contents

```
atom-VERSION-windows-x64/
├── bin/         (executables)
├── include/     (729 header files)
├── lib/         (7 static libs + cmake configs)
└── share/       (documentation)
```

## Quick Verification

```bash
# Extract and verify
unzip atom-*.zip -d test_verify
find test_verify -name "*.hpp" | wc -l  # Should show ~664
find test_verify -name "*.a" | wc -l    # Should show 7
```

## Known Limitations

1. **Missing libraries**: Only 7 of 19 modules have install rules
   - Solution: Add install rules to module CMakeLists.txt files
2. **NSIS error**: Expected in MSYS2 (use ZIP packages instead)

## Full Documentation

See `docs/MSYS2_PACKAGING_GUIDE.md` for complete details, troubleshooting, and best practices.

## Quick Links

- Build Guide: `docs/MINGW64_BUILD_NOTES.md`
- Packaging Guide: `docs/MSYS2_PACKAGING_GUIDE.md`
- Build Scripts: `scripts/build.bat`
