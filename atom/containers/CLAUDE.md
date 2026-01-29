# atom/containers - High-Performance Containers Module

> **Module Version:** 1.0.0
> **Documentation Version:** 1.0.0
> **Last Updated:** 2025-01-15

---

## Navigation

[Root Directory](../../CLAUDE.md) > **containers**

---

## Module Overview

The **atom::containers** module provides high-performance container implementations for specialized use cases. It includes lock-free data structures, intrusive containers, and advanced container adapters built on top of the Boost container libraries.

### Key Features

- **Lock-Free Containers**: Wait-free synchronization for concurrent access
- **Intrusive Containers**: Zero-allocation containers for embedded/real-time use
- **Boost Integration**: High-performance implementations using Boost libraries
- **Header-Only**: All components are header-only for easy integration

---

## Directory Structure

```
atom/containers/
├── lockfree.hpp       # Lock-free data structures
├── intrusive.hpp      # Intrusive containers
├── boost_containers.hpp # Boost container adapters
├── high_performance.hpp # High-performance utilities
└── graph.hpp          # Graph algorithms and structures
```

---

## Core Components

### Lock-Free Containers

The lockfree namespace provides lock-free (or wait-free) data structures that can be safely accessed from multiple threads without explicit synchronization.

#### Lock-Free Queue (Multi-Producer/Multi-Consumer)

```cpp
#include "atom/containers/lockfree.hpp"

using namespace atom::containers;

// Fixed-size lock-free queue (1024 elements)
lockfree::queue<int, 1024> q;

// Producer thread
q.push(42);

// Consumer thread
int value;
if (q.pop(value)) {
    std::cout << "Popped: " << value << "\n";
}
```

#### SPSC Queue (Single-Producer/Single-Consumer)

```cpp
#include "atom/containers/lockfree.hpp"

using namespace atom::containers;

// Optimized for single producer/consumer scenarios
lockfree::spsc_queue<int, 1024> q;

// Producer (single thread)
q.push(1);
q.push(2);

// Consumer (single thread)
int value;
while (q.pop(value)) {
    std::cout << value << "\n";
}
```

#### Lock-Free Stack

```cpp
#include "atom/containers/lockfree.hpp"

using namespace atom::containers;

lockfree::stack<int, 1024> s;

// Push from multiple threads
s.push(10);

// Pop from multiple threads
int value;
if (s.pop(value)) {
    std::cout << "Popped: " << value << "\n";
}
```

### Intrusive Containers

Intrusive containers avoid memory allocation overhead by storing linkage information directly within the elements.

#### Intrusive List

```cpp
#include "atom/containers/intrusive.hpp"

using namespace atom::containers;

// Element must inherit from list_base_hook
class MyNode : public intrusive::list_base_hook {
public:
    int value;
    MyNode(int v) : value(v) {}
};

intrusive::list<MyNode> myList;

// Add nodes
MyNode node1(1);
MyNode node2(2);
myList.push_back(node1);
myList.push_back(node2);

// Iterate
for (auto& node : myList) {
    std::cout << node.value << "\n";
}
```

#### Intrusive Set

```cpp
#include "atom/containers/intrusive.hpp"

using namespace atom::containers;

class MyNode : public intrusive::set_base_hook {
public:
    int value;
    MyNode(int v) : value(v) {}

    // Required for set ordering
    bool operator<(const MyNode& other) const {
        return value < other.value;
    }
};

intrusive::set<MyNode> mySet;

MyNode node1(1);
MyNode node2(2);
mySet.insert(node1);
mySet.insert(node2);

// Find
MyNode search(1);
auto it = mySet.find(search);
if (it != mySet.end()) {
    std::cout << "Found: " << it->value << "\n";
}
```

#### Intrusive Base (Multi-Container Support)

```cpp
#include "atom/containers/intrusive.hpp"

using namespace atom::containers;

// Inherit from intrusive_base to support multiple container types
class MultiContainerNode : public intrusive::intrusive_base {
public:
    int id;
    std::string data;

    MultiContainerNode(int i, const std::string& d) : id(i), data(d) {}
};

// Can be in multiple containers simultaneously
MultiContainerNode node(1, "data");

intrusive::list<MultiContainerNode> list;
intrusive::set<MultiContainerNode> set;

list.push_back(node);
set.insert(node);
```

