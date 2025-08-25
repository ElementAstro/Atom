# Atom Type System Examples

This directory contains comprehensive examples demonstrating the usage of Atom's type system utilities. The type system provides modern C++ utilities for type-safe programming, error handling, concurrent programming, and functional programming patterns.

## 📁 Example Categories

### **Core Type Utilities**

| Example | Description | Key Features |
|---------|-------------|--------------|
| `args.cpp` | Type-safe heterogeneous argument container | Type safety, validation, batch operations, thread safety |
| `argsview.cpp` | Immutable view over multiple arguments | Functional operations, type-safe access, zero-copy |
| `expected.cpp` | Monadic error handling without exceptions | Error chaining, functional composition, type safety |
| `optional.cpp` | Safe nullable value handling | Null safety, functional operations, chaining |
| `compat.cpp` | Compatibility layer for std::expected | Cross-platform compatibility, seamless fallback |

### **Container Types**

| Example | Description | Key Features |
|---------|-------------|--------------|
| `concurrent_map.cpp` | Thread-safe hash map with Robin Hood hashing | Lock-free operations, high performance, thread safety |
| `concurrent_set.cpp` | Thread-safe set container | Concurrent access, performance optimization |
| `concurrent_vector.cpp` | Thread-safe vector container | Parallel operations, memory safety |
| `auto_table.cpp` | Self-organizing hash table with access counting | Automatic optimization, performance monitoring |
| `flatmap.cpp` | Cache-friendly flat map implementation | Memory locality, fast iteration |
| `flatset.cpp` | Cache-friendly flat set implementation | Sorted storage, binary search |

### **Smart Pointers & Memory**

| Example | Description | Key Features |
|---------|-------------|--------------|
| `weak_ptr.cpp` | Enhanced weak pointer with additional features | Thread safety, timeout operations, functional style |
| `pointer.cpp` | Smart pointer utilities and helpers | Memory management, RAII patterns |
| `no_offset_ptr.cpp` | Pointer without offset calculations | Performance optimization, memory efficiency |
| `indestructible.cpp` | Objects that cannot be destroyed | Singleton patterns, global state management |

### **Small Object Optimization**

| Example | Description | Key Features |
|---------|-------------|--------------|
| `small_vector.cpp` | Vector with stack storage for small sizes | Zero heap allocation, performance optimization |
| `small_list.cpp` | List with stack storage optimization | Memory efficiency, cache locality |
| `static_vector.cpp` | Fixed-capacity vector | Compile-time bounds, no dynamic allocation |
| `static_string.cpp` | Fixed-capacity string | Stack allocation, compile-time optimization |

### **Serialization & Data**

| Example | Description | Key Features |
|---------|-------------|--------------|
| `json.cpp` | JSON handling with nlohmann/json | Serialization, parsing, type safety |
| `json-schema.cpp` | JSON schema validation | Data validation, schema enforcement |
| `rjson.cpp` | Rapid JSON implementation | High performance, custom JSON handling |
| `ryaml.cpp` | YAML parsing and generation | Configuration files, data serialization |
| `qvariant.cpp` | Qt-style variant type | Type erasure, dynamic typing |

### **Specialized Types**

| Example | Description | Key Features |
|---------|-------------|--------------|
| `robin_hood.cpp` | Robin Hood hash map implementation | High performance hashing, memory efficiency |
| `pod_vector.cpp` | Vector optimized for POD types | Performance optimization, memory layout |
| `uint.cpp` | Unsigned integer utilities | Type safety, overflow protection |
| `string.cpp` | Enhanced string utilities | String manipulation, performance |
| `trackable.cpp` | Object lifecycle tracking | Debug support, memory leak detection |
| `noncopyable.cpp` | Base class preventing copying | RAII patterns, resource management |
| `iter.cpp` | Iterator utilities and helpers | Range operations, functional programming |
| `cstream.cpp` | Stream utilities | I/O operations, formatting |
| `rtype.cpp` | Runtime type information | Reflection, dynamic typing |

### **Advanced Integration**

| Example | Description | Key Features |
|---------|-------------|--------------|
| `integration_examples.cpp` | Real-world usage patterns | Multiple utilities working together |
| `advanced_patterns.cpp` | Complex programming patterns | Monadic programming, functional composition |

## 🚀 Getting Started

### Prerequisites

- CMake 3.10 or higher
- C++20 compatible compiler
- Atom library dependencies

### Building Examples

```bash
# Build all type examples
mkdir build && cd build
cmake ..
make

# Or build specific examples
make type_args
make type_expected
make type_concurrent_map
```

### Build Options

Control which examples are built using CMake options:

```bash
# Build all type examples (default)
cmake -DATOM_EXAMPLE_TYPE_BUILD_ALL=ON ..

# Build only specific examples
cmake -DATOM_EXAMPLE_TYPE_BUILD_ALL=OFF -DATOM_EXAMPLE_TYPE_ARGS=ON ..
```

## 📖 Usage Patterns

### Error Handling with Expected

```cpp
#include "atom/type/expected.hpp"

auto divide(double a, double b) -> atom::type::expected<double, std::string> {
    if (b == 0.0) {
        return atom::type::Error<std::string>("Division by zero");
    }
    return a / b;
}

auto result = divide(10.0, 2.0)
    .map([](double x) { return x * 2; })
    .and_then([](double x) { return divide(x, 3.0); });
```

### Thread-Safe Containers

```cpp
#include "atom/type/concurrent_map.hpp"

atom::type::concurrent_map<std::string, int> map(4, 100); // 4 threads, cache size 100
map.insert("key", 42);
auto value = map.find("key");
```

### Functional Programming

```cpp
#include "atom/type/argsview.hpp"

auto view = atom::makeArgsView(1, 2, 3, 4, 5);
auto result = view
    .transform([](int x) { return x * 2; })
    .filter([](int x) { return x > 5; })
    .accumulate([](int acc, int x) { return acc + x; }, 0);
```

### Configuration Management

```cpp
#include "atom/type/args.hpp"

atom::Args config;
config.set("host", std::string("localhost"));
config.set("port", 8080);
config.set("debug", true);

auto host = config.get<std::string>("host");
auto port = config.getOr<int>("port", 3000);
```

## 🔧 Advanced Features

### Thread Safety

Many containers support different threading policies:

- `unsafe`: No synchronization (fastest)
- `reader_lock`: Concurrent reads, exclusive writes
- `mutex`: Full mutual exclusion

### Memory Optimization

- **Small Object Optimization**: Stack storage for small collections
- **Flat Containers**: Cache-friendly memory layout
- **POD Optimization**: Specialized handling for Plain Old Data

### Error Handling

- **Monadic Operations**: Chain operations that can fail
- **Type Safety**: Compile-time error prevention
- **Zero-Cost Abstractions**: No runtime overhead

## 📚 Learning Path

1. **Start with basics**: `args.cpp`, `expected.cpp`, `optional.cpp`
2. **Explore containers**: `concurrent_map.cpp`, `small_vector.cpp`
3. **Advanced patterns**: `integration_examples.cpp`, `advanced_patterns.cpp`
4. **Specialized use cases**: Choose based on your needs

## 🤝 Contributing

When adding new examples:

1. Follow the existing naming convention
2. Include comprehensive comments
3. Demonstrate both basic and advanced usage
4. Add error handling examples
5. Update this README

## 📄 License

These examples are part of the Atom project and follow the same license terms.
