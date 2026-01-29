# atom/log - Logging Framework

[根目录](../../CLAUDE.md) > **log**

---

## Module Overview

The `atom/log` module provides a comprehensive, high-performance logging framework built on top of spdlog. It offers synchronous and asynchronous logging capabilities, memory-mapped file logging for high-volume scenarios, and centralized log management with search and analysis features.

**Key Responsibilities:**

- Thread-safe logging with multiple sinks (console, file, rotating files)
- Asynchronous logging for minimal performance impact
- Memory-mapped file logging for high-throughput scenarios
- Centralized log management with search and analysis
- Log file upload and aggregation capabilities

---

## Module Structure

```
atom/log/
├── atomlog.hpp           # Main header with convenience macros
├── logger.hpp            # Core Logger class
├── async_logger.hpp      # AsynchronousLogger class
├── log_manager.hpp       # LogManager for centralized management
└── mmap_logger.hpp       # Memory-mapped logger for high-volume logging
```

---

## Public Interfaces

### Convenience Macros

```cpp
#include "atom/log/atomlog.hpp"

// Basic logging macros
ATOM_INFO("Message: {}", value);
ATOM_WARN("Warning: {}", warning);
ATOM_ERROR("Error: {}", error);
ATOM_DEBUG("Debug: {}", debug);
ATOM_TRACE("Trace: {}", trace);

// Conditional logging
ATOM_INFO_IF(condition, "Conditional message: {}", value);

// Logging with exceptions
ATOM_ERROR_EXCEPTION(e, "Exception occurred: {}", context);

// Named logger
ATOM_LOG_INFO("my_logger", "Message to specific logger: {}", value);
```

### Core Logger

```cpp
class Logger {
public:
    using Ptr = std::shared_ptr<Logger>;

    static Logger::Ptr get(std::string_view name);

    // Logging methods
    template <typename... Args>
    void info(fmt::format_string<Args...> fmt, Args&&... args);

    template <typename... Args>
    void warn(fmt::format_string<Args...> fmt, Args&&... args);

    template <typename... Args>
    void error(fmt::format_string<Args...> fmt, Args&&... args);

    template <typename... Args>
    void debug(fmt::format_string<Args...> fmt, Args&&... args);

    template <typename... Args>
    void trace(fmt::format_string<Args...> fmt, Args&&... args);

    // Configuration
    void setLevel(spdlog::level::level_enum level);
    spdlog::level::level_enum getLevel() const;
    void flush();
    void flush_on(spdlog::level::level_enum level);
};
```

### Asynchronous Logger

```cpp
class AsynchronousLogger : public Logger {
public:
    static std::shared_ptr<AsynchronousLogger> get(std::string_view name);

    // Asynchronous flush
    void flush() override;

    // Queue size monitoring
    size_t getQueueSize() const;
    bool isQueueOverflowing() const;
};
```

### Memory-Mapped Logger

```cpp
class MmapLogger {
public:
    MmapLogger(std::string_view filename, size_t max_size = 1024 * 1024);

    void log(std::string_view message);
    void flush();
    void close();

    // For high-volume logging scenarios
    void enableCompression(bool enable = true);
    size_t getCurrentSize() const;
};
```

### Log Manager

```cpp
class LogManager {
public:
    static LogManager& instance();

    // Log scanning
    void scanLogsFolder(std::string_view folder_path);

    // Log search
    std::vector<LogEntry> searchLogs(std::string_view search_term);
    std::vector<LogEntry> searchLogsByLevel(LogLevel level);
    std::vector<LogEntry> searchLogsByTimeRange(TimeRange range);

    // Log analysis
    void analyzeLogs();
    std::map<LogLevel, size_t> getLogCounts();

    // File operations
    bool uploadFile(std::string_view file_path);

    // Get log entries
    std::vector<LogEntry> getAllLogs() const;
};
```

---

## Dependencies

### Required Dependencies

- **spdlog** - Core logging framework
- **atom-error** - Error handling
- **atom-utils** - Utility functions
- **ZLIB** - Compression support for mmap logger

### Optional Dependencies

- **fmt** - Format library (usually required by spdlog)

---

## Usage Examples

### Basic Logging

```cpp
#include "atom/log/atomlog.hpp"

void processRequest(Request req) {
    ATOM_INFO("Processing request: {}", req.id);

    try {
        // ... process request
        ATOM_INFO("Request {} completed successfully", req.id);
    } catch (const std::exception& e) {
        ATOM_ERROR("Request {} failed: {}", req.id, e.what());
    }
}
```

### Creating Named Loggers

```cpp
#include "atom/log/logger.hpp"

void setupLoggers() {
    // Create logger with specific settings
    auto logger = Logger::get("network");
    logger->setLevel(spdlog::level::debug);
    logger->info("Network logger initialized");
}
```

### Asynchronous Logging

```cpp
#include "atom/log/async_logger.hpp"

void setupAsyncLogging() {
    auto async_logger = AsynchronousLogger::get("async_app");
    async_logger->setLevel(spdlog::level::info);

    // For high-throughput scenarios
    for (int i = 0; i < 1000000; ++i) {
        async_logger->info("Message {}", i);
    }
}
```

### Memory-Mapped File Logging

