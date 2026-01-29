# atom/error - Error Handling Module

[根目录](../../CLAUDE.md) > **error**

---

## Module Overview

The `atom/error` module provides the foundational error handling infrastructure for the entire Atom framework. It offers comprehensive exception management, stack trace capture, error context tracking, and flexible error reporting capabilities. This is a **core base module** with no dependencies, used by all other modules in the framework.

**Key Responsibilities:**

- Exception hierarchy with detailed error information (file, line, function, thread ID)
- Cross-platform stack trace capture with multiple backend support
- Error context propagation and scoped context management
- Global error handling and aggregation
- Platform-specific optimizations (Windows, Linux, macOS)

---

## Module Structure

```
atom/error/
├── error.hpp              # Main header (include all components)
├── exception.hpp           # Exception types aggregate header
├── stacktrace.hpp          # Stacktrace aggregate header
├── export.hpp             # DLL export definitions
├── core/                  # Core error types and definitions
│   ├── error_types.hpp     # ErrorSeverity, ErrorCategory, ErrorRecoveryStrategy
│   ├── error_codes.hpp     # Specific error code enumerations
│   └── error_metadata.cpp  # Error metadata implementation
├── exception/             # Exception class hierarchy
│   ├── exception_base.hpp/cpp     # Base Exception class with stack traces
│   ├── common_exceptions.hpp       # RuntimeError, LogicError, etc.
│   ├── argument_exceptions.hpp     # InvalidArgument, MissingArgument, etc.
│   ├── file_exceptions.hpp         # FileNotFound, FailToOpenFile, etc.
│   ├── object_exceptions.hpp       # ObjectAlreadyExist, ObjectNotExist, etc.
│   └── system_exceptions.hpp       # SystemErrorException, FailToLoadDll, etc.
├── stacktrace/            # Stack trace capture
│   ├── stacktrace.hpp/cpp          # Main StackTrace class
│   ├── stack_frame.hpp/cpp         # Individual stack frame representation
│   ├── stacktrace_utils.hpp/cpp    # Stack trace utilities
│   ├── backend_interface.hpp       # Backend abstraction
│   ├── builtin_backend.hpp/cpp     # Built-in stack trace implementation
│   ├── external_backends.hpp/cpp   # External library integrations
│   └── backend_factory.cpp        # Backend selection factory
├── context/               # Error context tracking
│   ├── error_context.hpp/cpp       # Error context with metadata
│   ├── context_manager.hpp/cpp    # Global context management
│   └── scoped_context.hpp/cpp      # RAII-style scoped context
└── handler/               # Error handling utilities
    ├── error_reporter.hpp/cpp      # Error reporting and logging
    ├── error_aggregator.hpp/cpp    # Error aggregation and collection
    └── global_handler.hpp/cpp      # Global error handler setup
```

---

## Public Interfaces

### Core Types

```cpp
namespace atom::error {

// Error severity levels
enum class ErrorSeverity : int {
    Trace, Debug, Info, Warning, Error, Critical, Fatal
};

// Error categories for grouping
enum class ErrorCategory : int {
    Unknown, System, Application, Network, IO, Memory,
    Security, Configuration, Validation, Business, External
};

// Recovery strategies
enum class ErrorRecoveryStrategy : int {
    None, Retry, Fallback, UserIntervention, Restart, Ignore
};

// Convert enums to string
constexpr std::string_view severityToString(ErrorSeverity);
constexpr std::string_view categoryToString(ErrorCategory);
constexpr std::string_view recoveryStrategyToString(ErrorRecoveryStrategy);

}  // namespace atom::error
```

### Exception Classes

```cpp
// Base exception with automatic stack trace capture
class Exception : public std::exception {
public:
    template <typename... Args>
    Exception(const char* file, int line, const char* func, Args&&... args);

    const char* what() const noexcept override;
    std::string getFile() const;
    int getLine() const;
    std::string getFunction() const;
    std::string getMessage() const;
    std::thread::id getThreadId() const;

    // Static helper for rethrowing with nested context
    template <typename... Args>
    static void rethrowNested(Args&&... args);
};

// Common exception types
class RuntimeError : public Exception { /* ... */ };
class LogicError : public Exception { /* ... */ };
class NullPointer : public Exception { /* ... */ };
class NotFound : public Exception { /* ... */ };
class OutOfRange : public Exception { /* ... */ };
// ... and more

// Convenience macros
#define THROW_EXCEPTION(...) \
    throw atom::error::Exception(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                 ATOM_FUNC_NAME, __VA_ARGS__)
#define THROW_RUNTIME_ERROR(...) THROW_EXCEPTION(__VA_ARGS__)
#define THROW_LOGIC_ERROR(...) THROW_EXCEPTION(__VA_ARGS__)
// ... many more
```

### Stack Trace

```cpp
class StackTrace {
public:
    StackTrace();
    std::string toString() const;
    std::vector<StackFrame> getFrames() const;
    size_t size() const;
    bool isEmpty() const;
};

// Individual stack frame
struct StackFrame {
    std::string address;
    std::string symbol;
    std::string filename;
    std::string line_number;
    std::string module_name;
};
```

### Error Context

```cpp
// Scoped error context (RAII)
class ScopedContext {
public:
    template <typename... Args>
    ScopedContext(std::string_view key, Args&&... args);
    ~ScopedContext();
};

// Context management
class ContextManager {
public:
    static ContextManager& instance();
    void setContext(std::string_view key, std::string_view value);
    std::optional<std::string> getContext(std::string_view key) const;
    void clearContext();
    ErrorContext getCurrentContext() const;
};
```

---

## Dependencies

### Required Dependencies

