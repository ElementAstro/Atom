# Locale Module

A comprehensive, cross-platform locale management system with advanced features for locale detection, validation, formatting, and caching.

## Overview

The locale module provides a modern, feature-rich API for handling system locale information across Windows, Linux, and macOS platforms. It includes advanced caching, performance monitoring, validation utilities, and extensive formatting capabilities.

## Features

### Core Features

- **Cross-platform support**: Windows, Linux, and macOS
- **Advanced locale detection**: Automatic system locale detection with fallback mechanisms
- **Comprehensive validation**: Multi-level locale validation with detailed error reporting
- **Rich formatting**: Number, currency, date/time, and list formatting with locale awareness
- **Intelligent caching**: Memory and persistent caching with configurable strategies
- **Performance monitoring**: Built-in metrics collection and performance analysis

### Enhanced Capabilities

- **Locale compatibility checking**: Cross-platform compatibility analysis
- **Automatic fallback**: Smart fallback to compatible locales
- **Configuration management**: Flexible configuration via files or environment variables
- **Thread-safe operations**: Full thread safety for concurrent access
- **Extensible architecture**: Plugin-friendly design for custom locale providers

## Quick Start

### Basic Usage

```cpp
#include "atom/sysinfo/locale/locale.hpp"

using namespace atom::system;

// Get current system locale
auto localeInfo = getSystemLanguageInfo();
std::cout << "Current locale: " << localeInfo.localeName << std::endl;

// Format numbers and currency
LocaleFormatter formatter("en_US.UTF-8");
std::cout << "Number: " << formatter.formatNumber(1234.56, 2) << std::endl;
std::cout << "Currency: " << formatter.formatCurrency(123.45) << std::endl;

// Format dates and times
auto now = std::chrono::system_clock::now();
std::cout << "Date: " << formatter.formatDate(now) << std::endl;
std::cout << "Time: " << formatter.formatTime(now) << std::endl;
```

### Advanced Usage with LocaleManager

```cpp
#include "atom/sysinfo/locale/locale.hpp"

using namespace atom::system;

// Create a locale manager
LocaleManager manager;

// Get available locales
auto available = manager.getAvailableLocales();
std::cout << "Available locales: " << available.size() << std::endl;

// Validate a locale
if (manager.validateLocale("de_DE.UTF-8")) {
    std::cout << "German locale is available" << std::endl;
}

// Set up locale change notifications
auto callbackId = manager.registerChangeCallback(
    [](const std::string& oldLocale, const std::string& newLocale) {
        std::cout << "Locale changed from " << oldLocale
                  << " to " << newLocale << std::endl;
    }
);

// Change locale
manager.setLocale("de_DE.UTF-8");
```

### Locale Detection and Validation

```cpp
#include "atom/sysinfo/locale/validator.hpp"

using namespace atom::system::locale;

// Detect best locale
std::string detected = LocaleDetector::detectSystemLocale();
std::cout << "Detected locale: " << detected << std::endl;

// Validate locale with detailed results
auto result = LocaleValidator::validate("en_US.UTF-8", ValidationLevel::Complete);
if (result.isValid) {
    std::cout << "Locale is valid (confidence: "
              << result.confidenceScore << ")" << std::endl;
} else {
    std::cout << "Validation errors:" << std::endl;
    for (const auto& error : result.errors) {
        std::cout << "  - " << error << std::endl;
    }

    std::cout << "Suggestions:" << std::endl;
    for (const auto& suggestion : result.suggestions) {
        std::cout << "  - " << suggestion << std::endl;
    }
}
```

### Configuration and Caching

```cpp
#include "atom/sysinfo/locale/config.hpp"

using namespace atom::system::locale;

// Configure the locale system
auto& configManager = LocaleConfigManager::getInstance();
configManager.loadFromEnvironment();

LocaleConfig config = configManager.getConfig();
config.cache.strategy = CacheStrategy::Hybrid;
config.cache.timeout = std::chrono::seconds(600);
config.performance.enableMetrics = true;
configManager.setConfig(config);

// Use caching
CacheConfig cacheConfig;
cacheConfig.strategy = CacheStrategy::Memory;
cacheConfig.maxMemoryEntries = 100;

LocaleCache cache(cacheConfig);
LocaleInfo info = getSystemLanguageInfo();
cache.store("current", info);

// Retrieve from cache
auto cached = cache.retrieve("current");
if (cached) {
    std::cout << "Retrieved from cache: " << cached->localeName << std::endl;
}
```

## Architecture

### Module Structure

```
locale/
├── common.hpp/cpp          # Common types and utilities
├── locale.hpp/cpp          # Main API and LocaleManager
├── windows.hpp/cpp         # Windows-specific implementation
├── linux.hpp/cpp           # Linux/Unix-specific implementation
├── macos.hpp/cpp           # macOS-specific implementation
├── validator.hpp/cpp       # Validation and testing utilities
├── config.hpp/cpp          # Configuration and caching
├── examples/               # Example applications
├── tests/                  # Unit tests
└── README.md              # This file
```

