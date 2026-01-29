# atom/memory - Memory Management Module

> **Module Version:** 1.0.0
> **Documentation Version:** 1.0.0
> **Last Updated:** 2025-01-15

---

## Navigation

[Root Directory](../../CLAUDE.md) > **memory**

---

## Module Overview

The **atom::memory** module provides advanced memory management utilities for high-performance C++ applications. It offers various memory pool implementations, allocators, and tracking tools to optimize memory usage and reduce fragmentation.

### Key Features

- **Variable-Size Memory Pool**: PMR-compatible with growth strategies
- **Fixed-Size Block Pools**: Optimized for uniform allocations
- **Object Pools**: RAII-based object pooling with smart pointers
- **Arena Allocators**: Stack-based temporary allocation
- **Memory Tracking**: Debug and profiling tools
- **Ring Buffers**: Lock-free circular buffers
- **Shared Pointers**: Custom smart pointer implementations

---

## Memory Pool Types

The module provides multiple memory pool types for different use cases:

### 1. MemoryPool (Variable-Size)

**File:** `memory.hpp`

A variable-size memory pool implementing `std::pmr::memory_resource`. Suitable for applications with mixed allocation sizes.

**Use when:**

- You need different-sized allocations
- PMR compatibility is required
- You want growth strategy control

### 2. FixedBlockPool (Fixed-Size)

**File:** `memory_pool.hpp`

A fixed-size block allocator optimized for uniform allocations. Simpler and faster than MemoryPool for single-size allocations.

**Use when:**

- All allocations are the same size
- Maximum performance is needed
- Simple pooling is sufficient

### 3. SimpleObjectPool (Object-Oriented)

**File:** `memory_pool.hpp`

Wrapper around FixedBlockPool with RAII smart pointers.

**Use when:**

- You want simple object pooling
- Automatic cleanup is desired
- PoolPtr smart pointer fits your design

### 4. ObjectPool (Advanced)

**File:** `object.hpp`

Advanced object pool with extensive features including priority-based allocation, batch operations, validation, and statistics.

**Use when:**

- You need advanced pooling features
- Statistics and monitoring are important
- Timeouts and auto-cleanup are needed

### 5. Arena (Stack-Based)

**File:** `short_alloc.hpp`

Stack-based arena allocator for temporary allocations.

**Use when:**

- You need stack-based temporary allocations
- Very fast allocation/deallocation is critical
- Scoped allocations are acceptable

---

## Core Components

### Variable-Size Memory Pool

```cpp
#include "atom/memory/memory.hpp"

using namespace atom::memory;

// Create pool with exponential growth strategy
MemoryPool<MyType, 4096, alignof(std::max_align_t)> pool(
    std::make_unique<ExponentialBlockSizeStrategy>(2.0)
);

// Allocate objects
MyType* obj = pool.allocate(10);

// Allocate with debugging tags
MyType* tagged = pool.allocateTagged(10, "MyTag", __FILE__, __LINE__);

// Deallocate
pool.deallocate(obj, 10);

// Get statistics
auto stats = pool.getTotalAllocated();
auto fragmentation = pool.getFragmentationRatio();

// Compact memory
size_t compacted = pool.compact();
```

### Fixed-Size Block Pool

```cpp
#include "atom/memory/memory_pool.hpp"

using namespace atom::memory;

// Pool for 64-byte blocks, 1024 blocks per chunk
FixedBlockPool<64, 1024> pool;

// Allocate
void* ptr = pool.allocate();

// Deallocate
pool.deallocate(ptr);

// Get statistics
auto [allocated, total] = pool.get_stats();
std::cout << "Allocated: " << allocated << "/" << total << "\n";
```

### Simple Object Pool

```cpp
#include "atom/memory/memory_pool.hpp"

using namespace atom::memory;

// Pool for MyObject objects
SimpleObjectPool<MyObject, 1024> pool;

// Allocate with constructor arguments
MyObject* obj = pool.allocate(arg1, arg2);

// Deallocate (calls destructor)
pool.deallocate(obj);

// Use with RAII smart pointer
PoolPtr<MyObject> ptr = make_pool_ptr(pool, arg1, arg2);
// Automatically returned to pool when ptr goes out of scope
```

### ObjectPool (Advanced)

