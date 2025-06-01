# Modern C++17+ Crontab Migration Guide

## Overview

This guide helps migrate from the legacy `crontab.hpp` implementation to the modern C++17+ design with enhanced type safety, error handling, and cross-platform support.

## Key Improvements

### 1. Type Safety
- **Before**: Raw strings for job IDs, expressions, and commands
- **After**: Strong types (`JobId`, `CronExpression`, `Command`) prevent mixing of unrelated values

### 2. Error Handling
- **Before**: Boolean return values with limited error information
- **After**: `std::expected<T, std::error_code>` with comprehensive error hierarchy

### 3. Thread Safety
- **Before**: No thread safety guarantees
- **After**: Full thread safety with `std::shared_mutex` and atomic operations

### 4. Resource Management
- **Before**: Manual resource cleanup
- **After**: RAII with automatic cleanup and scope guards

### 5. Platform Support
- **Before**: Unix/Linux only
- **After**: Cross-platform support (Windows, macOS, Linux)

## Migration Steps

### Step 1: Include New Headers

**Before:**
```cpp
#include "crontab.hpp"
```

**After:**
```cpp
#include "crontab_modern.hpp"
#include "crontab_errors.hpp"  // If handling errors explicitly
```

### Step 2: Replace Basic Job Management

**Before:**
```cpp
atom::system::Crontab crontab;

// Add job
bool success = crontab.addJob("job1", "0 9 * * *", "echo hello");
if (!success) {
    // Limited error information
    std::cerr << "Failed to add job" << std::endl;
}

// Remove job
crontab.removeJob("job1");

// List jobs
auto jobs = crontab.listJobs();
```

**After:**
```cpp
atom::system::CronManager manager;

// Add job with type safety and error handling
auto job = atom::system::JobBuilder{}
    .withId(atom::system::JobId{"job1"})
    .withExpression(atom::system::CronExpression::parse("0 9 * * *").value())
    .withCommand(atom::system::Command{"echo hello"})
    .build();

if (job) {
    auto result = manager.addJob(std::move(*job));
    if (!result) {
        std::cerr << "Failed to add job: " << result.error().message() << std::endl;
    }
} else {
    std::cerr << "Invalid job configuration" << std::endl;
}

// Remove job with error handling
auto removeResult = manager.removeJob(atom::system::JobId{"job1"});
if (!removeResult) {
    std::cerr << "Failed to remove job: " << removeResult.error().message() << std::endl;
}

// List jobs with type safety
auto jobs = manager.listJobs();
for (const auto& jobRef : jobs) {
    const auto& job = jobRef.get();
    std::cout << "Job ID: " << job.getId().value()
              << ", Expression: " << job.getExpression().value() << std::endl;
}
```

### Step 3: Enhanced Error Handling

**Before:**
```cpp
if (!crontab.addJob("job1", "invalid", "command")) {
    // No way to know what went wrong
}
```

**After:**
```cpp
auto expr = atom::system::CronExpression::parse("invalid");
if (!expr) {
    switch (expr.error().value()) {
        case static_cast<int>(atom::system::CrontabError::INVALID_EXPRESSION):
            std::cerr << "Invalid cron expression format" << std::endl;
            break;
        case static_cast<int>(atom::system::CrontabError::PLATFORM_ERROR):
            std::cerr << "Platform-specific error" << std::endl;
            break;
        default:
            std::cerr << "Unknown error: " << expr.error().message() << std::endl;
    }
}
```

### Step 4: Resource Management with RAII

**Before:**
```cpp
// Manual cleanup required
Crontab* crontab = new Crontab();
// ... use crontab
delete crontab;  // Easy to forget
```

**After:**
```cpp
// Automatic cleanup with RAII
{
    atom::system::CronManager manager;
    
    auto guard = atom::system::make_scope_guard([&]() {
        std::cout << "Cleaning up..." << std::endl;
    });
    
    // ... use manager
}  // Automatic cleanup when scope ends
```

