---
type: "manual"
---

# Update xmake Build Configuration

Update the xmake build configuration to align with the latest modifications
in the Atom project. Specifically:

1. **Analyze Current State**: Examine the existing xmake.lua files throughout
   the project to understand the current build configuration structure.

2. **Identify Recent Changes**: Review recent code changes (particularly in
   CMakeLists.txt files and source code) to identify:
   - New source files that need to be added to the build
   - Removed files that should be excluded
   - New dependencies or libraries that have been introduced
   - Changed module structures or organization
   - Updated API usage patterns

3. **Research Latest xmake APIs**: Use web search to find the most current
   xmake documentation and best practices for:
   - Modern xmake syntax and conventions
   - Recommended ways to handle C++20/C++23 features
   - Proper dependency management approaches
   - Cross-platform build configuration
   - Integration with vcpkg or other package managers if applicable

4. **Update Build Configuration**: Modify all xmake.lua files to:
   - Use the latest xmake API syntax and features
   - Include all current source files in their respective targets
   - Properly configure all dependencies and link requirements
   - Ensure cross-platform compatibility (Windows/Linux/macOS)
   - Match the module structure defined in CMake configuration

5. **Verification**: Ensure that:
   - All source files in the project can be successfully built
   - No files are missing from the build configuration
   - The build configuration mirrors the functionality of the CMake setup
   - All modules and their dependencies are correctly specified

Use web search proactively to verify you're using current xmake best
practices and the latest API features.
