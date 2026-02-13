# Task Completion Checklist

When completing a development task in the Atom project, follow this checklist:

## 1. Code Quality

### Formatting

- [ ] Run `clang-format` on modified C++ files
- [ ] Run `black` and `isort` on modified Python files
- [ ] Run `cmake-format` on modified CMake files

### Linting

- [ ] Run `pre-commit run --all-files` to check all pre-commit hooks
- [ ] Fix any linting issues

### Style Verification

- [ ] Verify naming conventions (camelCase, PascalCase, UPPER_SNAKE_CASE)
- [ ] Check that private members have `m_` prefix
- [ ] Ensure files use `lowercase_with_underscores` naming
- [ ] Verify header files use `.hpp` extension
- [ ] Ensure `#pragma once` is used in headers

## 2. Testing

### Unit Tests

- [ ] Add tests for new functionality in `tests/<module>/`
- [ ] Run `ctest --output-on-failure` to verify all tests pass
- [ ] Run specific module tests: `ctest -R "<module>_.*"`

### Test Coverage

- [ ] Ensure new code has appropriate test coverage
- [ ] Add both positive and negative test cases
- [ ] Test edge cases and error conditions

### Build Verification

- [ ] Build with Debug configuration
- [ ] Build with Release configuration
- [ ] Verify no compilation warnings

## 3. Documentation

### Code Documentation

- [ ] Add Doxygen comments to public APIs
- [ ] Document function parameters and return values
- [ ] Add `@brief`, `@param`, `@return`, `@throws` tags as needed

### Module Documentation

- [ ] Update `atom/<module>/CLAUDE.md` if module changes were made
- [ ] Document new APIs, classes, or functions
- [ ] Update any relevant architecture diagrams

### Examples

- [ ] Add example code demonstrating new functionality
- [ ] Place examples in `example/<module>/`

## 4. Build System

### CMake Configuration

- [ ] Update `atom/<module>/CMakeLists.txt` if sources were added/removed
- [ ] Update module dependencies in `cmake/ModuleDependenciesData.cmake` if needed
- [ ] Verify CMake configure step succeeds

### Dependencies

- [ ] Declare new dependencies in module's CMakeLists.txt
- [ ] Use `find_package()` with `QUIET` for optional dependencies
- [ ] Conditionally compile with `#ifdef` checks for optional deps

## 5. Platform Considerations

### Cross-Platform Verification

- [ ] Test on Windows (if possible)
- [ ] Test on Linux (if possible)
- [ ] Test on macOS (if possible)
- [ ] Use platform-specific abstractions from `atom/system` module

### Windows-Specific

- [ ] Handle paths correctly (use forward slashes or proper escaping)
- [ ] Consider DLL export/import for Windows shared libraries
- [ ] Test with both MSVC and MinGW64 if applicable

## 6. Error Handling

### Error Management

- [ ] Integrate with `atom::error` system
- [ ] Use `ATOM_ERROR()`, `ATOM_WARN()`, `ATOM_INFO()` logging
- [ ] Add appropriate error contexts
- [ ] Handle exceptions properly

### Resource Management

- [ ] Use RAII for resource management
- [ ] Clean up resources in destructors
- [ ] Consider using `atom::memory` utilities for memory management

## 7. Integration

### Module Integration

- [ ] Verify module dependencies are correctly declared
- [ ] Test integration with dependent modules
- [ ] Update main module header `<module>.hpp` if new APIs are public

### Python Bindings (if applicable)

- [ ] Add Python bindings in `python/<module>/`
- [ ] Test Python bindings
- [ ] Update `python/<module>/__init__.py` for exports

## 8. Version Control

### Git Workflow

- [ ] Stage only relevant files (avoid `.vscode/`, `build/`, etc.)
- [ ] Write clear commit message following project conventions
- [ ] Use conventional commit format if applicable
- [ ] Create pull request if working on a branch

### Commit Message Format

```
<type>(<scope>): <description>

<body>

<footer>
```

Types: feat, fix, docs, style, refactor, test, chore

## 9. Final Checks

### Before Committing

- [ ] No debug/TODO comments left in production code
- [ ] No commented-out code blocks
- [ ] All TODOs have associated issues or explanations
- [ ] Verify no sensitive data (API keys, passwords) in code

### Verification

- [ ] Full build succeeds: `cmake --build build -j`
- [ ] All tests pass: `ctest --output-on-failure`
- [ ] Pre-commit hooks pass: `pre-commit run --all-files`
- [ ] Code compiles without warnings on target platforms

## Quick Checklist (Minimal)

For small changes, at minimum:

- [ ] Code is formatted (clang-format/black)
- [ ] Tests pass (ctest)
- [ ] Pre-commit hooks pass
- [ ] Public APIs have Doxygen comments
- [ ] Commit message is clear and descriptive