### Boost Container Adapters

```cpp
#include "atom/containers/boost_containers.hpp"

using namespace atom::containers;

// Use Boost.FlatMap for O(log n) lookups with better cache locality
boost_flat_map<std::string, int> fmap;

fmap["one"] = 1;
fmap["two"] = 2;

auto it = fmap.find("one");
if (it != fmap.end()) {
    std::cout << it->second << "\n";
}
```

---

## Public Interfaces

### Lock-Free Queue

```cpp
template <typename T, size_t Capacity = 1024>
class queue {
public:
    queue();

    // Push element (returns false if full)
    [[nodiscard]] bool push(const T& item) noexcept;

    // Pop element (returns false if empty)
    [[nodiscard]] bool pop(T& item) noexcept;

    // Check if empty (may be stale immediately after call)
    [[nodiscard]] bool empty() const noexcept;
};
```

### Lock-Free SPSC Queue

```cpp
template <typename T, size_t Capacity = 1024>
class spsc_queue {
public:
    spsc_queue();

    // Push (single producer only)
    [[nodiscard]] bool push(const T& item) noexcept;

    // Pop (single consumer only)
    [[nodiscard]] bool pop(T& item) noexcept;

    [[nodiscard]] bool empty() const noexcept;
};
```

### Lock-Free Stack

```cpp
template <typename T, size_t Capacity = 1024>
class stack {
public:
    stack();

    [[nodiscard]] bool push(const T& item) noexcept;
    [[nodiscard]] bool pop(T& item) noexcept;
    [[nodiscard]] bool empty() const noexcept;
};
```

### Intrusive List

```cpp
template <typename T>
using list = boost::intrusive::list<T>;

// Element requirements:
// - Must inherit from list_base_hook
// - Must be stable in memory (no moving/reallocating)
```

### Intrusive Set

```cpp
template <typename T, typename Compare = std::less<T>>
using set = boost::intrusive::set<
    T, boost::intrusive::compare<Compare>>;

// Element requirements:
// - Must inherit from set_base_hook
// - Must provide ordering operator (< or Compare)
```

### Intrusive Unordered Set

```cpp
template <typename T, typename Hash = boost::hash<T>,
          typename Equal = std::equal_to<T>>
class unordered_set {
public:
    unordered_set();

    std::pair<iterator, bool> insert(T& value);
    bool remove(T& value) noexcept;
    iterator find(const T& value);

    iterator begin() noexcept;
    iterator end() noexcept;
    bool empty() const noexcept;
    size_t size() const noexcept;
    void clear() noexcept;
};
```

---

## Dependencies

### Required Dependencies

- **atom::macro**: Macro utilities (for ATOM_HAS_BOOST_* macros)

### Optional Dependencies

The module requires Boost libraries for functionality. The following Boost components are used:

| Component | Header-Only | Library | Purpose |
|-----------|-------------|---------|---------|
| Boost.Lockfree | Yes | No | Lock-free containers |
| Boost.Intrusive | Yes | No | Intrusive containers |
| Boost.Container | Yes | No | Boost containers |

### Build Configuration

```cmake
# Enable Boost lockfree support
-DATOM_USE_BOOST_LOCKFREE=ON

# Enable Boost intrusive support
-DATOM_USE_BOOST_INTRUSIVE=ON

# Enable Boost container support
-DATOM_USE_BOOST_CONTAINER=ON
```

The module is header-only when these features are enabled.

---

## Usage Patterns

### Producer-Consumer Pattern

```cpp
#include "atom/containers/lockfree.hpp"
#include <thread>
#include <vector>

using namespace atom::containers;

lockfree::queue<int, 4096> queue;

void producer() {
    for (int i = 0; i < 1000; ++i) {
        while (!queue.push(i)) {
            // Queue full, retry or spin
            std::this_thread::yield();
        }
    }
}

void consumer() {
    int value;
    int received = 0;
    while (received < 1000) {
        if (queue.pop(value)) {
            ++received;
        } else {
            std::this_thread::yield();
        }
    }
}

int main() {
    std::thread p1(producer);
    std::thread p2(consumer);

    p1.join();
    p2.join();

    return 0;
}
```

### Object Pool with Intrusive Containers