```cpp
#include "atom/memory/object.hpp"

using namespace atom::memory;

// Create advanced object pool
ObjectPool<MyObject> pool(
    ObjectPoolConfig{}
        .setInitialSize(100)
        .setMaxSize(1000)
        .setGrowthFactor(2.0)
        .setEnableValidation(true)
);

// Allocate with priority
auto obj = pool.allocate(Priority::High, arg1, arg2);

// Batch operations
std::vector<MyObject*> objects = pool.allocateBatch(10);

// Get statistics
auto stats = pool.getStatistics();
std::cout << "Hit rate: " << stats.hitRate << "%\n";
```

---

## Public Interfaces

### MemoryPool Class

```cpp
template <typename T, size_t BlockSize = 4096,
          size_t Alignment = alignof(std::max_align_t)>
class MemoryPool : public std::pmr::memory_resource {
public:
    // Construction
    explicit MemoryPool(
        std::unique_ptr<BlockSizeStrategy> strategy =
            std::make_unique<ExponentialBlockSizeStrategy>());

    // Allocation
    [[nodiscard]] T* allocate(size_t n);
    [[nodiscard]] T* allocateTagged(size_t n, const std::string& tag,
                                    const std::string& file = "",
                                    int line = 0);

    // Deallocation
    void deallocate(T* p, size_t n);

    // Operations
    void reset();
    size_t compact();
    void reserve(size_t expected_allocations, size_t avg_size = sizeof(T));

    // Statistics
    [[nodiscard]] size_t getTotalAllocated() const noexcept;
    [[nodiscard]] size_t getTotalAvailable() const noexcept;
    [[nodiscard]] size_t getAllocationCount() const noexcept;
    [[nodiscard]] size_t getDeallocationCount() const noexcept;
    [[nodiscard]] double getFragmentationRatio() const;

    // Debugging
    [[nodiscard]] std::optional<MemoryTag> findTag(void* ptr) const;
    [[nodiscard]] std::unordered_map<void*, MemoryTag> getTaggedAllocations() const;
};
```

### FixedBlockPool Class

```cpp
template <std::size_t BlockSize = 64, std::size_t BlocksPerChunk = 1024>
class FixedBlockPool {
public:
    [[nodiscard]] void* allocate();
    void deallocate(void* ptr) noexcept;

    std::pair<std::size_t, std::size_t> get_stats() const noexcept;
    bool is_empty() const noexcept;
    void reset() noexcept;
};
```

### SimpleObjectPool Class

```cpp
template <typename T, std::size_t BlocksPerChunk = 1024>
class SimpleObjectPool {
public:
    template <typename... Args>
    [[nodiscard]] T* allocate(Args&&... args);
    void deallocate(T* ptr) noexcept;

    std::pair<std::size_t, std::size_t> get_stats() const noexcept;
    bool is_empty() const noexcept;
    void reset() noexcept;
};
```

### PoolPtr Smart Pointer

```cpp
template <typename T>
class PoolPtr {
public:
    PoolPtr() noexcept = default;
    explicit PoolPtr(T* ptr, SimpleObjectPool<T>* pool) noexcept;

    void reset(T* ptr = nullptr, SimpleObjectPool<T>* pool = nullptr);
    T* release() noexcept;
    T* get() const noexcept;
    T& operator*() const;
    T* operator->() const noexcept;
    explicit operator bool() const noexcept;

    void swap(PoolPtr& other) noexcept;
};
```

---

## Ring Buffer

```cpp
#include "atom/memory/ring.hpp"

using namespace atom::memory;

// Thread-safe ring buffer
RingBuffer<int, 1024> buffer;

// Write
buffer.push(42);
buffer.tryPush(43);  // Non-blocking

// Read
int value = buffer.pop();
auto result = buffer.tryPop();  // Returns std::optional<int>

// Query
std::cout << "Size: " << buffer.size() << "\n";
std::cout << "Empty: " << buffer.empty() << "\n";
```

---

## Memory Tracking

```cpp
#include "atom/memory/tracker.hpp"

using namespace atom::memory;

// Enable memory tracking
MemoryTracker::getInstance().enable();

// Allocate memory (tracked automatically)
auto* ptr = new int(42);

// Get statistics
auto stats = MemoryTracker::getInstance().getStatistics();
std::cout << "Total allocated: " << stats.totalAllocated << " bytes\n";
std::cout << "Allocation count: " << stats.allocationCount << "\n";

// Get leak report
auto leaks = MemoryTracker::getInstance().getLeaks();
for (const auto& leak : leaks) {
    std::cout << "Leak: " << leak.size << " bytes at "
              << leak.address << "\n";
}
```

---

## Dependencies

### Required Dependencies

