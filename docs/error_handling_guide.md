# Atom Error Handling System

## Overview

The Atom Error Handling System provides a comprehensive, thread-safe, and production-ready error management framework for C++ applications. It includes error classification, context management, recovery mechanisms, formatting, and monitoring capabilities.

## Key Features

- **Comprehensive Error Classification**: Hierarchical error codes with severity levels and categories
- **Rich Error Context**: Detailed error information with correlation IDs, user data, and system information
- **Thread-Safe Operations**: Full thread safety for concurrent error handling
- **Error Recovery**: Retry policies, circuit breakers, and fallback strategies
- **Multiple Output Formats**: Plain text, JSON, HTML, colored terminal, and structured logging
- **Performance Optimized**: Asynchronous processing and efficient memory management
- **Integration Ready**: Easy integration with existing codebases

## Quick Start

### Basic Error Reporting

```cpp
#include "atom/error/error_handler.hpp"

// Simple error reporting
REPORT_ERROR(100, "File not found");

// Error with correlation ID
REPORT_ERROR_WITH_CORRELATION(200, "request-123", "Database connection failed");

// Using exceptions
try {
    // Some operation that might fail
    throw std::runtime_error("Operation failed");
} catch (const std::exception& e) {
    THROW_EXCEPTION("Wrapped error: " + std::string(e.what()));
}
```

### Error Context Creation

```cpp
#include "atom/error/error_context.hpp"

// Create error context with rich information
auto context = ErrorContext::create(100, "Detailed error message");
context->addTag("critical");
context->addTag("user-facing");
context->setUserData("user_id", std::string("12345"));
context->setUserData("session_id", 67890);
context->setSystemInfo("component", "file_processor");

// Report the context
GlobalErrorHandler::getInstance().reportError(context);
```

### Error Recovery

```cpp
#include "atom/error/error_recovery.hpp"

// Function that might fail
auto unreliableOperation = []() -> std::string {
    // Simulate random failure
    if (rand() % 3 == 0) {
        throw std::runtime_error("Random failure");
    }
    return "Success";
};

// Create recovery executor with retry policy
ErrorRecoveryExecutor<std::string> executor;
executor.withRetryPolicy(RecoveryStrategyFactory::createExponentialBackoff(3, std::chrono::milliseconds(100)))
        .withFallback(RecoveryStrategyFactory::createDefaultFallback<std::string>("fallback_value"));

// Execute with automatic recovery
try {
    std::string result = executor.execute(unreliableOperation);
    std::cout << "Result: " << result << std::endl;
} catch (const std::exception& e) {
    std::cout << "All recovery attempts failed: " << e.what() << std::endl;
}
```

### Error Formatting

```cpp
#include "atom/error/error_formatter.hpp"

auto context = ErrorContext::create(100, "Sample error");

// Format as JSON
auto jsonFormatter = ErrorFormatterFactory::createFormatter(OutputFormat::Json);
std::string jsonOutput = jsonFormatter->format(context);

// Format as colored terminal output
auto coloredFormatter = ErrorFormatterFactory::createFormatter(OutputFormat::Colored);
std::string coloredOutput = coloredFormatter->format(context);

// Using convenience macros
std::string plainOutput = FORMAT_ERROR_PLAIN(context);
std::string htmlOutput = FORMAT_ERROR_HTML(context);
```

## Architecture

### Core Components

1. **Error Codes and Metadata** (`error_code.hpp/cpp`)
   - Hierarchical error classification system
   - Severity levels: Trace, Debug, Info, Warning, Error, Critical, Fatal
   - Categories: System, Application, Network, IO, Memory, Security, etc.
   - Recovery strategies: None, Retry, Fallback, UserIntervention, Restart, Ignore

2. **Error Context** (`error_context.hpp/cpp`)
   - Rich error information container
   - Unique error IDs and correlation support
   - User data and system information
   - Parent-child error relationships
   - Thread-safe operations

3. **Error Handler** (`error_handler.hpp/cpp`)
   - Asynchronous error processing
   - Error aggregation and filtering
   - Global and thread-local error handling
   - Statistics and monitoring

4. **Error Recovery** (`error_recovery.hpp/cpp`)
   - Retry policies (fixed interval, exponential backoff, jittered)
   - Circuit breaker pattern
   - Fallback strategies
   - Bulkhead pattern for resource isolation