```cpp
#include "atom/containers/intrusive.hpp"

using namespace atom::containers;

class PooledObject : public intrusive::intrusive_base {
public:
    int id;
    bool inUse{false};

    PooledObject(int i) : id(i) {}
};

class ObjectPool {
    intrusive::list<PooledObject> freeList;
    intrusive::list<PooledObject> usedList;

public:
    PooledObject* acquire() {
        if (!freeList.empty()) {
            auto& obj = freeList.front();
            freeList.pop_front();
            obj.inUse = true;
            usedList.push_back(obj);
            return &obj;
        }
        return nullptr;
    }

    void release(PooledObject* obj) {
        obj->inUse = false;
        usedList.erase(usedList.iterator_to(*obj));
        freeList.push_back(*obj);
    }
};
```

---

## Performance Characteristics

### Lock-Free Containers

| Container | Push | Pop | Memory Overhead | Best Use Case |
|-----------|------|-----|-----------------|--------------|
| `queue` | Lock-free | Lock-free | Fixed (pre-allocated) | MPSC, MPMC |
| `spsc_queue` | Wait-free | Wait-free | Fixed (pre-allocated) | SPSC only |
| `stack` | Lock-free | Lock-free | Fixed (pre-allocated) | LIFO processing |

**Key characteristics:**

- Fixed capacity (compile-time)
- No memory allocation after construction
- Thread-safe without locks
- May fail on push if full

### Intrusive Containers

| Container | Insert | Erase | Find | Memory Overhead |
|-----------|--------|-------|------|-----------------|
| `list` | O(1) | O(1) | O(n) | Per-element (2-3 pointers) |
| `slist` | O(1) | O(1) | O(n) | Per-element (1 pointer) |
| `set` | O(log n) | O(log n) | O(log n) | Per-element (3 pointers + color) |
| `unordered_set` | O(1) avg | O(1) avg | O(1) avg | Per-element (next pointer) |

**Key characteristics:**

- Zero heap allocation
- Elements must be stable in memory
- Better cache locality
- Lower memory overhead than std containers

---

## Thread Safety

### Lock-Free Containers

- **Multiple threads**: Safe for concurrent push/pop
- **Single producer/consumer**: Use `spsc_queue` for better performance
- **Empty check**: Result may be stale immediately after call
- **No explicit locking**: Uses atomic operations

### Intrusive Containers

- **Not thread-safe**: External synchronization required
- **Same element in multiple containers**: Safe with different container types
- **Element removal**: Must be done from the container it's in

---

## Best Practices

### When to Use Lock-Free Containers

**Use lock-free containers when:**

- You need high-throughput producer-consumer scenarios
- You want to avoid lock contention
- Fixed capacity is acceptable
- You can handle "full" or "empty" conditions

**Avoid lock-free containers when:**

- You need dynamic capacity
- You require blocking operations
- Memory overhead is a concern (pre-allocated)

### When to Use Intrusive Containers

**Use intrusive containers when:**

- Elements can own the linkage data
- You need zero heap allocation
- Better cache performance is critical
- Elements have stable lifetime

**Avoid intrusive containers when:**

- Elements cannot be modified to add hooks
- Elements are copied/moved frequently
- You need opaque element types

---

## Testing

### Test Organization

Tests are located in `tests/containers/`:

- `test_lockfree.cpp`: Lock-free container tests
- `test_intrusive.cpp`: Intrusive container tests

### Running Tests

```bash
# Build tests
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run container tests
ctest -R containers_ --output-on-failure
```

---

## Comparison with Standard Containers

### vs std::queue

| Feature | lockfree::queue | std::queue |
|---------|----------------|------------|
| Thread-safe | Yes | No |
| Capacity | Fixed | Dynamic |
| Push fails when full | Yes | No (throws) |
| Memory allocation | Construction only | Per-element |
| Performance | High (no locks) | Varies |

### vs std::list

| Feature | intrusive::list | std::list |
|---------|-----------------|-----------|
| Heap allocation | None | Per-node |
| Thread-safe | No | No |
| Element requirements | Must inherit from hook | None |
| Cache locality | Better | Good |

---

## Related Modules

- **atom::async**: Async primitives and executors
- **atom::memory**: Memory pools and allocators
- **atom::type**: Type utilities

---

## Change Log

### 2025-01-15

- Initial module documentation
- Documented lock-free and intrusive containers
- Added performance characteristics and usage patterns

---

**Maintained By:** Atom Framework Team
