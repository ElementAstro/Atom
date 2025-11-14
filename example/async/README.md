# Atom Async Examples

This directory contains comprehensive examples demonstrating the capabilities of the `atom::async` library. The examples are organized to mirror the structure of `atom/async`, with subdirectories for each component category, providing complete coverage of all async functionality.

## 📁 Directory Structure

The example directory structure exactly mirrors `atom/async`:

```
example/async/
├── core/                    # Core async primitives examples
│   ├── async_worker_features_example.cpp
│   ├── async_worker_usage_example.cpp
│   ├── future_example.cpp
│   └── promise_example.cpp
├── execution/               # Task execution system examples
│   ├── async_executor_example.cpp
│   ├── packaged_task_example.cpp
│   ├── parallel_example.cpp
│   └── pool_example.cpp
├── messaging/               # Message passing and queue examples
│   ├── eventstack_example.cpp
│   ├── message_bus_example.cpp
│   ├── message_queue_example.cpp
│   └── queue_example.cpp
├── sync/                    # Synchronization primitive examples
│   ├── limiter_example.cpp
│   ├── safetype_example.cpp
│   ├── slot_example.cpp
│   └── trigger_example.cpp
├── threading/               # Threading utility examples
│   ├── lock_example.cpp
│   ├── thread_wrapper_example.cpp
│   └── threadlocal_example.cpp
├── utils/                   # Utility component examples
│   ├── daemon_example.cpp
│   ├── generator_example.cpp
│   ├── lodash_example.cpp
│   └── timer_example.cpp
├── component_integration.cpp    # Integration examples
└── web_server_example.cpp       # Real-world application example
```

This structure provides:
- **Complete Coverage**: Every component in `atom/async` has a corresponding example
- **Easy Navigation**: Find examples by matching the component's location in `atom/async`
- **Logical Organization**: Related examples are grouped together by functionality

## 📚 Learning Progression

### 🟢 **Level 1: Core Fundamentals (Beginner)**

Start here if you're new to async programming or the atom::async library.

#### Core Async Primitives (`core/`)

| Example | Description | Key Concepts |
|---------|-------------|--------------|
| [`core/async_worker_usage_example.cpp`](core/async_worker_usage_example.cpp) | Basic AsyncWorker usage patterns | Task creation, state management, result retrieval |
| [`core/promise_example.cpp`](core/promise_example.cpp) | Promise creation and value setting | Promise/Future pattern, callbacks, cancellation |
| [`core/future_example.cpp`](core/future_example.cpp) | EnhancedFuture operations | Future chaining, timeouts, error handling |
| [`core/async_worker_features_example.cpp`](core/async_worker_features_example.cpp) | AsyncWorkerManager usage | Manager orchestration, batch coordination, error recovery |

### 🟡 **Level 2: Execution Systems (Intermediate)**

Build upon fundamental concepts with task execution and parallelism.

#### Task Execution (`execution/`)

| Example | Description | Key Concepts |
|---------|-------------|--------------|
| [`execution/async_executor_example.cpp`](execution/async_executor_example.cpp) | AsyncExecutor configuration and usage | Thread management, priority execution, resource optimization |
| [`execution/pool_example.cpp`](execution/pool_example.cpp) | Thread pool implementations | Pool configurations, load balancing, performance tuning |
| [`execution/parallel_example.cpp`](execution/parallel_example.cpp) | Parallel execution patterns | Parallel algorithms, execution policies, performance optimization |
| [`execution/packaged_task_example.cpp`](execution/packaged_task_example.cpp) | Enhanced packaged tasks | Task management, error handling, component integration |

### 🟠 **Level 3: Messaging & Communication (Intermediate-Advanced)**

Learn about inter-component communication and messaging patterns.

#### Message Passing (`messaging/`)

| Example | Description | Key Concepts |
|---------|-------------|--------------|
| [`messaging/message_bus_example.cpp`](messaging/message_bus_example.cpp) | Publish-subscribe messaging | Message filtering, namespaces, global broadcasting |
| [`messaging/message_queue_example.cpp`](messaging/message_queue_example.cpp) | Priority-based message queuing | Priority handling, filtering, subscriber management |
| [`messaging/eventstack_example.cpp`](messaging/eventstack_example.cpp) | Event handling systems | Event processing, stack operations, thread safety |
| [`messaging/queue_example.cpp`](messaging/queue_example.cpp) | Various queue implementations | Thread-safe queues, lock-free patterns, specialized queues |

### 🔴 **Level 4: Synchronization (Advanced)**

Master synchronization primitives and thread coordination.

#### Synchronization Primitives (`sync/`)

| Example | Description | Key Concepts |
|---------|-------------|--------------|
| [`sync/trigger_example.cpp`](sync/trigger_example.cpp) | Event-driven programming | Callback management, scheduling, event coordination |
| [`sync/slot_example.cpp`](sync/slot_example.cpp) | Signal-slot patterns | Connection management, execution policies, decoupling |
| [`sync/safetype_example.cpp`](sync/safetype_example.cpp) | Thread-safe data structures | Atomic operations, concurrent access, data protection |
| [`sync/limiter_example.cpp`](sync/limiter_example.cpp) | Rate limiting patterns | Function-specific limits, throttling, resource protection |

### 🟣 **Level 5: Threading Utilities (Advanced)**

