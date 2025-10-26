# Atom Async Examples

This directory contains comprehensive examples demonstrating the capabilities of the `atom::async` library. The examples are organized by complexity and functionality to provide a clear learning progression.

## 📚 Learning Progression

### 🟢 **Level 1: Fundamentals (Beginner)**

Start here if you're new to async programming or the atom::async library.

| Example | Description | Key Concepts |
|---------|-------------|--------------|
| [`async_worker_usage.cpp`](async_worker_usage.cpp) | Basic AsyncWorker usage patterns | Task creation, state management, result retrieval |
| [`promise.cpp`](promise.cpp) | Promise creation and value setting | Promise/Future pattern, callbacks, cancellation |
| [`future.cpp`](future.cpp) | EnhancedFuture operations | Future chaining, timeouts, error handling |

### 🟡 **Level 2: Core Components (Intermediate)**

Build upon fundamental concepts with more advanced usage patterns.

| Example | Description | Key Concepts |
|---------|-------------|--------------|
| [`async_worker_features.cpp`](async_worker_features.cpp) | Advanced AsyncWorker patterns | WorkerContainer, error recovery, performance optimization |
| [`async_executor.cpp`](async_executor.cpp) | AsyncExecutor configuration and usage | Thread management, priority execution, resource optimization |
| [`pool.cpp`](pool.cpp) | Thread pool implementations | Pool configurations, load balancing, performance tuning |
| [`parallel.cpp`](parallel.cpp) | Parallel execution patterns | Parallel algorithms, execution policies, performance optimization |

### 🟠 **Level 3: Messaging & Communication (Intermediate-Advanced)**

Learn about inter-component communication and messaging patterns.

| Example | Description | Key Concepts |
|---------|-------------|--------------|
| [`message_bus.cpp`](message_bus.cpp) | Publish-subscribe messaging | Message filtering, namespaces, global broadcasting |
| [`message_queue.cpp`](message_queue.cpp) | Priority-based message queuing | Priority handling, filtering, subscriber management |
| [`eventstack.cpp`](eventstack.cpp) | Event handling systems | Event processing, stack operations, thread safety |
| [`queue.cpp`](queue.cpp) | Various queue implementations | Thread-safe queues, lock-free patterns, specialized queues |

### 🔴 **Level 4: Synchronization (Advanced)**

Master synchronization primitives and thread coordination.

| Example | Description | Key Concepts |
|---------|-------------|--------------|
| [`trigger.cpp`](trigger.cpp) | Event-driven programming | Callback management, scheduling, event coordination |
| [`slot.cpp`](slot.cpp) | Signal-slot patterns | Connection management, execution policies, decoupling |
| [`safetype.cpp`](safetype.cpp) | Thread-safe data structures | Atomic operations, concurrent access, data protection |
| [`limiter.cpp`](limiter.cpp) | Rate limiting patterns | Function-specific limits, throttling, resource protection |
| [`lock.cpp`](lock.cpp) | Lock implementations | Different lock types, performance characteristics, usage patterns |

### 🟣 **Level 5: Threading Utilities (Advanced)**

Advanced thread management and coordination techniques.

| Example | Description | Key Concepts |
|---------|-------------|--------------|
| [`thread_wrapper.cpp`](thread_wrapper.cpp) | Advanced thread management | Stop tokens, timeouts, thread coordination |
| [`threadlocal.cpp`](threadlocal.cpp) | Thread-local storage patterns | TLS initialization, cleanup management, isolation |

### ⚫ **Level 6: Utilities & Tools (Specialized)**

Specialized utilities and helper components.

| Example | Description | Key Concepts |
|---------|-------------|--------------|
| [`timer.cpp`](timer.cpp) | Timer and scheduling | Interval management, task prioritization, timer coordination |
| [`daemon.cpp`](daemon.cpp) | Daemon process management | Process lifecycle, signal handling, monitoring |
| [`generator.cpp`](generator.cpp) | Generator and coroutine patterns | Coroutine usage, two-way communication, concurrent generation |
| [`packaged_task.cpp`](packaged_task.cpp) | Enhanced packaged tasks | Task management, error handling, component integration |

### 🌟 **Level 7: Integration & Real-World (Expert)**

Complex integration patterns and production-ready examples.

| Example | Description | Key Concepts |
|---------|-------------|--------------|
| [`component_integration.cpp`](component_integration.cpp) | Component integration patterns | Multi-component workflows, resource sharing, coordination |
| [`web_server_example.cpp`](web_server_example.cpp) | Async web server implementation | HTTP handling, database integration, session management |

## 🛠️ Building and Running Examples

### Prerequisites

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.16 or higher
- Atom library dependencies

### Build Instructions

1. **Navigate to the project root:**

   ```bash
   cd /path/to/atom/project
   ```

2. **Create build directory:**

   ```bash
   mkdir build && cd build
   ```

3. **Configure with CMake:**

   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Release
   ```

4. **Build all examples:**

   ```bash
   cmake --build . --target async_examples
   ```

5. **Build specific example:**

   ```bash
   cmake --build . --target async_promise_example
   ```

### Running Examples

Examples are built as individual executables in the `build/example/async/` directory:

```bash
# Run a specific example
./build/example/async/async_promise_example

