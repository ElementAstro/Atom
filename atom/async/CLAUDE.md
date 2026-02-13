# atom/async - Async Primitives

[根目录](../../CLAUDE.md) > **async**

---

## Module Overview

The `atom/async` module provides comprehensive asynchronous programming primitives for high-performance C++ applications. It offers futures, promises, thread pools, message queues, event systems, and synchronization utilities designed for concurrent and parallel programming.

**Key Responsibilities:**

- Future/Promise pattern for asynchronous results
- Thread pools and executors for task execution
- Message passing and event systems
- Thread synchronization primitives
- Async utilities (timers, generators, daemons)

---

## Module Structure

```
atom/async/
├── async.hpp              # Main backward compatibility header
├── async_executor.hpp     # Async executor interface
├── daemon.hpp             # Daemon process utilities
├── eventstack.hpp         # Event stack for event handling
├── future.hpp             # Future (backward compat)
├── generator.hpp          # Generator utilities
├── limiter.hpp            # Rate limiter
├── lock.hpp               # Lock utilities (backward compat)
├── lodash.hpp             # Async utilities
├── message_bus.hpp        # Message passing bus
├── message_queue.hpp      # Thread-safe message queue
├── packaged_task.hpp      # Packaged task (backward compat)
├── parallel.hpp           # Parallel algorithms
├── pool.hpp               # Thread pool
├── promise.hpp            # Promise (backward compat)
├── queue.hpp              # Queue utilities
├── safetype.hpp           # Safe type wrappers
├── slot.hpp               # Slot/signal mechanism
├── thread_wrapper.hpp     # Thread wrapper (backward compat)
├── threadlocal.hpp        # Thread-local storage
├── timer.hpp              # Timer utilities (backward compat)
├── trigger.hpp            # Trigger/condition
├── core/                  # Core async primitives
│   ├── async.hpp          # Core async types
│   ├── future.hpp         # Future implementation
│   ├── promise.hpp        # Promise implementation
│   ├── promise_awaiter.hpp # C++20 coroutines support
│   ├── promise_fwd.hpp    # Forward declarations
│   ├── promise_impl.hpp   # Promise implementation details
│   ├── promise_utils.hpp  # Promise utilities
│   └── promise_void_impl.hpp # void specialization
├── threading/             # Threading utilities
│   ├── lock.hpp           # Lock implementations
│   ├── thread_wrapper.hpp # Thread wrapper
│   └── threadlocal.hpp    # Thread-local storage
├── messaging/             # Message passing
│   ├── eventstack.hpp     # Event stack
│   ├── message_bus.hpp    # Message bus
│   ├── message_queue.hpp  # Message queue
│   └── queue.hpp          # Queue implementations
├── execution/             # Task execution
│   ├── async_executor.hpp # Async executor
│   ├── packaged_task.hpp  # Packaged task
│   ├── parallel.hpp       # Parallel algorithms
│   └── pool.hpp           # Thread pool
├── sync/                  # Synchronization
│   ├── limiter.hpp        # Rate limiter
│   ├── safetype.hpp       # Thread-safe wrappers
│   ├── slot.hpp           # Slot/signal
│   └── trigger.hpp        # Trigger
└── utils/                 # Utilities
    ├── daemon.hpp         # Daemon utilities
    ├── generator.hpp      # Generator
    ├── lodash.hpp         # Async utilities
    └── timer.hpp          # Timer
```

---

## Public Interfaces

### Promise/Future

```cpp
namespace atom::async {

template <typename T>
class Promise {
public:
    Promise();
    ~Promise();

    // Set the value
    void setValue(const T& value);
    void setValue(T&& value);
    void setException(std::exception_ptr ex);

    // Get the associated future
    Future<T> getFuture();

    // State query
    bool isFulfilled() const;
};

template <typename T>
class Future {
public:
    // Wait for the result
    T get() const;
    void wait() const;

    // Callbacks
    template <typename F>
    auto then(F&& func) -> Future<decltype(func(std::declval<T>()))>;

    // State query
    bool isReady() const;
    bool hasValue() const;
    bool hasException() const;
};

}  // namespace atom::async
```

### Async Executor

```cpp
namespace atom::async {

class AsyncExecutor {
public:
    AsyncExecutor(size_t num_threads = std::thread::hardware_concurrency());
    ~AsyncExecutor();

    // Submit work
    template <typename F, typename... Args>
    auto submit(F&& func, Args&&... args) -> Future<decltype(func(args...))>;

    // Batch submission
    template <typename F>
    void submitBatch(std::vector<F> tasks);

    // Control
    void shutdown();
    void wait();
    size_t getThreadCount() const;
    size_t getPendingTaskCount() const;
};

}  // namespace atom::async
```

