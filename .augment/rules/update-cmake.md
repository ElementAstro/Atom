---
type: "manual"
---

# Review and Clean Up CMake Build Configuration

Review and clean up the CMake build configuration in the Atom project:

## Audit CMakeLists.txt

- Verify all CMake commands and configurations are correct and functional
- Remove any commented-out code, unused variables, or redundant configurations
- Ensure all defined options, targets, and dependencies are actually being
  used
- Confirm proper module inclusion and subdirectory additions

## Audit ./cmake/ Directory

- Review all `.cmake` files for correctness and necessity
- Identify and remove any unused or obsolete CMake modules/scripts
- Verify that all custom Find modules (e.g., `FindGTestFixed.cmake`) are
  being properly included and used
- Ensure all helper scripts and macros are referenced somewhere in the build
  system
- Check for duplicate functionality across different CMake files

## Verification

- Confirm that all CMake files in the `cmake/` directory are actually
  included/used by the main `CMakeLists.txt` or its subdirectories
- Ensure the build system works correctly after cleanup (all modules build,
  tests run, dependencies resolve)
- Document any files removed and why they were considered redundant

## Goal

Ensure a clean, functional CMake build system with no dead code or unused
files, where every configuration serves a clear purpose.
