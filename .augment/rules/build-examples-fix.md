---
type: "manual"
---

# Build All Example Projects

Build all example projects in the Atom repository's `example/` directory using
the MSVC compiler and vcpkg dependency manager. For each example, perform a
complete build cycle and systematically resolve any compilation, linking, or
runtime issues encountered.

## Specific Requirements

1. **Discovery Phase:**
   - Identify all example projects/subdirectories within `example/`
   - Determine the build system used for each example (CMake targets, standalone projects, etc.)
   - Verify which Atom modules each example depends on

2. **Build Process:**
   - Configure and build each example using the MSVC toolchain with vcpkg
   - Use appropriate CMake presets or build commands consistent with the project's build system
   - Enable parallel compilation where possible
   - Build in both Debug and Release configurations if feasible

3. **Issue Resolution:**
   - Fix all compilation errors (syntax errors, missing headers, type mismatches)
   - Resolve all linking errors (missing libraries, undefined symbols)
   - Address MSVC-specific compatibility issues using conditional compilation
   - Ensure cross-platform compatibility is maintained (don't break GCC/Clang)
   - Install any missing dependencies through vcpkg or package managers

4. **Verification:**
   - Confirm each example executable builds successfully without errors
   - If the examples have associated tests, run them to verify correctness
   - If no automated tests exist, perform basic smoke testing by running each
     example binary to ensure it executes without crashing

5. **Documentation:**
   - Track all changes made to fix build issues (file paths, errors, solutions)
   - Note any new dependencies added or build configuration changes
   - Document any platform-specific workarounds implemented

## Expected Deliverables

Provide a comprehensive summary containing:

- **Examples Built:** Complete list of all example projects found and their
  build status (success/failure)
- **Issues Encountered:** Detailed description of each build error, linking
  error, or runtime issue discovered
- **Resolutions Applied:** Specific fixes implemented for each issue (code
  changes, dependency installations, configuration updates)
- **Verification Results:** Confirmation that each example compiles, links,
  and runs successfully
- **Build Artifacts:** Location of generated executables and any relevant
  build outputs
- **Compatibility Notes:** Any MSVC-specific changes made and verification
  that cross-platform compatibility is preserved

Focus on achieving a complete, successful build of all examples while
maintaining code quality and cross-platform compatibility.