### Thread Pool

```cpp
namespace atom::async {

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    // Submit work
    template <typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> Future<decltype(f(args...))>;

    // Resize pool
    void resize(size_t num_threads);

    // Control
    void wait();
    void clear();
    void pause();
    void resume();

    // Status
    size_t getThreadCount() const;
    size_t getActiveThreadCount() const;
    size_t getPendingTaskCount() const;
};

}  // namespace atom::async
```

### Message Queue

```cpp
namespace atom::async {

template <typename T>
class MessageQueue {
public:
    MessageQueue(size_t max_size = std::numeric_limits<size_t>::max());

    // Thread-safe push/pop
    bool push(const T& value);
    bool push(T&& value);
    bool pop(T& value);
    std::optional<T> tryPop();
    template <typename Rep, typename Period>
    std::optional<T> tryPopFor(std::chrono::duration<Rep, Period> timeout);

    // Status
    [[nodiscard]] size_t size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] bool full() const;
};

}  // namespace atom::async
```

### Message Bus

```cpp
namespace atom::async {

class MessageBus {
public:
    using MessageId = std::size_t;
    using HandlerId = std::size_t;
    using Message = std::any;

    // Subscribe to messages
    template <typename T>
    HandlerId subscribe(std::function<void(const T&)> handler);

    // Unsubscribe
    void unsubscribe(HandlerId handler_id);

    // Publish messages
    template <typename T>
    void publish(const T& message);

    // Publish with return value (future)
    template <typename T>
    std::vector<std::future<void>> publishAsync(const T& message);
};

}  // namespace atom::async
```

### Rate Limiter

```cpp
namespace atom::async {

class RateLimiter {
public:
    RateLimiter(size_t max_operations, std::chrono::milliseconds time_window);

    // Try to acquire a token
    bool tryAcquire();
    bool acquire();  // Blocking

    // Wait for token with timeout
    template <typename Rep, typename Period>
    bool tryAcquireFor(std::chrono::duration<Rep, Period> timeout);

    // Reset limiter
    void reset();

    // Status
    [[nodiscard]] size_t getAvailableTokens() const;
};

}  // namespace atom::async
```

### Timer

```cpp
namespace atom::async {

class Timer {
public:
    Timer();

    // Start/reset timer
    void start();
    void reset();

    // Elapsed time
    template <typename Duration = std::chrono::milliseconds>
    Duration elapsed() const;

    // Check if elapsed
    template <typename Duration>
    bool hasElapsed(Duration duration) const;

    // Sleep
    template <typename Duration>
    static void sleep(Duration duration);
};

}  // namespace atom::async
```

---

## Dependencies

### Required Dependencies

- **atom-utils** - Utility functions

### Optional Dependencies

- **spdlog** - Logging support

---

## Usage Examples

### Basic Promise/Future

```cpp
#include "atom/async/core/promise.hpp"
#include "atom/async/core/future.hpp"

void examplePromiseFuture() {
    atom::async::Promise<int> promise;
    auto future = promise.getFuture();

    // In another thread or callback
    std::thread([&promise]() {
        // Do some work
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        promise.setValue(42);
    }).detach();

    // Wait for result
    int result = future.get();
    ATOM_INFO("Result: {}", result);  // Output: Result: 42
}
```

### Async Executor

```cpp
#include "atom/async/execution/async_executor.hpp"

void exampleExecutor() {
    atom::async::AsyncExecutor executor(4);

    // Submit work
    auto future1 = executor.submit([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 42;
    });

    auto future2 = executor.submit([]() {
        return std::string("Hello");
    });

    // Wait for results
    int result1 = future1.get();
    std::string result2 = future2.get();

    ATOM_INFO("Results: {}, {}", result1, result2);

    executor.shutdown();
}
```

### Thread Pool

```cpp
#include "atom/async/execution/pool.hpp"

void exampleThreadPool() {
    atom::async::ThreadPool pool(8);

    std::vector<atom::async::Future<int>> futures;

    // Submit multiple tasks
    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.enqueue([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return i * i;
        }));
    }

    // Wait for all results
    for (auto& future : futures) {
        ATOM_INFO("Result: {}", future.get());
    }
}
```

### Message Queue

```cpp
#include "atom/async/messaging/message_queue.hpp"

void producerConsumer() {
    atom::async::MessageQueue<int> queue(100);

    // Producer thread
    std::thread producer([&queue]() {
        for (int i = 0; i < 1000; ++i) {
            queue.push(i);
        }
    });

    // Consumer thread
    std::thread consumer([&queue]() {
        int value;
        while (queue.pop(value)) {
            ATOM_INFO("Consumed: {}", value);
        }
    });

    producer.join();
    consumer.join();
}
```

