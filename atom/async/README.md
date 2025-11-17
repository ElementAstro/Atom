# Atom Async Module

This directory contains the asynchronous programming components for the Atom framework.

## Directory Structure

The async module has been refactored to follow a clean, organized structure:

```
atom/async/
├── CMakeLists.txt              # CMake build configuration
├── xmake.lua                   # XMake build configuration
├── README.md                   # This file
├── [compatibility headers]     # Backward compatibility headers (deprecated)
├── core/                       # Core async primitives
│   ├── async.hpp              # Main async worker functionality
│   ├── future.hpp             # Enhanced future implementation
│   ├── promise.hpp            # Promise implementation
│   ├── promise.cpp            # Promise implementation
│   ├── promise_awaiter.hpp    # Coroutine awaiter support
│   ├── promise_fwd.hpp        # Forward declarations
│   ├── promise_impl.hpp       # Promise implementation details
│   ├── promise_utils.hpp      # Promise utilities
│   └── promise_void_impl.hpp  # Void specialization
├── threading/                  # Threading primitives
│   ├── thread_wrapper.hpp     # Thread wrapper and utilities
│   ├── threadlocal.hpp        # Thread-local storage
│   ├── lock.hpp               # Lock implementations
│   └── lock.cpp               # Lock implementations
├── messaging/                  # Message passing and queues
│   ├── queue.hpp              # Various queue implementations
│   ├── message_bus.hpp        # Message bus system
│   ├── message_queue.hpp      # Message queue implementation
│   └── eventstack.hpp         # Event stack system
├── execution/                  # Task execution systems
│   ├── async_executor.hpp     # Advanced async executor
│   ├── async_executor.cpp     # Async executor implementation
│   ├── pool.hpp               # Thread pool implementations
│   ├── parallel.hpp           # Parallel execution utilities
│   └── packaged_task.hpp      # Enhanced packaged tasks
├── sync/                       # Synchronization primitives
│   ├── trigger.hpp            # Event triggers
│   ├── slot.hpp               # Slot-based synchronization
│   ├── safetype.hpp           # Thread-safe type wrappers
│   ├── limiter.hpp            # Rate limiting
│   └── limiter.cpp            # Rate limiter implementation
└── utils/                      # Utility components
    ├── timer.hpp              # Timer functionality
    ├── timer.cpp              # Timer implementation
    ├── daemon.hpp             # Daemon utilities
    ├── generator.hpp          # Generator/coroutine utilities
    └── lodash.hpp             # Functional programming utilities
```

## Backward Compatibility

All existing header file paths continue to work without modification. The root-level headers are now compatibility headers that forward to the new locations:

- `async.hpp` → `core/async.hpp`
- `future.hpp` → `core/future.hpp`
- `promise.hpp` → `core/promise.hpp`
- `thread_wrapper.hpp` → `threading/thread_wrapper.hpp`
- `lock.hpp` → `threading/lock.hpp`
- `queue.hpp` → `messaging/queue.hpp`
- `message_bus.hpp` → `messaging/message_bus.hpp`
- `async_executor.hpp` → `execution/async_executor.hpp`
- `pool.hpp` → `execution/pool.hpp`
- `trigger.hpp` → `sync/trigger.hpp`
- `timer.hpp` → `utils/timer.hpp`
- And more...

## Migration Guide

### For New Code

Use the new structured paths:

```cpp
#include "atom/async/core/promise.hpp"
#include "atom/async/threading/lock.hpp"
#include "atom/async/execution/async_executor.hpp"
```

### For Existing Code

No changes required! Existing includes will continue to work:

```cpp
#include "atom/async/promise.hpp"      // Still works
#include "atom/async/lock.hpp"         // Still works
#include "atom/async/async_executor.hpp" // Still works
```

## Key Components

### Core Async Primitives

- **Promise/Future**: Enhanced promise and future implementations with coroutine support
- **AsyncWorker**: Main async task management system

### Threading

- **Thread Wrapper**: Enhanced C++20 jthread wrapper
- **Locks**: Various lock implementations (spinlock, adaptive, etc.)
- **Thread Local**: Thread-local storage utilities

### Messaging

- **Queues**: Thread-safe, lock-free, and specialized queue implementations
- **Message Bus**: Publish-subscribe messaging system
- **Event Stack**: Event handling system

### Execution

- **Async Executor**: High-performance thread pool with priority scheduling
- **Thread Pools**: Various thread pool implementations
- **Parallel**: Parallel execution utilities

### Synchronization

- **Triggers**: Event-based synchronization
- **Rate Limiter**: Request rate limiting
- **Safe Types**: Thread-safe type wrappers

### Utilities

- **Timer**: High-precision timer system
- **Generator**: C++20 coroutine generators
- **Daemon**: Background service utilities

## Build System

The module supports both CMake and XMake build systems. The build files have been updated to reflect the new directory structure while maintaining compatibility.

## Dependencies

- C++20 compiler support
- spdlog (logging)
- Optional: Boost (for enhanced features)
- Optional: ASIO (for network async operations)

## Notes

This refactoring maintains 100% backward compatibility while providing a cleaner, more maintainable codebase structure that follows established patterns from other Atom modules.