Advanced thread management and coordination techniques.

#### Threading Components (`threading/`)

| Example | Description | Key Concepts |
|---------|-------------|--------------|
| [`threading/thread_wrapper_example.cpp`](threading/thread_wrapper_example.cpp) | Advanced thread management | Stop tokens, timeouts, thread coordination |
| [`threading/threadlocal_example.cpp`](threading/threadlocal_example.cpp) | Thread-local storage patterns | TLS initialization, cleanup management, isolation |
| [`threading/lock_example.cpp`](threading/lock_example.cpp) | Lock implementations | Different lock types, performance characteristics, usage patterns |

### ⚫ **Level 6: Utilities & Tools (Specialized)**

Specialized utilities and helper components.

#### Utility Components (`utils/`)

| Example | Description | Key Concepts |
|---------|-------------|--------------|
| [`utils/timer_example.cpp`](utils/timer_example.cpp) | Timer and scheduling | Interval management, task prioritization, timer coordination |
| [`utils/lodash_example.cpp`](utils/lodash_example.cpp) | Debounce/Throttle utilities | Debounce (leading/trailing/maxWait), Throttle (leading/trailing/both) |
| [`utils/daemon_example.cpp`](utils/daemon_example.cpp) | Daemon process management | Process lifecycle, signal handling, monitoring |
| [`utils/generator_example.cpp`](utils/generator_example.cpp) | Generator and coroutine patterns | Coroutine usage, two-way communication, concurrent generation |

### 🌟 **Level 7: Integration & Real-World (Expert)**

Complex integration patterns and production-ready examples.

#### Integration Examples (Root)

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
   # Build a core example
   cmake --build . --target async_core_promise_example

   # Build an execution example
   cmake --build . --target async_execution_pool_example

   # Build a messaging example
   cmake --build . --target async_messaging_message_bus_example
   ```

### Running Examples

Examples are built as individual executables in the `build/example/async/` directory:

```bash
# Run core examples
./build/example/async/async_core_promise_example
./build/example/async/async_core_future_example

# Run execution examples
./build/example/async/async_execution_async_executor_example
./build/example/async/async_execution_pool_example

# Run messaging examples
./build/example/async/async_messaging_message_bus_example

# Run with verbose output (if supported)
./build/example/async/async_core_promise_example --verbose
```

### Example Naming Convention

The new structure uses a hierarchical naming pattern:

- **Executable names**: `async_<category>_<example_name>`
  - Examples: `async_core_promise_example`, `async_execution_pool_example`
- **Source files**: `<category>/<component_name>_example.cpp`
  - Examples: `core/promise_example.cpp`, `execution/pool_example.cpp`
- **Root examples**: `async_<example_name>` (for integration examples)
  - Examples: `async_component_integration`, `async_web_server_example`

This naming convention makes it easy to:
- Identify which category an example belongs to
- Find the source file for a given executable
- Organize examples in IDE project views

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

1. **Follow the directory structure**: Place examples in the appropriate subdirectory matching `atom/async` structure
   - Core primitives → `core/`
   - Execution systems → `execution/`
   - Messaging → `messaging/`
   - Synchronization → `sync/`
   - Threading → `threading/`
   - Utilities → `utils/`

2. **Follow naming conventions**:
   - Source files: `<component_name>_example.cpp`
   - Place in subdirectory matching the component's location in `atom/async`

3. **Include comprehensive documentation**:
   - File header with description, level, prerequisites, related examples
   - Inline comments explaining concepts
   - Performance measurements where applicable

4. **Add appropriate error handling**:
   - Demonstrate proper exception handling
   - Show graceful degradation strategies
   - Include resource cleanup examples

5. **Update this README**:
   - Add the example to the appropriate learning level section
   - Update the directory structure diagram if needed
   - Add any special build or runtime requirements

6. **Ensure cross-platform compatibility**:
   - Test on Windows, Linux, and macOS if possible
   - Use platform-agnostic code where possible
   - Document any platform-specific behavior

7. **Maintain complete coverage**:
   - Every component in `atom/async` should have a corresponding example
   - If adding a new component to `atom/async`, add its example simultaneously

## 🚀 Quick Start Guide

### For Complete Beginners

1. **Start with core async concepts:**

   ```bash
   # Navigate to build directory
   cd build/example/async

   # Run core examples
   ./async_core_async_worker_usage_example
   ./async_core_promise_example
   ./async_core_future_example
   ```

2. **Progress to execution systems:**

   ```bash
   ./async_execution_async_executor_example
   ./async_execution_pool_example
   ```

3. **Learn messaging patterns:**

   ```bash
   ./async_messaging_message_bus_example
   ./async_messaging_message_queue_example
   ```

4. **Explore utilities:**

   ```bash
   ./async_utils_timer_example
   ./async_utils_lodash_example
   ```

5. **Try integration examples:**

   ```bash
   ./async_component_integration
   ```

### For Experienced Developers

Jump directly to advanced examples by category:

```bash
# Advanced execution
./async_execution_parallel_example
./async_execution_packaged_task_example

# Synchronization primitives
./async_sync_limiter_example
./async_sync_trigger_example

# Threading utilities
./async_threading_lock_example
./async_threading_thread_wrapper_example

# Real-world application
./async_web_server_example
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