5. **Error Formatting** (`error_formatter.hpp/cpp`)
   - Multiple output formats
   - Customizable formatting options
   - Localization support
   - Template-based formatting

### Error Severity Levels

```cpp
enum class ErrorSeverity {
    Trace,      // Detailed tracing information
    Debug,      // Debug information
    Info,       // Informational messages
    Warning,    // Warning conditions
    Error,      // Error conditions
    Critical,   // Critical conditions
    Fatal       // Fatal conditions
};
```

### Error Categories

```cpp
enum class ErrorCategory {
    Unknown,        // Unknown category
    System,         // System-level errors
    Application,    // Application logic errors
    Network,        // Network-related errors
    IO,            // Input/output errors
    Memory,        // Memory-related errors
    Security,      // Security-related errors
    Configuration, // Configuration errors
    Validation,    // Data validation errors
    Business,      // Business logic errors
    External       // External service errors
};
```

## Advanced Usage

### Custom Error Handlers

```cpp
// Create custom error handler
GlobalErrorHandler::getInstance().setGlobalHandler(
    [](std::shared_ptr<ErrorContext> context) {
        // Custom error processing logic
        if (context->getSeverity() >= ErrorSeverity::Critical) {
            // Send alert to monitoring system
            sendAlert(context);
        }
        
        // Log to file
        logToFile(context);
    }
);
```

### Error Aggregation

```cpp
ErrorAggregator aggregator;

// Add aggregation strategies
aggregator.addStrategy(AggregationStrategy::BySeverity);
aggregator.addStrategy(AggregationStrategy::ByCategory);
aggregator.addStrategy(AggregationStrategy::ByTimeWindow);

// Process errors
auto context1 = ErrorContext::create(100, "Error 1");
auto context2 = ErrorContext::create(200, "Error 2");

aggregator.addError(context1);
aggregator.addError(context2);

// Get aggregated results
auto results = aggregator.getAggregatedErrors();
for (const auto& [key, errors] : results) {
    std::cout << "Group " << key << ": " << errors.size() << " errors" << std::endl;
}
```

### Circuit Breaker Pattern

```cpp
// Create circuit breaker
auto circuitBreaker = std::make_shared<CircuitBreaker>(
    3,                              // failure threshold
    std::chrono::seconds(30),       // timeout
    2                               // success threshold for half-open
);

// Use with recovery executor
ErrorRecoveryExecutor<int> executor;
executor.withCircuitBreaker(circuitBreaker);

// Monitor circuit breaker state
auto stats = circuitBreaker->getStatistics();
std::cout << "Circuit breaker state: " << static_cast<int>(circuitBreaker->getState()) << std::endl;
std::cout << "Total failures: " << stats["total_failures"] << std::endl;
std::cout << "Total successes: " << stats["total_successes"] << std::endl;
```

### Custom Formatters

```cpp
// Register custom formatter
ErrorFormatterFactory::registerFormatter("custom", []() -> std::unique_ptr<ErrorFormatter> {
    return std::make_unique<MyCustomFormatter>();
});

// Use custom formatter
auto customFormatter = ErrorFormatterFactory::createCustomFormatter("custom");
std::string output = customFormatter->format(context);
```

### Template-Based Formatting

```cpp
// Create template formatter
std::string templateStr = "[{timestamp}] {severity}: {message} (Code: {error_code})";
TemplateFormatter formatter(templateStr);

std::string formatted = formatter.format(context);
// Output: [1640995200] ERROR: File not found (Code: 100)
```

## Best Practices

### 1. Error Code Organization

- Use consistent error code ranges for different modules
- Document error codes and their meanings
- Use appropriate severity levels
- Choose correct recovery strategies

### 2. Context Information

- Include relevant context information
- Use correlation IDs for related operations
- Add meaningful tags for categorization
- Include user and system information when helpful

### 3. Performance Considerations

- Use asynchronous error reporting for high-throughput scenarios
- Configure appropriate queue sizes for error processing
- Use filtering to reduce noise in error logs
- Monitor error processing performance

### 4. Thread Safety

- The system is fully thread-safe
- Use thread-local error handlers when appropriate
- Be careful with shared error contexts
- Use proper synchronization for custom handlers

### 5. Error Recovery

- Choose appropriate retry policies
- Use circuit breakers for external dependencies
- Implement meaningful fallback strategies
- Monitor recovery success rates

## Configuration