- **None** (base module)

### Platform-Specific Dependencies

- **Windows**: `dbghelp`, `psapi` (for stack traces)
- **Linux**: `dl` (for dynamic loading)
- **macOS**: System frameworks (built-in)

### Optional Stack Trace Backends

- `ATOM_USE_CPPTRACE` - cpptrace library (recommended)
- `ATOM_USE_BACKWARD_CPP` - backward-cpp with source snippets
- `ATOM_USE_BOOST_STACKTRACE` - Boost.Stacktrace
- `ATOM_USE_LIBUNWIND` - Portable unwinding library
- `ATOM_USE_EXECINFO` - POSIX backtrace (glibc, macOS)
- `ATOM_USE_LIBBACKTRACE` - GCC's backtrace library
- `ATOM_USE_STD_STACKTRACE` - C++23 std::stacktrace
- `ATOM_USE_ABSEIL_STACKTRACE` - Abseil stacktrace

---

## Usage Examples

### Basic Exception Throwing

```cpp
#include "atom/error/error.hpp"

void processValue(int value) {
    if (value < 0) {
        THROW_INVALID_ARGUMENT("Value cannot be negative: {}", value);
    }
    if (value > 1000) {
        THROW_OUT_OF_RANGE("Value exceeds maximum: {}", value);
    }
    // ... process value
}
```

### Exception with Context

```cpp
void loadData(std::string_view filename) {
    // Add context for all exceptions in this scope
    ScopedContext ctx("file", filename);

    // Any exception thrown here will include the file context
    std::ifstream file(filename);
    if (!file) {
        THROW_FAIL_TO_OPEN_FILE(filename);
    }
    // ... process file
}
```

### Stack Trace Capture

```cpp
#include "atom/error/stacktrace.hpp"

void functionWithStacktrace() {
    try {
        THROW_RUNTIME_ERROR("Error occurred");
    } catch (const atom::error::Exception& e) {
        std::cout << e.what() << std::endl;
        // Output includes:
        // - Error message
        // - File, line, function
        // - Thread ID
        // - Full stack trace
    }
}
```

### Error Recovery with Strategy

```cpp
auto riskyOperation() -> atom::error::Expected<Result> {
    try {
        Result result = performOperation();
        return result;
    } catch (const atom::error::Exception& e) {
        // Check recovery strategy and attempt recovery
        if (canRecoverFrom(e)) {
            return recover();
        }
        return atom::error::Unexpected<std::string>(e.what());
    }
}
```

---

## Testing

The module includes comprehensive unit tests in `tests/error/`:

- `test_error.cpp` - Core exception handling tests
- `test_stacktrace.cpp` - Stack trace capture tests
- `test_error_formatting.cpp` - Error message formatting tests
- `test_error_integration.cpp` - Integration tests
- `test_error_system.cpp` - System-specific error tests

### Running Tests

```bash
# Build with tests
cmake --preset release
cmake --build --preset release -j

# Run all error tests
ctest -R "error_" --output-on-failure

# Run specific test
./tests/error/test_error
```

---

## Build Options

### CMake Options

```cmake
# Enable specific stack trace backend
-DCMAKE_BUILD_TYPE=Release
-DAQTOM_USE_CPPTRACE=ON          # Use cpptrace (recommended)
-DAQTOM_USE_BACKWARD_CPP=ON       # Use backward-cpp
-DAQTOM_USE_BOOST_STACKTRACE=ON   # Use Boost
-DAQTOM_USE_STD_STACKTRACE=ON     # Use C++23 std::stacktrace
```

### Platform Notes

**Windows (MSVC)**

- Uses Windows StackWalk API for built-in backend
- Set `WINDOWS_EXPORT_ALL_SYMBOLS=ON` for MinGW compatibility

**Linux**

- Uses `backtrace()` from execinfo.h for built-in backend
- May require `-rdynamic` linker flag for better symbol resolution

**macOS**

- Uses `backtrace()` from system libraries
- Consider using cpptrace or backward-cpp for better output

---

## Common Patterns

### Throwing Exceptions with File Information

```cpp
void processData(const Data& data) {
    if (!data.isValid()) {
        THROW_EXCEPTION("Invalid data: ", data.toString());
    }
}
```

### System Error Handling

```cpp
#include <windows.h>
#include "atom/error/exception.hpp"

bool loadLibrary(std::string_view path) {
    HMODULE handle = LoadLibraryA(path.data());
    if (!handle) {
        THROW_FAIL_TO_LOAD_DLL(path);
    }
    return true;
}
```

### Nested Exception Handling

```cpp
void outerFunction() {
    try {
        innerFunction();
    } catch (...) {
        THROW_NESTED_EXCEPTION("Context for inner failure");
    }
}
```

---

## Platform-Specific Features

### Windows Stack Trace

- Uses `StackWalk64` and `SymFromAddr`
- Requires `dbghelp.lib` and `psapi.lib`
- Automatic symbol loading from PDB files

### Linux Stack Trace

- Uses `backtrace()` from `execinfo.h`
- Can be enhanced with `libbacktrace` or `libunwind`
- Symbol resolution via `dladdr()`

### macOS Stack Trace

- Uses `backtrace()` from system libraries
- Can use cpptrace for better symbolization
- Consider using `atos` for symbol resolution

---

## See Also

- [atom/log](../log/CLAUDE.md) - Logging module (uses error module)
- [Module Dependencies](../../cmake/ModuleDependenciesData.cmake) - Dependency definitions

---

## Change Log

### 2025-01-15

- Initial module documentation created
- Documented all exception types and stack trace backends
- Added usage examples and testing information
- Documented platform-specific features
