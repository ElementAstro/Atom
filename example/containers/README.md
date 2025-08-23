# Atom Containers Module Examples

This directory contains examples demonstrating the high-performance container capabilities of the Atom framework.

## 🚀 Overview

The Atom containers module provides high-performance, memory-efficient container implementations with focus on:
- **Cache locality**: Flat containers for better memory access patterns
- **Memory optimization**: Small containers with stack storage optimization
- **Concurrency**: Lock-free containers for multi-threaded applications
- **Compatibility**: Boost container integration with standard library fallbacks

## 📁 Examples

### ✅ **High Performance Containers Example**
**File**: `high_performance_containers_example.cpp`
**Status**: Fully functional ✅

This comprehensive example demonstrates:

#### **Flat Containers**
- `flat_map` and `flat_set` implementations
- Better cache locality compared to tree-based containers
- Sorted vector-based storage for improved performance
- Memory-efficient storage with reduced fragmentation

#### **Small Containers**
- `small_vector` with stack storage optimization
- Configurable stack capacity to avoid heap allocations
- Automatic fallback to heap storage when needed
- Optimal for small collections with known size patterns

#### **Lock-Free Containers**
- Thread-safe containers for concurrent programming
- Lock-free queue implementations
- Atomic operations for high-performance multi-threading
- Suitable for producer-consumer scenarios

#### **Performance Comparisons**
- Benchmarks comparing different container types
- Memory usage analysis
- Access pattern performance tests
- Real-world usage scenarios

#### **Boost Integration**
- Seamless integration with Boost containers when available
- Graceful fallback to standard library containers
- Conditional compilation based on Boost availability
- Best-of-both-worlds approach

## 🛠️ Building and Running

### Build the Example
```bash
# Configure CMake with examples enabled
cmake -B build -S . -DATOM_EXAMPLE_BUILD_ALL=ON

# Build the containers example
cmake --build build --target containers_high_performance_containers_example
```

### Run the Example
```bash
# Execute the example
./build/example/containers/containers_high_performance_containers_example.exe
```

### Expected Output
The example will demonstrate:
1. **Flat Map Operations**: Insertion, lookup, and iteration performance
2. **Flat Set Operations**: Unique element storage and set operations
3. **Small Vector Usage**: Stack optimization and heap fallback
4. **Performance Comparisons**: Timing comparisons between container types
5. **Memory Analysis**: Memory usage patterns and optimization benefits
6. **Thread Safety**: Concurrent access patterns and lock-free operations

## 🎯 Key Features Demonstrated

### **1. Cache-Friendly Design**
```cpp
// Flat containers store elements in contiguous memory
atom::containers::flat_map<int, std::string> flatMap;
// Better cache locality than std::map for iteration
```

### **2. Memory Optimization**
```cpp
// Small vector with stack storage for up to 16 elements
atom::containers::small_vector<int, 16> smallVec;
// No heap allocation for small collections
```

### **3. Lock-Free Concurrency**
```cpp
// Thread-safe queue without locks
atom::containers::lockfree_queue<Task> taskQueue;
// High-performance producer-consumer patterns
```

### **4. Boost Integration**
```cpp
// Automatically uses Boost containers when available
#ifdef BOOST_CONTAINER_FLAT_MAP_HPP
    using FlatMap = boost::container::flat_map<K, V>;
#else
    using FlatMap = atom::containers::flat_map<K, V>;
#endif
```

## 📊 Performance Characteristics

### **Flat Containers**
- **Insertion**: O(n) for sorted insertion, O(log n) + O(n) for random
- **Lookup**: O(log n) binary search
- **Iteration**: Excellent cache locality, faster than tree containers
- **Memory**: Reduced overhead, better memory density

### **Small Containers**
- **Stack Storage**: Zero heap allocations for small sizes
- **Capacity**: Configurable stack capacity at compile time
- **Fallback**: Automatic heap allocation when stack capacity exceeded
- **Performance**: Faster allocation/deallocation for small collections

### **Lock-Free Containers**
- **Concurrency**: True parallelism without locks
- **Scalability**: Better performance under high contention
- **Wait-Free**: Some operations guarantee progress
- **Memory Ordering**: Careful atomic operations for correctness

## 🔧 Configuration Options

### **Compile-Time Configuration**
```cpp
// Configure small vector stack capacity
using MySmallVector = atom::containers::small_vector<int, 32>;

// Configure flat container growth policy
using MyFlatMap = atom::containers::flat_map<
    int, std::string,
    std::less<int>,
    atom::containers::growth_policy::exponential
>;
```

### **Runtime Configuration**
```cpp
// Reserve capacity for better performance
flatMap.reserve(1000);

// Configure lock-free queue capacity
lockfreeQueue.set_capacity(10000);
```

## 🎨 Usage Patterns

### **When to Use Flat Containers**
- ✅ Frequent iteration over elements
- ✅ Memory-constrained environments
- ✅ Cache-sensitive applications
- ✅ Mostly-read workloads
- ❌ Frequent random insertions/deletions

### **When to Use Small Containers**
- ✅ Small, known-size collections
- ✅ Temporary containers in hot paths
- ✅ Memory allocation sensitive code
- ✅ Stack-based storage preferred
- ❌ Large or highly variable sized collections

### **When to Use Lock-Free Containers**
- ✅ High-concurrency scenarios
- ✅ Producer-consumer patterns
- ✅ Real-time systems
- ✅ Lock contention issues
- ❌ Complex multi-container operations

## 🔍 Advanced Topics

### **Memory Layout Optimization**
The containers module provides detailed control over memory layout:
- Contiguous storage for better cache performance
- Alignment control for SIMD operations
- Memory pool integration for custom allocation strategies

### **Template Metaprogramming**
Advanced template techniques used:
- SFINAE for conditional compilation
- Type traits for container selection
- Concept-based constraints (C++20)

### **Platform Optimization**
Platform-specific optimizations:
- CPU cache line awareness
- NUMA-aware memory allocation
- Compiler-specific optimizations

## 🚨 Important Notes

### **Thread Safety**
- Flat containers are **not** thread-safe by default
- Small containers follow standard library thread safety rules
- Lock-free containers provide specific thread safety guarantees
- Always check documentation for specific container thread safety

### **Iterator Invalidation**
- Flat containers may invalidate iterators on modification
- Small containers follow vector-like invalidation rules
- Lock-free containers have specific iterator guarantees

### **Memory Management**
- Containers integrate with custom allocators
- RAII principles ensure proper cleanup
- Exception safety guarantees provided

## 📚 Further Reading

- **Boost Container Documentation**: For Boost-specific features
- **Standard Library Reference**: For fallback container behavior
- **Performance Analysis**: Detailed benchmarking methodologies
- **Concurrent Programming**: Lock-free programming principles

---

This example showcases the power and flexibility of the Atom containers module, providing high-performance alternatives to standard containers while maintaining compatibility and ease of use.