### Global Error Handler Configuration

```cpp
// Initialize global error handler
GlobalErrorHandler::getInstance().initialize();

// Configure processing options
auto& reporter = GlobalErrorHandler::getInstance().getReporter();
reporter.setQueueSize(10000);           // Set queue size
reporter.setMaxProcessingTime(std::chrono::seconds(5));  // Set processing timeout

// Add custom filters
reporter.addFilter("severity_filter", [](std::shared_ptr<ErrorContext> context) {
    return context->getSeverity() >= ErrorSeverity::Warning;
});

// Add custom handlers
reporter.addHandler("file_logger", [](std::shared_ptr<ErrorContext> context) {
    // Log to file
});

reporter.addHandler("metrics_collector", [](std::shared_ptr<ErrorContext> context) {
    // Collect metrics
});
```

### Formatter Configuration

```cpp
// Configure JSON formatter
auto jsonFormatter = ErrorFormatterFactory::createFormatter(OutputFormat::Json);
jsonFormatter->setOption("pretty_print", "true");
jsonFormatter->setOption("indent_size", "4");

// Configure HTML formatter
auto htmlFormatter = ErrorFormatterFactory::createFormatter(OutputFormat::Html);
htmlFormatter->setOption("include_css", "true");

// Configure colored formatter
auto coloredFormatter = ErrorFormatterFactory::createFormatter(OutputFormat::Colored);
coloredFormatter->setOption("enable_colors", "true");
```

## Integration Examples

### Integration with Logging Systems

```cpp
#include <spdlog/spdlog.h>

// Integrate with spdlog
GlobalErrorHandler::getInstance().setGlobalHandler(
    [](std::shared_ptr<ErrorContext> context) {
        auto formatter = ErrorFormatterFactory::createFormatter(OutputFormat::Structured);
        std::string formatted = formatter->format(context);
        
        switch (context->getSeverity()) {
            case ErrorSeverity::Error:
            case ErrorSeverity::Critical:
            case ErrorSeverity::Fatal:
                spdlog::error(formatted);
                break;
            case ErrorSeverity::Warning:
                spdlog::warn(formatted);
                break;
            default:
                spdlog::info(formatted);
                break;
        }
    }
);
```

### Integration with Monitoring Systems

```cpp
// Integrate with Prometheus metrics
#include <prometheus/counter.h>
#include <prometheus/histogram.h>

class MetricsErrorHandler {
private:
    prometheus::Counter& errorCounter_;
    prometheus::Histogram& errorDuration_;
    
public:
    void handleError(std::shared_ptr<ErrorContext> context) {
        // Increment error counter by severity and category
        errorCounter_.Increment({
            {"severity", std::string(severityToString(context->getSeverity()))},
            {"category", std::string(categoryToString(context->getCategory()))}
        });
        
        // Record error processing time if available
        if (context->hasUserData("processing_time")) {
            auto duration = std::any_cast<double>(context->getUserData("processing_time"));
            errorDuration_.Observe(duration);
        }
    }
};
```

## Troubleshooting

### Common Issues

1. **Memory Leaks**: Ensure proper cleanup of error contexts and handlers
2. **Performance Issues**: Check queue sizes and processing times
3. **Thread Safety**: Verify proper synchronization in custom handlers
4. **Missing Error Information**: Ensure all relevant context is captured

### Debugging

```cpp
// Enable debug logging
GlobalErrorHandler::getInstance().setDebugMode(true);

// Get error processing statistics
auto& reporter = GlobalErrorHandler::getInstance().getReporter();
auto stats = reporter.getStatistics();

for (const auto& [key, value] : stats) {
    std::cout << key << ": " << value << std::endl;
}

// Get context manager statistics
auto& manager = ErrorContextManager::getInstance();
auto contextStats = manager.getStatistics();

for (const auto& [key, value] : contextStats) {
    std::cout << key << ": " << value << std::endl;
}
```

## API Reference

For detailed API documentation, see the header files:

- `atom/error/error_code.hpp` - Error classification and metadata
- `atom/error/error_context.hpp` - Error context and management
- `atom/error/error_handler.hpp` - Error reporting and handling
- `atom/error/error_recovery.hpp` - Error recovery mechanisms
- `atom/error/error_formatter.hpp` - Error formatting and display

## License

This error handling system is part of the Atom project and is licensed under the same terms as the main project.