### Step 5: Thread-Safe Operations

**Before:**
```cpp
// Not thread-safe
std::thread t1([&crontab]() { crontab.addJob("job1", "0 * * * *", "cmd1"); });
std::thread t2([&crontab]() { crontab.addJob("job2", "0 * * * *", "cmd2"); });
// Race condition!
```

**After:**
```cpp
// Thread-safe by design
atom::system::CronManager manager;

std::thread t1([&manager]() {
    auto job = atom::system::JobBuilder{}
        .withId(atom::system::JobId{"job1"})
        .withExpression(atom::system::CronExpression::parse("0 * * * *").value())
        .withCommand(atom::system::Command{"cmd1"})
        .build().value();
    manager.addJob(std::move(job));
});

std::thread t2([&manager]() {
    auto job = atom::system::JobBuilder{}
        .withId(atom::system::JobId{"job2"})
        .withExpression(atom::system::CronExpression::parse("0 * * * *").value())
        .withCommand(atom::system::Command{"cmd2"})
        .build().value();
    manager.addJob(std::move(job));
});

t1.join();
t2.join();
```

## Advanced Features

### 1. Job Statistics and Monitoring

```cpp
atom::system::CronManager manager;

// Add a job
auto job = atom::system::JobBuilder{}
    .withId(atom::system::JobId{"monitored-job"})
    .withExpression(atom::system::CronExpression::parse("0 * * * *").value())
    .withCommand(atom::system::Command{"echo monitoring"})
    .build().value();

manager.addJob(std::move(job));

// Get job statistics
auto jobResult = manager.getJob(atom::system::JobId{"monitored-job"});
if (jobResult) {
    const auto& job = jobResult.value().get();
    const auto& stats = job.getStatistics();
    
    std::cout << "Success count: " << stats.getSuccessCount() << std::endl;
    std::cout << "Failure count: " << stats.getFailureCount() << std::endl;
    std::cout << "Success rate: " << stats.getSuccessRate() << std::endl;
    std::cout << "Total runs: " << stats.getTotalRuns() << std::endl;
}
```

### 2. Job Persistence and Configuration

```cpp
// Manager with custom config file
atom::system::CronManager manager("my_cron_jobs.json");

// Jobs are automatically saved to/loaded from the config file
auto job = atom::system::JobBuilder{}
    .withId(atom::system::JobId{"persistent-job"})
    .withExpression(atom::system::CronExpression::parse("0 0 * * *").value())
    .withCommand(atom::system::Command{"backup script"})
    .build().value();

manager.addJob(std::move(job));
// Job is automatically persisted to my_cron_jobs.json
```

### 3. Zero-Copy Data Operations

```cpp
// Efficient data handling without copies
std::vector<char> large_data(1024 * 1024);  // 1MB
atom::system::DataView<char> view(large_data);

// View provides access without copying the data
process_data(view);  // No copy overhead

// Convert to span for standard library compatibility
std::span<const char> span = view.span();
```

### 4. Scheduler Management

```cpp
atom::system::CronManager manager;

// Start the scheduler
manager.start();

// Add jobs while running
auto job = atom::system::JobBuilder{}
    .withId(atom::system::JobId{"runtime-job"})
    .withExpression(atom::system::CronExpression::parse("*/5 * * * *").value())
    .withCommand(atom::system::Command{"echo every 5 minutes"})
    .build().value();

manager.addJob(std::move(job));

// Check if running
if (manager.isRunning()) {
    std::cout << "Scheduler is active" << std::endl;
}

// Graceful shutdown
manager.shutdown();
```

## Error Handling Best Practices

### 1. Using std::expected