```cpp
#include "atom/log/mmap_logger.hpp"

void highVolumeLogging() {
    MmapLogger mmap_logger("high_volume.log", 10 * 1024 * 1024);

    // Enable compression for smaller log files
    mmap_logger.enableCompression(true);

    // Write logs
    for (int i = 0; i < 1000000; ++i) {
        mmap_logger.log(fmt::format("Entry {}", i));
    }

    mmap_logger.flush();
    mmap_logger.close();
}
```

### Log Management and Search

```cpp
#include "atom/log/log_manager.hpp"

void analyzeLogs() {
    auto& manager = LogManager::instance();

    // Scan folder for log files
    manager.scanLogsFolder("/var/log/myapp");

    // Search for specific patterns
    auto errors = manager.searchLogs("ERROR");
    auto db_errors = manager.searchLogs("database");

    // Search by time range
    TimeRange range{
        .start = std::chrono::system_clock::now() - std::chrono::hours(24),
        .end = std::chrono::system_clock::now()
    };
    auto recent_logs = manager.searchLogsByTimeRange(range);

    // Analyze log distribution
    manager.analyzeLogs();
    auto counts = manager.getLogCounts();
    ATOM_INFO("Errors: {}, Warnings: {}, Info: {}",
              counts[LogLevel::Error],
              counts[LogLevel::Warning],
              counts[LogLevel::Info]);
}
```

---

## Testing

The module includes comprehensive tests in `tests/log/`:

- `test_logger.cpp` - Core logger functionality tests

### Running Tests

```bash
# Build with tests
cmake --preset release
cmake --build --preset release -j

# Run all log tests
ctest -R "log_" --output-on-failure

# Run specific test
./tests/log/test_logger
```

---

## Build Options

### CMake Options

```cmake
# spdlog is required
find_package(spdlog REQUIRED)

# ZLIB for compression
find_package(ZLIB REQUIRED)

# Link dependencies
target_link_libraries(atom-log
    PUBLIC
        spdlog::spdlog
        atom-error
        atom-utils
        ZLIB::ZLIB
)

# Configure spdlog options
target_compile_definitions(atom-log PUBLIC SPDLOG_FMT_EXTERNAL)
```

---

## Configuration

### spdlog Configuration

The module uses spdlog with the following settings:

- **Header-only mode** - For better compatibility
- **External fmt** - Uses system fmt library
- **Thread-safe** - All logging operations are thread-safe
- **Automatic flush** - Configurable flush on error level

### Logger Patterns

Default log pattern:

```
[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v
```

Output example:

```
[2025-01-15 10:30:45.123] [info] [12345] Processing request
[2025-01-15 10:30:45.456] [error] [12345] Connection failed
```

---

## Performance Considerations

### Choosing the Right Logger

| Logger Type | Use Case | Overhead | Throughput |
|-------------|----------|----------|------------|
| **Logger** | General purpose | Low | Medium |
| **AsynchronousLogger** | High-throughput | Very Low | High |
| **MmapLogger** | Very high volume | Minimal | Very High |

### Best Practices

1. **Use async logging** for production to minimize latency
2. **Set appropriate log levels** to reduce log volume
3. **Use conditional logging** for expensive operations:

   ```cpp
   ATOM_INFO_IF(isDebugMode(), "Expensive data: {}", computeExpensiveData());
   ```

4. **Flush strategically** - not on every log entry
5. **Use MmapLogger** for burst logging scenarios

---

## Log Levels

| Level | Value | Use Case |
|-------|-------|----------|
| **Trace** | 0 | Detailed diagnostic information |
| **Debug** | 1 | Diagnostic information for debugging |
| **Info** | 2 | General informational messages |
| **Warning** | 3 | Warning messages for potentially harmful situations |
| **Error** | 4 | Error messages for error events |
| **Critical** | 5 | Critical error messages |
| **Off** | 6 | Disable logging |

---

## Common Patterns

### Logging with Context

```cpp
void processFile(std::string_view filename) {
    ATOM_INFO("Starting to process file: {}", filename);

    try {
        // ... process file
        ATOM_INFO("File {} processed successfully", filename);
    } catch (const atom::error::Exception& e) {
        ATOM_ERROR("Failed to process file {}: {}", filename, e.what());
    }
}
```

### Conditional Logging

```cpp
ATOM_INFO_IF(isVerbose(), "Verbose logging enabled");
ATOM_WARN_IF(memoryUsage() > 0.9, "High memory usage: {}%", memoryUsage() * 100);
```

### Exception Logging

```cpp
try {
    riskyOperation();
} catch (const std::exception& e) {
    ATOM_ERROR_EXCEPTION(e, "Operation failed");
}
```

---

## Platform-Specific Features

### Windows

- File path handling with backslashes
- UTF-8 encoding support
- Performance counters for timing

### Linux

- Standard file system paths
- syslog integration (optional)
- Signal handling for log rotation

### macOS

- Standard Unix paths
- Apple system log integration (optional)
- Notification Center alerts (optional)

---

## See Also

- [atom/error](../error/CLAUDE.md) - Error handling (used by log module)
- [spdlog documentation](https://github.com/gabime/spdlog) - Underlying logging framework

---

## Change Log

### 2025-01-15

- Initial module documentation created
- Documented all logger types and interfaces
- Added usage examples and best practices
- Documented performance considerations
