# atom/type - Type Utilities

[根目录](../../CLAUDE.md) > **type**

---

## Module Overview

The `atom/type` module provides a comprehensive collection of type utilities, containers, and data structures for modern C++ development. It offers enhanced versions of standard library types, functional programming utilities (Expected), and high-performance container alternatives.

**Key Responsibilities:**

- Type-safe error handling with `Expected<T, E>` (Rust-style Result)
- High-performance container types (SmallVector, FlatMap, StaticVector)
- Concurrent data structures (ConcurrentMap, ConcurrentSet, ConcurrentVector)
- JSON and YAML utilities (with optional dependencies)
- Type utilities and metaprogramming helpers

---

## Module Structure

```
atom/type/
├── type.hpp               # Main header (deprecated, use specific headers)
├── args.hpp               # Arguments view utility
├── argsview.hpp           # Arguments view implementation
├── auto_table.hpp         # Auto-sizing table
├── compat.hpp             # Compatibility utilities
├── concurrent_map.hpp     # Thread-safe hash map
├── concurrent_set.hpp     # Thread-safe set
├── concurrent_vector.hpp  # Thread-safe vector
├── cstream.hpp            # Compile-time string streams
├── expected.hpp           # Expected<T,E> for error handling
├── flatmap.hpp            # Flat map (sorted vector)
├── flatset.hpp            # Flat set (sorted vector)
├── indestructible.hpp     # Indestructible type wrapper
├── iter.hpp               # Iterator utilities
├── json.hpp               # JSON utilities
├── json_fwd.hpp           # JSON forward declarations
├── json-schema.hpp        # JSON schema utilities
├── no_offset_ptr.hpp      # Offset pointer (for serialization)
├── noncopyable.hpp        # Non-copyable base class
├── optional.hpp           # Optional type utilities
├── pod_vector.hpp         # POD vector (plain old data)
├── pointer.hpp            # Pointer utilities
├── qvariant.hpp           # Qt-style variant
├── rjson.hpp/cpp          # JSON parsing utilities
├── rtype.hpp              # Runtime type information
├── ryaml.hpp/cpp          # YAML parsing utilities
├── robin_hood.hpp         # Robin Hood hash map
├── small_list.hpp         # Small list optimization
├── small_vector.hpp       # Small vector optimization
├── static_string.hpp      # Compile-time strings
├── static_vector.hpp      # Fixed-capacity vector
├── string.hpp             # String utilities
├── trackable.hpp          # Trackable objects
├── uint.hpp               # Unsigned integer utilities
└── weak_ptr.hpp           # Weak pointer utilities
```

---

## Public Interfaces

### Expected<T, E> (Rust-style Result)

```cpp
namespace atom::type {

template <typename T, typename E = std::string>
class expected {
public:
    using value_type = T;
    using error_type = E;

    // Constructors
    constexpr expected() noexcept;
    template <typename U>
    constexpr expected(U&& value);
    template <typename U>
    constexpr expected(const unexpected<U>& unex);

    // Value access
    [[nodiscard]] constexpr bool has_value() const noexcept;
    constexpr T& value() &;
    constexpr T* operator->() noexcept;
    constexpr T& operator*() noexcept;

    // Error access
    [[nodiscard]] constexpr const Error<E>& error() const&;

    // Monadic operations
    template <typename Func>
    constexpr auto and_then(Func&& func) &;
    template <typename Func>
    constexpr auto map(Func&& func) &;
    template <typename Func>
    constexpr auto transform_error(Func&& func) &;
    template <typename Func>
    constexpr auto or_else(Func&& func) &;
};

// Utility functions
template <typename T>
constexpr auto make_expected(T&& value);

template <typename E>
constexpr auto make_unexpected(E&& error);

}  // namespace atom::type
```

### SmallVector<T, N>

```cpp
namespace atom::type {

template <typename T, std::size_t N = 8>
class SmallVector {
public:
    using value_type = T;
    using iterator = T*;
    using const_iterator = const T*;

    // Capacity is N (inline) + dynamic
    SmallVector();
    explicit SmallVector(size_t count);

    // Standard vector interface
    void push_back(const T& value);
    void push_back(T&& value);
    template <typename... Args>
    void emplace_back(Args&&... args);

    void pop_back();
    void clear();
    void reserve(size_t new_cap);
    void resize(size_t new_size);

    // Element access
    T& operator[](size_t pos);
    T& at(size_t pos);
    T& front();
    T& back();
    T* data();

    // Iterators
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    // Capacity
    [[nodiscard]] size_t size() const;
    [[nodiscard]] size_t capacity() const;
    [[nodiscard]] bool empty() const;
};

}  // namespace atom::type
```