```cpp
auto handleJobOperation() -> std::expected<void, std::error_code> {
    atom::system::CronManager manager;
    
    auto expr = atom::system::CronExpression::parse("0 9 * * *");
    if (!expr) {
        return std::unexpected(expr.error());
    }
    
    auto job = atom::system::JobBuilder{}
        .withId(atom::system::JobId{"example"})
        .withExpression(std::move(*expr))
        .withCommand(atom::system::Command{"echo example"})
        .build();
    
    if (!job) {
        return std::unexpected(job.error());
    }
    
    return manager.addJob(std::move(*job));
}

// Usage
auto result = handleJobOperation();
if (!result) {
    std::cerr << "Operation failed: " << result.error().message() << std::endl;
}
```

### 2. Exception Handling

```cpp
try {
    atom::system::CronManager manager;
    
    // This might throw if platform initialization fails
    auto job = atom::system::JobBuilder{}
        .withId(atom::system::JobId{"risky-job"})
        .withExpression(atom::system::CronExpression::parse("0 0 * * *").value())
        .withCommand(atom::system::Command{"risky command"})
        .build().value();
    
    manager.addJob(std::move(job));
    
} catch (const atom::system::CrontabException& e) {
    std::cerr << "Crontab error: " << e.what() << std::endl;
} catch (const std::exception& e) {
    std::cerr << "General error: " << e.what() << std::endl;
}
```

## Performance Considerations

### 1. Move Semantics
Always use move semantics when adding jobs to avoid unnecessary copies:

```cpp
auto job = /* create job */;
manager.addJob(std::move(job));  // ✓ Efficient
// manager.addJob(job);          // ✗ Inefficient copy
```

### 2. Batch Operations
For multiple operations, consider the performance implications:

```cpp
// Efficient: Single manager instance
atom::system::CronManager manager;
for (int i = 0; i < 1000; ++i) {
    auto job = /* create job */;
    manager.addJob(std::move(job));
}

// Less efficient: Multiple manager instances
for (int i = 0; i < 1000; ++i) {
    atom::system::CronManager manager;  // Overhead
    auto job = /* create job */;
    manager.addJob(std::move(job));
}
```

### 3. Read Operations
Use const references for read-only access:

```cpp
const auto& jobs = manager.listJobs();
for (const auto& jobRef : jobs) {
    const auto& job = jobRef.get();  // No copy
    // Read job properties
}
```

## Compatibility Notes

### 1. Compiler Requirements
- C++17 or later
- Supported compilers: GCC 7+, Clang 5+, MSVC 2017+

### 2. Dependencies
- nlohmann/json for serialization
- System libraries for platform integration

### 3. Platform-Specific Features
- **Windows**: Uses Task Scheduler
- **macOS**: Uses launchd
- **Linux**: Uses system crontab

## Testing the Migration

Create a simple test to verify the migration:

```cpp
#include "crontab_modern.hpp"
#include <iostream>

int main() {
    try {
        atom::system::CronManager manager;
        
        auto job = atom::system::JobBuilder{}
            .withId(atom::system::JobId{"test-migration"})
            .withExpression(atom::system::CronExpression::parse("0 12 * * *").value())
            .withCommand(atom::system::Command{"echo Migration successful!"})
            .build();
        
        if (job) {
            auto result = manager.addJob(std::move(*job));
            if (result) {
                std::cout << "Migration successful!" << std::endl;
                std::cout << "Job count: " << manager.getJobCount() << std::endl;
            } else {
                std::cerr << "Failed to add job: " << result.error().message() << std::endl;
            }
        } else {
            std::cerr << "Failed to build job" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

## Troubleshooting

### Common Issues

1. **Compilation Errors**
   - Ensure C++17 support is enabled
   - Check all header includes

2. **Runtime Errors**
   - Verify platform permissions for cron operations
   - Check file system permissions for config files

3. **Performance Issues**
   - Use move semantics consistently
   - Avoid unnecessary copies of jobs and data

### Getting Help

For additional support:
1. Check the unit tests for usage examples
2. Review the header documentation
3. Enable debug logging for detailed error information
