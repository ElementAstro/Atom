---
type: "manual"
---

# Update GitHub CI/CD Workflows

Update the GitHub CI/CD workflow configuration files (`.github/workflows/*.yml`)
to align with the latest build system modifications in the Atom project. Ensure
comprehensive coverage of all build scenarios and complete functionality:

1. **Review Current Build System State**:
   - Examine the current CMake configuration in `CMakeLists.txt` and `cmake/` directory
   - Identify all build presets, options, and module configurations (e.g., `ATOM_BUILD_ALGORITHM`, `ATOM_BUILD_IMAGE`, etc.)
   - Review the enhanced build scripts (`scripts/build.sh` and `scripts/build.bat`)
   - Document all available build types (debug, release, relwithdebinfo) and build flags

2. **Audit Existing GitHub CI Workflows**:
   - Review all workflow files in `.github/workflows/`
   - Identify gaps between current CI configuration and actual build system capabilities
   - Check for outdated commands, missing build scenarios, or deprecated configurations

3. **Update CI Workflows to Cover All Build Scenarios**:
   - **Platform Coverage**: Ensure builds are tested on Linux, macOS, and Windows
   - **Build Type Coverage**: Include debug, release, and relwithdebinfo builds
   - **Module Coverage**: Test builds with different module combinations (selective module building)
   - **Feature Coverage**: Include builds with Python bindings, examples, tests, and documentation
   - **Compiler Coverage**: Test with different compilers (GCC, Clang, MSVC) where applicable
   - **Dependency Management**: Ensure proper vcpkg/Conan integration if used

4. **Ensure Complete Functionality**:
   - Add test execution steps (CTest) for all build configurations
   - Include code quality checks (formatting, linting) if applicable
   - Add artifact generation and upload for successful builds
   - Ensure proper caching of dependencies to optimize CI runtime
   - Add status badges and reporting mechanisms

5. **Validation**:
   - Verify that all updated workflows are syntactically correct
   - Ensure workflow triggers are appropriate (push, pull request, schedule, etc.)
   - Confirm that all necessary secrets and environment variables are documented