### Message Bus

```cpp
#include "atom/async/messaging/message_bus.hpp"

struct UpdateEvent {
    int entityId;
    float newX;
    float newY;
};

void exampleMessageBus() {
    atom::async::MessageBus bus;

    // Subscribe to events
    auto handler_id = bus.subscribe<UpdateEvent>([](const UpdateEvent& event) {
        ATOM_INFO("Entity {} moved to ({}, {})",
                  event.entityId, event.newX, event.newY);
    });

    // Publish events
    bus.publish(UpdateEvent{123, 10.5f, 20.3f});
    bus.publish(UpdateEvent{456, 15.0f, 25.0f});

    // Unsubscribe when done
    bus.unsubscribe(handler_id);
}
```

### Rate Limiting

```cpp
#include "atom/async/sync/limiter.hpp"

void exampleRateLimiter() {
    // Allow 10 operations per second
    atom::async::RateLimiter limiter(10, std::chrono::milliseconds(1000));

    for (int i = 0; i < 20; ++i) {
        if (limiter.tryAcquire()) {
            ATOM_INFO("Operation {} allowed", i);
        } else {
            ATOM_WARN("Operation {} rate limited", i);
        }
    }
}
```

---

## Testing

The module does not currently have dedicated unit tests. Tests should be added in `tests/async/`:

### Test Structure

```
tests/async/
├── CMakeLists.txt
├── test_promise_future.cpp   # Promise/Future tests
├── test_executor.cpp         # Executor tests
├── test_thread_pool.cpp      # Thread pool tests
├── test_message_queue.cpp    # Message queue tests
├── test_message_bus.cpp      # Message bus tests
├── test_limiter.cpp          # Rate limiter tests
└── test_timer.cpp           # Timer tests
```

---

## Build Options

```cmake
# Create library
add_library(atom-async STATIC ${SOURCES} ${HEADERS})
add_library(atom::async ALIAS atom-async)

# Optional spdlog for logging
find_package(spdlog QUIET)
if(spdlog_FOUND)
    target_link_libraries(atom-async PRIVATE spdlog::spdlog)
endif()

# Link thread library
target_link_libraries(atom-async PRIVATE atom-utils ${CMAKE_THREAD_LIBS_INIT})
```

---

## Common Patterns

### Parallel For Loop

```cpp
#include "atom/async/execution/parallel.hpp"

void parallelForExample() {
    std::vector<int> data(10000);

    atom::async::parallelFor(data.begin(), data.end(),
        [](int& value) {
            value = computeExpensive(value);
        },
        8  // num threads
    );
}
```

### Async Chain with Then

```cpp
void asyncChain() {
    atom::async::Promise<int> promise;
    auto future = promise.getFuture();

    // Chain operations
    auto result = future
        .then([](int value) {
            return value * 2;
        })
        .then([](int value) {
            return std::to_string(value);
        })
        .get();  // Blocks until complete

    promise.setValue(21);
    // result == "42"
}
```

### Producer-Consumer with Multiple Consumers

```cpp
void multiConsumer() {
    atom::async::MessageQueue<Task> queue;

    // Start multiple consumers
    std::vector<std::thread> consumers;
    for (int i = 0; i < 4; ++i) {
        consumers.emplace_back([&queue]() {
            Task task;
            while (queue.pop(task)) {
                processTask(task);
            }
        });
    }

    // Producer
    for (int i = 0; i < 1000; ++i) {
        queue.push(createTask(i));
    }

    // Join all
    for (auto& consumer : consumers) {
        consumer.join();
    }
}
```

---

## Performance Considerations

### Thread Pool Sizing

- **CPU-bound tasks**: `num_threads = hardware_concurrency`
- **I/O-bound tasks**: `num_threads > hardware_concurrency`
- **Mixed workloads**: Use separate pools for different task types

### Message Queue Sizing

- **Unbounded** (`max_size = max`): Best throughput, risk of OOM
- **Bounded** (`max_size = N`): Backpressure when full
- Choose based on producer/consumer speed ratio

### Message Bus Overhead

- Copy overhead for message types
- Consider `std::shared_ptr` for large messages
- Handler execution is synchronous

---

## See Also

- [atom/utils](../utils/CLAUDE.md) - Utility functions
- [atom/io](../io/CLAUDE.md) - Async I/O operations
- [atom/connection](../connection/CLAUDE.md) - Networking (uses async module)

---

## Change Log

### 2025-01-15

- Initial module documentation created
- Documented Promise/Future, Executor, ThreadPool, MessageQueue, MessageBus
- Added usage examples and common patterns
- Documented performance considerations