### FlatMap<K, V>

```cpp
namespace atom::type {

template <typename K, typename V,
          typename Compare = std::less<K>,
          typename Container = std::vector<std::pair<K, V>>>
class FlatMap {
public:
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<K, V>;

    // Lookup
    iterator find(const K& key);
    const_iterator find(const K& key) const;
    V& at(const K& key);
    V& operator[](const K& key);

    // Modifiers
    void insert(const value_type& value);
    void insert(value_type&& value);
    template <typename... Args>
    void emplace(Args&&... args);
    void erase(const K& key);

    // Capacity
    [[nodiscard]] size_t size() const;
    [[nodiscard]] bool empty() const;

    // Iterators
    iterator begin();
    iterator end();
};

}  // namespace atom::type
```

### ConcurrentMap<K, V>

```cpp
namespace atom::type {

template <typename K, typename V,
          typename Hash = std::hash<K>,
          typename KeyEqual = std::equal_to<K>>
class ConcurrentMap {
public:
    using key_type = K;
    using mapped_type = V;

    // Thread-safe operations
    bool insert(const K& key, const V& value);
    bool insert(K&& key, V&& value);

    bool erase(const K& key);

    std::optional<V> find(const K& key) const;
    V& operator[](const K& key);

    void clear();
    [[nodiscard]] size_t size() const;
    [[nodiscard]] bool empty() const;
};

}  // namespace atom::type
```

### StaticString

```cpp
namespace atom::type {

template <std::size_t N>
class StaticString {
public:
    constexpr StaticString() = default;
    constexpr StaticString(const char (&str)[N + 1]);

    // Access
    constexpr const char& operator[](std::size_t pos) const;
    constexpr const char* data() const;
    constexpr std::size_t size() const;

    // Comparison
    constexpr auto operator<=>(const StaticString&) const = default;

    // Conversion
    constexpr operator std::string_view() const;
};

}  // namespace atom::type
```

---

## Dependencies

### Required Dependencies

- **atom-error** - Error handling
- **atom-utils** - Utility functions

### Optional Dependencies

- **nlohmann_json** - JSON parsing and generation
- **yaml-cpp** - YAML parsing and generation

---

## Usage Examples

### Expected for Error Handling

```cpp
#include "atom/type/expected.hpp"

auto divide(int a, int b) -> atom::type::expected<double, std::string> {
    if (b == 0) {
        return atom::type::make_unexpected("Division by zero");
    }
    return static_cast<double>(a) / b;
}

// Usage
void example() {
    auto result1 = divide(10, 2);
    if (result1.has_value()) {
        ATOM_INFO("Result: {}", result1.value());
    }

    auto result2 = divide(10, 0);
    if (!result2.has_value()) {
        ATOM_ERROR("Error: {}", result2.error().error());
    }

    // Monadic operations
    auto result3 = divide(10, 2)
        .and_then([](double val) {
            return atom::type::make_expected(val * 2);
        })
        .map([](double val) {
            return static_cast<int>(val);
        });
}
```

### SmallVector for Stack Storage

```cpp
#include "atom/type/small_vector.hpp"

void processSmallCollections() {
    // Uses stack storage for up to 8 elements
    atom::type::SmallVector<int, 8> vec;

    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    // Still using stack storage (no heap allocation)
    ATOM_INFO("Size: {}, Capacity: {}", vec.size(), vec.capacity());

    // Add more elements (may allocate on heap)
    for (int i = 0; i < 100; ++i) {
        vec.push_back(i);
    }

    // Iterate
    for (int val : vec) {
        // Process value
    }
}
```

### FlatMap for Cache-Friendly Lookups

```cpp
#include "atom/type/flatmap.hpp"

void exampleFlatMap() {
    atom::type::FlatMap<std::string, int> map;

    map.insert({"apple", 1});
    map.insert({"banana", 2});
    map.insert({"cherry", 3});

    // Access
    auto it = map.find("banana");
    if (it != map.end()) {
        ATOM_INFO("Found: {}", it->second);
    }

    // Sorted iteration
    for (const auto& [key, value] : map) {
        ATOM_INFO("{}: {}", key, value);
    }
}
```

### ConcurrentMap for Thread-Safe Access