- **atom::meta**: Concept and type traits
- **atom::type**: NonCopyable and other type utilities

### Optional Dependencies

- **spdlog**: Enhanced logging for debugging
- **Boost**: Optional Boost.Pool integration

---

## Build Configuration

### Header-Only Mode

Most components are header-only. Some features require compilation:

```cmake
# Build as header-only (default)
set(IS_HEADER_ONLY TRUE)

# Build with tracking support
add_executable(my_app main.cpp tracker.cpp)
```

### Platform Considerations

- **Windows**: Uses `_aligned_malloc`/`_aligned_free`
- **Linux/macOS**: Uses `aligned_alloc`/`free`

---

## Performance Characteristics

### Allocation Speed

| Pool Type | Allocation Speed | Deallocation Speed | Best For |
|-----------|-----------------|-------------------|----------|
| MemoryPool | Medium | Medium | Mixed sizes |
| FixedBlockPool | Fast | Very Fast | Uniform sizes |
| SimpleObjectPool | Fast | Fast | Object pooling |
| Arena | Very Fast | N/A (bulk) | Temporaries |

### Memory Overhead

| Pool Type | Per-Block Overhead | Fragmentation |
|-----------|-------------------|---------------|
| MemoryPool | ~16 bytes | Low (with compaction) |
| FixedBlockPool | 0 bytes | None |
| SimpleObjectPool | 0 bytes | None |
| Arena | 0 bytes | None |

---

## Usage Patterns

### RAII with PoolPtr

```cpp
{
    SimpleObjectPool<MyObject> pool;
    PoolPtr<MyObject> obj = make_pool_ptr(pool, arg1, arg2);

    // Use obj
    obj->doSomething();

} // obj automatically returned to pool
```

### Scoped Arena Allocation

```cpp
void processBatch() {
    Arena<1024 * 64> arena;  // 64KB arena

    // All allocations from arena
    std::vector<int, ArenaAllocator<int>> vec(arena);
    vec.push_back(1);
    vec.push_back(2);

    // Arena automatically freed when scope exits
}
```

### PMR Integration

```cpp
#include <memory_resource>

MemoryPool<MyType> pool;
std::pmr::vector<MyType> vec(&pool);

vec.resize(100);  // Allocated from pool
```

---

## Debugging and Profiling

### Tagged Allocations

```cpp
// Tag allocations for debugging
auto* textureData = pool.allocateTagged(
    textureSize, "Texture", __FILE__, __LINE__
);

// Find tags later
if (auto tag = pool.findTag(textureData)) {
    std::cout << "Allocated at " << tag->file << ":" << tag->line << "\n";
}
```

### Memory Leak Detection

```cpp
// Enable tracking at startup
MemoryTracker::getInstance().enable();

// At shutdown
if (auto leaks = MemoryTracker::getInstance().getLeaks(); !leaks.empty()) {
    std::cerr << "Memory leaks detected:\n";
    for (const auto& leak : leaks) {
        std::cerr << "  " << leak.size << " bytes at " << leak.address << "\n";
    }
}
```

---

## Testing

### Test Organization

Tests are located in `tests/memory/`:

- `test_memory_pool.cpp`: Variable-size pool tests
- `test_fixed_pool.cpp`: Fixed-size pool tests
- `test_object_pool.cpp`: Object pool tests
- `test_arena.cpp`: Arena allocator tests

### Running Tests

```bash
# Build tests
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run memory tests
ctest -R memory_ --output-on-failure
```

---

## Best Practices

### When to Use Memory Pools

**Use memory pools when:**

- You have many small allocations of the same size
- You need to reduce heap fragmentation
- You want predictable allocation performance
- You're allocating/deallocating frequently

**Avoid memory pools when:**

- Allocation sizes vary widely and unpredictably
- You have very few allocations overall
- Memory is constrained (pools reserve memory upfront)

### Pool Selection Guide

```
Need PMR compatibility?
├── Yes → MemoryPool
└── No → Uniform allocation sizes?
    ├── Yes → FixedBlockPool / SimpleObjectPool
    └── No → Need advanced features?
        ├── Yes → ObjectPool
        └── No → Standard allocator
```

---

## Related Modules

- **atom::containers**: Lock-free containers and queues
- **atom::meta**: Type traits and metaprogramming
- **atom::type**: Type utilities and wrappers

---

## Change Log

### 2025-01-15

- Initial module documentation
- Documented all memory pool types
- Added performance characteristics and best practices

---

**Maintained By:** Atom Framework Team