### Key Classes

- **LocaleManager**: Main interface for locale operations
- **LocaleFormatter**: Advanced formatting utilities
- **LocaleDetector**: Automatic locale detection
- **LocaleValidator**: Comprehensive validation system
- **LocaleCache**: High-performance caching system
- **LocaleConfigManager**: Configuration management

## Platform-Specific Features

### Windows

- Native Windows API integration
- Support for Windows locale identifiers
- UI language detection
- Registry-based configuration

### Linux/Unix

- POSIX locale support
- Environment variable detection
- System configuration file parsing
- ICU library integration (optional)

### macOS

- Core Foundation integration
- Cocoa framework support
- System preferences integration
- Native formatting APIs

## Configuration

### Environment Variables

```bash
# Basic configuration
export LOCALE_DEFAULT="en_US.UTF-8"
export LOCALE_ENABLE_VALIDATION=true
export LOCALE_ENABLE_COMPATIBILITY=true

# Cache configuration
export LOCALE_CACHE_STRATEGY=2  # Hybrid caching
export LOCALE_CACHE_TIMEOUT=300
export LOCALE_CACHE_MAX_ENTRIES=1000
export LOCALE_CACHE_COMPRESSION=true

# Performance configuration
export LOCALE_LAZY_LOADING=true
export LOCALE_PRELOADING=false
export LOCALE_THREAD_POOL_SIZE=4
export LOCALE_ASYNC_OPS=true
export LOCALE_METRICS=true

# Logging
export LOCALE_LOG_LEVEL=2  # Info level
```

### Configuration File

```ini
# locale.conf
default_locale=en_US.UTF-8
enable_validation=true
enable_compatibility_checks=true

# Cache settings
cache_strategy=2
cache_timeout=300
cache_max_memory_entries=1000
cache_enable_compression=true

# Performance settings
perf_enable_lazy_loading=true
perf_enable_preloading=false
perf_thread_pool_size=4
perf_enable_async=true
perf_enable_metrics=true
```

## Performance

### Benchmarks

The locale system is designed for high performance with the following characteristics:

- **Locale detection**: < 1ms typical
- **Validation**: < 5ms for complete validation
- **Formatting**: < 0.1ms per operation (cached)
- **Cache lookup**: < 0.01ms typical
- **Memory usage**: < 10MB for typical workloads

### Optimization Tips

1. **Enable caching**: Use memory or hybrid caching for frequently accessed locales
2. **Preload common locales**: Configure preloading for known locale requirements
3. **Use lazy loading**: Enable lazy loading to reduce startup time
4. **Monitor performance**: Enable metrics to identify bottlenecks
5. **Validate once**: Cache validation results to avoid repeated checks

## Error Handling

The locale system provides comprehensive error handling with detailed error codes and messages:

```cpp
auto result = setSystemLocale("invalid_locale");
switch (result) {
    case LocaleError::None:
        std::cout << "Success" << std::endl;
        break;
    case LocaleError::InvalidLocale:
        std::cout << "Invalid locale format" << std::endl;
        break;
    case LocaleError::SystemError:
        std::cout << "System error occurred" << std::endl;
        break;
    case LocaleError::UnsupportedPlatform:
        std::cout << "Operation not supported" << std::endl;
        break;
}
```

## Thread Safety

All public APIs are thread-safe and can be used safely from multiple threads:

- **LocaleManager**: Full thread safety with internal locking
- **LocaleFormatter**: Thread-safe for read operations
- **LocaleCache**: Thread-safe with optimized locking
- **Configuration**: Thread-safe access and modification

## Migration Guide

### From Original API

The new locale module maintains backward compatibility with the original API:

```cpp
// Old API (still works)
auto info = getSystemLanguageInfo();
printLocaleInfo(info);

// New API (recommended)
LocaleManager manager;
auto info = manager.getCurrentLocale();
// Use LocaleFormatter for advanced formatting
```

### Breaking Changes

- None - full backward compatibility maintained
- New features available through enhanced classes
- Original functions forward to new implementation

## Examples

See the `examples/` directory for complete example applications:

- `basic_usage.cpp`: Basic locale operations
- `advanced_formatting.cpp`: Advanced formatting examples
- `validation_demo.cpp`: Validation and testing examples
- `performance_test.cpp`: Performance measurement examples
- `configuration_example.cpp`: Configuration management examples

## Testing

Run the test suite to verify functionality:

```bash
cd tests/
./run_tests.sh
```

## Contributing

1. Follow the existing code style
2. Add tests for new features
3. Update documentation
4. Ensure cross-platform compatibility

## License

Copyright (C) 2023-2024 Max Qian <lightapt.com>
