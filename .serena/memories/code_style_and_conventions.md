# Code Style and Conventions

## Naming Conventions

Based on `STYLE_OF_CODE.md`:

### Variables

- **Convention:** camelCase
- **Example:** `int studentCount;`

### Constants

- **Convention:** UPPER_SNAKE_CASE
- **Example:** `const int MAX_STUDENTS = 100;`

### Functions

- **Convention:** camelCase, verb + noun format
- **Example:** `void displayStudentInfo(const Student& student);`

### Classes

- **Convention:** PascalCase (nouns or noun phrases)
- **Example:** `class Student { ... };`

### Class Member Variables

- **Convention:** camelCase with `m_` prefix (private members)
- **Example:**

  ```cpp
  class Student {
      int m_id;
      string m_name;
  };
  ```

### Enum Types

- **Convention:** UPPER_SNAKE_CASE
- **Example:** `enum Color { RED, GREEN, BLUE };`

### Namespaces

- **Convention:** PascalCase
- **Example:**

  ```cpp
  namespace Atom {
      // ...
  }
  ```

  Note: Split components into separate namespaces to avoid complexity.

### Files

- **Convention:** lowercase_with_underscores
- **Headers:** `.hpp` extension
- **Example:** `student_info.hpp`, `student_info.cpp`

## Formatting

Based on `.clang-format` (Google style with modifications):

- **Indentation:** 4 spaces
- **Tab width:** 8 spaces (but use spaces, not tabs)
- **Column limit:** 80 characters
- **Pointer alignment:** Left (`int* ptr`)
- **Brace style:** Attach (opening brace on same line)
- **Include sorting:** Enabled
- **Allow short functions on single line:** Yes

## Comments and Documentation

- **Style:** Doxygen-style comments for public APIs
- **Example:**

  ```cpp
  /**
   * @brief Display student information
   * @param student Student information
   * @return void
   * @throws atom::error::Exception Description of when exception is thrown
   */
  void displayStudentInfo(const Student& student);
  ```

## Include Guards

- **Style:** Use `#pragma once` (not include guards)
- **Example:**

  ```cpp
  #pragma once
  
  // Header content
  ```

## Namespace Conventions

All code is in the `atom` namespace with module-specific subnamespaces:

```cpp
namespace atom {
namespace algorithm {
    // Algorithm module code
}
}  // namespace atom
```

## Pre-commit Hooks

The project uses pre-commit for code quality:

- **C++:** trailing-whitespace, cmake-format
- **Python:** black, isort, ruff
- **General:** check-yaml, check-json, check-added-large-files

Install with: `pre-commit install`
Run manually: `pre-commit run --all-files`