```cpp
#include "atom/type/concurrent_map.hpp"

void exampleConcurrentMap() {
    atom::type::ConcurrentMap<std::string, int> map;

    // Thread-safe insert
    map.insert("key1", 100);
    map.insert("key2", 200);

    // Thread-safe lookup
    auto value = map.find("key1");
    if (value.has_value()) {
        ATOM_INFO("Found: {}", *value);
    }

    // Thread-safe erase
    map.erase("key1");
}
```

---

## Testing

The module has a GoogleTest suite under `tests/type/` (one `test_<header>.cpp`
or `.hpp` per header). Compiled `.cpp` tests are linked directly; header-only
`.hpp` tests are aggregated through `test_header_only.cpp` (which must `#include`
each one — several were historically orphaned and are wired in incrementally as
each header is verified). Helper types in aggregated `.hpp` tests must live in a
**named namespace** to avoid ODR clashes across files.

### Running Tests

```bash
# MSYS2 MinGW64 (selective module build)
cmake -B build/type -G Ninja -DATOM_BUILD_ALL=OFF -DATOM_BUILD_ERROR=ON \
  -DATOM_BUILD_TYPE=ON -DATOM_BUILD_UTILS=ON -DATOM_BUILD_META=ON \
  -DATOM_AUTO_RESOLVE_DEPS=ON -DATOM_BUILD_TESTS=ON \
  -DATOM_BUILD_TESTS_SELECTIVE=ON -DATOM_TEST_BUILD_TYPE=ON
cmake --build build/type --target atom_type_tests -j
ctest --test-dir build/type -L type --output-on-failure
```

### Conventions

- All types live in `namespace atom::type`; classes are `PascalCase`.
- Throwing paths integrate with `atom::error` (domain exceptions derive from
  `atom::error::Exception`); containers add `operator<=>`, `std::formatter`,
  and `[[nodiscard]]` observers, reusing `atom::meta` concepts where applicable.
- Parallel algorithm paths are guarded behind `ATOM_USE_PARALLEL_ALGORITHMS`
  (they pull in TBB via `<execution>`).

---

## Build Options

### CMake Options

```cmake
# Find optional dependencies
find_package(nlohmann_json QUIET)
find_package(yaml-cpp QUIET)

# Header-only module (mostly)
add_library(atom-type INTERFACE)
add_library(atom::type ALIAS atom-type)
atom_configure_module(atom-type HEADER_ONLY)

# Link optional dependencies
if(nlohmann_json_FOUND)
    target_link_libraries(atom-type INTERFACE nlohmann_json::nlohmann_json)
endif()
if(yaml-cpp_FOUND)
    target_link_libraries(atom-type INTERFACE yaml-cpp::yaml-cpp)
endif()
```

---

## Container Comparison

| Container | Use Case | Performance | Memory |
|-----------|----------|-------------|--------|
| **std::vector** | General purpose | Good | Medium |
| **SmallVector** | Small collections | Excellent (small N) | Low (inline) |
| **StaticVector** | Fixed max size | Excellent | Minimal |
| **std::map** | Large datasets | Medium | High |
| **FlatMap** | Small-medium maps | Good (cache-friendly) | Low |
| **ConcurrentMap** | Thread-safe | Medium-High | Medium |

---

## Common Patterns

### Return Type with Expected

```cpp
auto parseInteger(std::string_view str)
    -> atom::type::expected<int, std::string> {
    try {
        size_t pos = 0;
        int value = std::stoi(std::string(str), &pos);
        if (pos != str.length()) {
            return atom::type::make_unexpected("Invalid characters");
        }
        return value;
    } catch (const std::exception& e) {
        return atom::type::make_unexpected(e.what());
    }
}
```

### Chaining Expected Operations

```cpp
auto processWorkflow(Input input)
    -> atom::type::expected<Output, std::string> {
    return validate(input)
        .and_then([](auto valid) {
            return transform(valid);
        })
        .and_then([](auto transformed) {
            return save(transformed);
        });
}
```

### Using SmallVector for Function Parameters

```cpp
void processArguments(atom::type::SmallVector<std::string, 4> args) {
    for (const auto& arg : args) {
        ATOM_INFO("Arg: {}", arg);
    }
}

// No heap allocation for up to 4 arguments
processArguments({"arg1", "arg2", "arg3"});
```

---

## Performance Considerations

### SmallVector

- **N** should be chosen based on typical size
- Too large: wastes stack space
- Too small: frequent heap allocations
- Good default: 4-16 elements