# Run with verbose output (if supported)
./build/example/async/async_promise_example --verbose
```

### Example Naming Convention

- Executable names follow the pattern: `async_<example_name>_example`
- Source files use the pattern: `<component_name>.cpp`

## 📖 Example Structure

Each example follows a consistent structure:

### File Header

```cpp
/**
 * @file example_name.cpp
 * @brief Brief description of what this example demonstrates
 *
 * @details Detailed explanation of concepts covered
 * @level Beginner|Intermediate|Advanced|Expert
 * @prerequisites List of required knowledge
 * @related_examples List of related example files
 */
```

### Code Organization

1. **Utility Functions** - Helper functions and common utilities
2. **Data Structures** - Example-specific data types
3. **Demonstration Sections** - Organized by functionality
4. **Main Function** - Orchestrates all demonstrations

### Documentation Standards

- Comprehensive comments explaining concepts
- Performance timing and measurement
- Error handling demonstrations
- Thread safety considerations
- Best practices and common pitfalls

## 🎯 Key Learning Objectives

### After completing these examples, you should understand

**Core Concepts:**

- Asynchronous programming patterns
- Promise/Future paradigm
- Thread pool management
- Message-based communication

**Advanced Patterns:**

- Component integration strategies
- Error handling and recovery
- Performance optimization techniques
- Resource management and cleanup

**Production Considerations:**

- Thread safety and synchronization
- Rate limiting and throttling
- Monitoring and diagnostics
- Scalability patterns

## 🔧 Troubleshooting

### Common Build Issues

**Missing Dependencies:**

```bash
# Install required packages (Ubuntu/Debian)
sudo apt-get install build-essential cmake libboost-all-dev

# Install required packages (macOS)
brew install cmake boost
```

**Compiler Compatibility:**

- Ensure C++20 support is enabled
- Check compiler version requirements
- Verify CMake configuration

### Runtime Issues

**Thread-related Errors:**

- Check system thread limits
- Verify proper resource cleanup
- Monitor memory usage

**Performance Issues:**

- Adjust thread pool sizes
- Review synchronization overhead
- Profile critical sections

## 📚 Additional Resources

- [Atom Async Documentation](../../docs/async/README.md)
- [C++20 Coroutines Guide](https://en.cppreference.com/w/cpp/language/coroutines)
- [Concurrency Patterns](https://en.wikipedia.org/wiki/Concurrency_pattern)
- [Thread Safety Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-concurrency)

## 🤝 Contributing

When adding new examples:

1. Follow the established naming conventions
2. Include comprehensive documentation
3. Add appropriate error handling
4. Provide performance measurements
5. Update this README with the new example
6. Ensure cross-platform compatibility

## 🚀 Quick Start Guide

### For Complete Beginners

1. **Start with basic concepts:**

   ```bash
   ./async_worker_usage_example
   ./async_promise_example
   ./async_future_example
   ```

2. **Progress to messaging:**

   ```bash
   ./async_message_bus_example
   ./async_message_queue_example
   ```

3. **Explore integration:**

   ```bash
   ./async_component_integration_example
   ```

### For Experienced Developers

Jump directly to advanced examples:

```bash
./async_web_server_example
./async_parallel_example
./async_pool_example
```

## 📊 Performance Benchmarks

Many examples include performance measurements. Look for output like:

```text
⏱️  Starting: Basic AsyncWorker
⏱️  Completed: Basic AsyncWorker in 1250 μs
```

### Optimization Tips

- Thread pool size should match CPU cores for CPU-bound tasks
- Use message queues for decoupling components
- Implement proper rate limiting for production systems
- Monitor memory usage in long-running applications

## 🐛 Debugging Async Code

### Common Debugging Techniques Demonstrated

**Logging and Tracing:**

- Thread-safe logging with timestamps
- Request/response correlation IDs
- Performance measurement points

**Error Handling:**

- Exception propagation through futures
- Graceful degradation strategies
- Resource cleanup on failures

**Testing Strategies:**

- Deterministic testing with controlled timing
- Mock objects for external dependencies
- Load testing with concurrent requests

## 🔍 Code Analysis

### Metrics Tracked in Examples

- **Throughput:** Messages/requests processed per second
- **Latency:** Time from request to response
- **Resource Usage:** Thread count, memory allocation
- **Error Rates:** Failed operations percentage

### Example Output Analysis

```text
📊 Total scheduled notifications: 4
📊 Processed requests: 4
📊 Collected results: 4
⏱️  Completed: ThreadPool-MessageQueue integration in 523847 μs
```

## 🌐 Platform Considerations

### Windows-Specific Notes

- Uses Windows thread APIs where applicable
- ASIO integration for I/O operations
- Visual Studio project files available

### Linux/Unix Notes

- POSIX thread support
- Signal handling for daemon processes
- Performance optimizations for Linux

### macOS Notes

- Clang compiler optimizations
- Grand Central Dispatch integration where beneficial
- Metal performance shaders compatibility

## 📈 Scaling Patterns

### Horizontal Scaling

- Message bus for distributed communication
- Load balancing across thread pools
- Stateless component design

### Vertical Scaling

- Thread pool optimization
- Memory pool management
- CPU affinity configuration

## 🔐 Security Considerations

Examples demonstrate:

- Input validation patterns
- Rate limiting for DoS protection
- Session management best practices
- Secure error handling (no information leakage)

## 📄 License

These examples are part of the Atom project and follow the same licensing terms.
