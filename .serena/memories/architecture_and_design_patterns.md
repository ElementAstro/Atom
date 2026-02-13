# Architecture and Design Patterns

## Module Architecture

### Module Structure Pattern

Each module follows a consistent structure:

```
atom/<module>/
├── CMakeLists.txt              # Module build configuration
├── <module>.hpp                # Main header (backwards compatibility)
├── core/                       # Core functionality
│   ├── <core_files>.hpp
│   └── <core_files>.cpp
├── <subcategory>/              # Functional subdirectories
│   ├── <files>.hpp
│   └── <files>.cpp
└── CLAUDE.md                   # Module documentation (if present)
```

### Dependency Hierarchy

```
Core (no dependencies):
├── error
├── type
└── containers

Low-Level (depend on Core):
├── log (depends on: error, utils)
├── meta (depends on: error, utils)
└── memory (depends on: type, error)

Mid-Level (depend on Low-Level):
├── utils (depends on: error, type)
├── algorithm (depends on: type, utils, error)
├── async (depends on: utils)
└── io (depends on: async, utils)

High-Level (depend on Mid-Level):
├── sysinfo (depends on: type, utils)
├── system (depends on: sysinfo, meta, utils)
├── serial (depends on: system, connection)
├── secret (depends on: algorithm, io)
├── search (depends on: type, io)
└── image (depends on: algorithm, io, async)

Application-Level (depend on High-Level):
├── connection (depends on: async, system)
├── components (depends on: meta, utils)
└── web (depends on: utils, io, system)
```

## Key Design Patterns

### 1. Error Handling Pattern

All modules integrate with `atom::error` system:

```cpp
#include "atom/error/error.hpp"

try {
    // Your code here
} catch (const atom::error::Exception& e) {
    ATOM_ERROR("Operation failed: {}", e.what());
    // Handle error with context
}
```

### 2. Logging Pattern

Use the unified logging framework:

```cpp
#include "atom/log/log.hpp"

ATOM_INFO("Processing image: {}", filename);
ATOM_WARN("High memory usage: {} MB", usage);
ATOM_ERROR("Failed to load: {}", error);
ATOM_DEBUG("Debug information: {}", details);
```

Log levels: TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL

### 3. RAII Pattern

Use RAII for resource management:

```cpp
class ResourceHolder {
public:
    ResourceHolder() {
        // Acquire resource
    }
    
    ~ResourceHolder() {
        // Release resource automatically
    }
    
    // Delete copy operations
    ResourceHolder(const ResourceHolder&) = delete;
    ResourceHolder& operator=(const ResourceHolder&) = delete;
    
    // Allow move operations
    ResourceHolder(ResourceHolder&&) noexcept = default;
    ResourceHolder& operator=(ResourceHolder&&) noexcept = default;
};
```

### 4. Factory Pattern

Many modules use factory patterns for object creation:

```cpp
namespace atom {
namespace algorithm {

class CompressionFactory {
public:
    static std::unique_ptr<Compressor> Create(CompressionType type);
};

}  // namespace algorithm
}  // namespace atom
```

### 5. Strategy Pattern

Used extensively in algorithm module:

```cpp
template<typename Strategy>
class AlgorithmExecutor {
    Strategy strategy_;
public:
    void execute() {
        strategy_.apply();
    }
};
```

### 6. Observer Pattern

Used in async and logging modules:

```cpp
template<typename Event>
class Observer {
public:
    virtual void onNotify(const Event& event) = 0;
};

template<typename Event>
class Subject {
    std::vector<Observer<Event>*> observers_;
public:
    void addObserver(Observer<Event>* observer);
    void notifyObservers(const Event& event);
};
```

## Memory Management Patterns

### 1. Memory Pools

Use `atom::memory` for custom allocation:

```cpp
#include "atom/memory/memory_pool.hpp"

atom::memory::MemoryPool<1024> pool;  // 1KB blocks
void* ptr = pool.allocate();
pool.deallocate(ptr);
```

### 2. Smart Pointers

Prefer smart pointers over raw pointers:

```cpp
// Ownership transfer
std::unique_ptr<MyClass> ptr = std::make_unique<MyClass>();

// Shared ownership
std::shared_ptr<MyClass> ptr = std::make_shared<MyClass>();

// Observer (no ownership)
MyClass* ptr = getRawPointer();  // Only when ownership is clear
```

### 3. Small Vector Optimization

Use `atom::type::small_vector` for small collections:

```cpp
#include "atom/type/small_vector.hpp"

atom::type::small_vector<int, 8> vec;  // Stack allocation for ≤8 elements
```

## Async Patterns

### 1. Future/Promise

```cpp
#include "atom/async/future.hpp"

atom::async::Future<int> future = ...;
auto result = future.get();  // Blocking wait

// Non-blocking
if (future.isReady()) {
    auto result = future.get();
}
```

### 2. Executor Pattern

```cpp
#include "atom/async/executor.hpp"

auto executor = atom::async::createThreadPoolExecutor(4);
executor->submit([]() {
    // Task code
});
```

## Thread Safety Patterns

### 1. Lock-Free Data Structures

Use `atom::containers` for lock-free queues:

```cpp
#include "atom/containers/lock_free_queue.hpp"

atom::containers::LockFreeQueue<int> queue;
queue.push(42);
int value = queue.pop();
```

### 2. Mutex Protection

```cpp
#include <mutex>

class ThreadSafeClass {
    mutable std::mutex mutex_;
    int data_;
public:
    void setData(int value) {
        std::lock_guard lock(mutex_);
        data_ = value;
    }
    
    int getData() const {
        std::lock_guard lock(mutex_);
        return data_;
    }
};
```

## Configuration Patterns

### 1. Compile-Time Configuration

Use CMake options for feature flags:

```cpp
#ifdef ATOM_USE_OPENCV
    // OpenCV-specific code
#endif

#ifdef ATOM_ENABLE_DEBUG
    // Debug-only code
#endif
```

### 2. Runtime Configuration

Use configuration files or environment variables:

```cpp
#include "atom/system/env.hpp"

std::string value = atom::system::getEnv("ATOM_CONFIG_PATH");
```

## Performance Patterns

### 1. Lazy Evaluation

Defer computation until needed:

```cpp
class LazyEvaluator {
    mutable bool computed_ = false;
    mutable Result result_;
    
public:
    const Result& get() const {
        if (!computed_) {
            result_ = compute();
            computed_ = true;
        }
        return result_;
    }
};
```

### 2. Move Semantics

Use move semantics for efficient transfers:

```cpp
class DataHolder {
    std::vector<int> data_;
    
public:
    // Move constructor
    DataHolder(DataHolder&& other) noexcept
        : data_(std::move(other.data_)) {}
    
    // Move assignment
    DataHolder& operator=(DataHolder&& other) noexcept {
        data_ = std::move(other.data_);
        return *this;
    }
};
```

### 3. Template Metaprogramming

Use `atom::meta` for compile-time optimizations:

```cpp
#include "atom/meta/type_traits.hpp"

template<typename T>
constexpr bool isNumeric = atom::meta::is_numeric_v<T>;
```

## Testing Patterns

### 1. Fixture Pattern

```cpp
class AlgorithmTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }
    
    void TearDown() override {
        // Cleanup code
    }
    
    // Test helpers
    void verifyResult(const Result& expected, const Result& actual);
};
```

### 2. Parameterized Tests

```cpp
class ParameterizedTest : public ::testing::TestWithParam<int> {
    // Test code
};

INSTANTIATE_TEST_SUITE_P(
    VariousInputs,
    ParameterizedTest,
    ::testing::Values(1, 2, 3, 4, 5)
);
```

## Common Anti-Patterns to Avoid

1. **Don't** use raw pointers for ownership
2. **Don't** mix error handling strategies (choose exceptions or error codes)
3. **Don't** use `using namespace` in header files
4. **Don't** put implementation in headers (except templates)
5. **Don't** ignore compiler warnings
6. **Don't** use C-style casts (use `static_cast`, `dynamic_cast`, etc.)
7. **Don't** create circular dependencies between modules
8. **Don't** put business logic in constructors/destructors