### FlatMap

- Best for small to medium maps (< 1000 elements)
- Better cache locality than std::map
- Slower insertion than hash map
- Faster lookup than std::map for small N

### ConcurrentMap

- Uses fine-grained locking
- Good for read-heavy workloads
- May have contention on write-heavy scenarios
- Consider lock-free alternatives for high contention

---

## See Also

- [atom/error](../error/CLAUDE.md) - Error handling
- [atom/utils](../utils/CLAUDE.md) - Utility functions
- [atom/containers](../containers/CLAUDE.md) - Advanced containers

---

## Change Log

### 2026-06-16

- **Every previously-orphaned header test is now wired and passing** (812 tests,
  stable across reruns; the `tests/type` aggregator now `#include`s all 22
  header-only tests, and rtype is compiled as its own TU). Newly wired this pass:
  static_string, iter, flatmap, json-schema, pointer, weak_ptr, rtype,
  concurrent_map, concurrent_set; concurrent_vector flakiness fixed.
- Real bug fixes: `iter` (processContainer dangling-pointer crash; ZipIterator
  unequal-length infinite loop); `flatmap`/`json-schema`/`rtype` serialization &
  validation (operator==, `is_number_integer` guards, type dispatch, JsonValue
  `int`/`const char*` ctors); `pointer` move double-free + `atom::error`;
  `concurrent_set`/`concurrent_map`/`concurrent_vector` thread-pool lifecycle
  (lost-wakeup hangs, join-under-lock deadlocks, element loss, move-of-live-threads,
  transaction-rollback cache); `concurrent_map` no longer depends on `atom::search`
  (embedded `KeyValueLRUCache`).
- Build: `tests/type/CMakeLists.txt` gained `-Wa,-mbig-obj` (MSVC `/bigobj`) — the
  aggregated TU exceeds the COFF section limit on MinGW.
- A handful of stress tests are `DISABLED_` for a MinGW winpthreads
  `std::shared_mutex` assertion under extreme read-lock churn (an environment
  limitation, not a logic defect) — documented at each site.
- **All 35 `example/type/*.cpp` rebuilt**: 29 were pre-corrupted (comments glued
  into code) and did not compile; each was rewritten into a clean, minimal,
  compiling example of the header's real API (35/35 now compile; representative
  ones run-verified). Fixed a latent `#include <cassert>` omission in
  small_vector.hpp found in the process.
- **JsonValue accessors unified to snake_case** (`as_string`/`as_number`/… +
  `to_string`) to match YamlValue and the project convention; added
  `JsonValue(int)`/`JsonValue(const char*)` to remove ctor ambiguities.
- **Dependency audit**: removed the inverted `concurrent_map → atom/search`
  dependency (embedded a self-contained LRU); rtype↔meta reflection confirmed
  NOT duplication (different serialization backends: self-built rjson/ryaml vs
  vendored nlohmann/yaml-cpp).
- **Learned from the canonical reference (C++23 `std::expected`)**: audited
  `expected`'s monadic surface against the standard and closed the gaps —
  added `error_or(G&&)` (error-channel analogue of `value_or`, previously
  missing) and `transform()` (the std-canonical name for the value-mapping
  op `map`, for std-interface parity). Robin-hood map, the LRU caches, and the
  RAII/condition-variable concurrency fixes likewise follow established
  best-practice patterns.

### 2026-06-15

- Namespace normalization: every header's types moved into `namespace atom::type`
  (previously several were global-scope, bare `atom`, or misplaced in
  `atom::containers`/`atom::utils`); cross-module consumers updated, no compat
  aliases. Header guards standardized to `ATOM_TYPE_<NAME>_HPP`.
- Modernization: added `operator<=>`/`operator==`, `std::formatter`, and
  `[[nodiscard]]` to containers/wrappers; integrated `atom::error` exceptions;
  reused `atom::meta` concepts where applicable.
- Fixed numerous pre-existing bugs surfaced by wiring previously-orphaned tests
  (small_vector/small_list/flatset/static_vector/concurrent_vector/optional/
  indestructible/no_offset_ptr/cstream/args), including crashes (infinite
  recursion, null deref, iterator invalidation, empty-container UB) and
  exception-safety issues. Full `tests/type` suite green.

### 2025-01-15

- Initial module documentation created
- Documented Expected<T,E> and major container types
- Added usage examples for common patterns
- Documented performance considerations
