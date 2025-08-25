# Memory Management Examples

This directory contains comprehensive examples demonstrating the various memory management components provided by the Atom library. Each example showcases different aspects of efficient memory management in C++.

## Overview

The Atom memory management library provides several high-performance components:

- **MemoryPool**: High-performance memory pool with advanced features like tagged allocations, block size strategies, and statistics
- **Fixed-size MemoryPool**: Thread-safe fixed-size memory pool for consistent allocation sizes
- **ObjectPool**: Thread-safe object pool for managing reusable objects with validation and statistics
- **RingBuffer**: Lock-free circular buffer for high-performance data streaming
- **SharedMemory**: Cross-process shared memory communication with synchronization
- **MemoryTracker**: Advanced memory leak detection and allocation tracking
- **Arena & ShortAlloc**: Stack-based memory allocation for temporary objects
- **Utilities**: Helper functions and type traits for memory management

## Examples

### Core Components

#### 1. `memory.cpp` - Advanced MemoryPool

Demonstrates the main MemoryPool class with:

- Basic memory allocation and deallocation
- Tagged allocations for memory tracking
- Custom block size strategies (exponential vs linear growth)
- Memory compaction and fragmentation analysis
- Integration with PMR (Polymorphic Memory Resources)
- Performance comparison with standard allocators
- Thread safety in concurrent environments
- Memory reservation for performance optimization

#### 2. `memory_pool.cpp` - Fixed-size MemoryPool

Shows the fixed-size memory pool implementation:

- Basic block allocation and deallocation
- Object pool usage with automatic construction/destruction
- Performance comparison with standard malloc/free
- Thread-safe operations
- Memory statistics and monitoring

#### 3. `object.cpp` - ObjectPool Management

Comprehensive object pool examples covering:

- Basic object acquisition and release
- Timeouts and priority-based allocation
- Object validation on acquire/release
- Batch operations for multiple objects
- Auto-cleanup and pool management
- Statistics and monitoring
- Performance comparisons
- Concurrent access patterns
- Custom object creation strategies
- Batch actions on all pooled objects

#### 4. `ring.cpp` - RingBuffer Operations

Demonstrates circular buffer functionality:

- Basic push/pop operations
- Push with overwrite for full buffers
- Advanced inspection (front, back, at, contains)
- Iterator support and range-based loops
- Buffer manipulation (clear, resize, rotate)
- Higher-order functions (forEach, removeIf)
- Complex types and custom classes
- Thread safety and concurrent access
- Performance demonstrations
- Real-world logging system example

#### 5. `shared.cpp` - Shared Memory Communication

Shows inter-process communication using shared memory:

- Basic creation and usage
- Opening existing shared memory
- Error handling and timeouts
- Partial read/write operations
- Binary data with span interface
- Resize functionality
- Asynchronous operations
- Change notifications and callbacks
- Multi-threaded producer-consumer patterns
- Advanced functions with withLock

### Memory Management Tools

#### 6. `tracker.cpp` - Memory Leak Detection

Advanced memory tracking capabilities:

- Basic initialization and configuration
- Manual tracking of memory allocations
- Automatic tracking with overloaded operators
- Memory leak detection and reporting
- Multi-threaded memory tracking
- Generating detailed memory reports
- Testing edge cases and error conditions
- Advanced error handling
- Performance impact assessment
- Integration with real-world scenarios

#### 7. `short_alloc.cpp` - Arena-based Allocation

Stack-based memory allocation examples:

- Basic arena creation and usage
- ShortAlloc with STL containers
- Performance comparisons with standard allocators
- Memory usage analysis
- Advanced usage patterns with complex data structures
- Thread safety considerations
- Performance testing and benchmarking

#### 8. `utils.cpp` - Memory Utilities

Helper functions and utilities:

- Basic memory configuration
- Type trait utilities for compile-time validation
- Smart pointer creation (makeShared, makeUnique)
- Custom deleters and RAII patterns
- Memory alignment utilities
- Thread-safe singleton pattern
- Weak pointer utilities
- Combined usage of multiple utilities

### Integration Examples

#### 9. `integration.cpp` - Component Integration

Real-world scenarios combining multiple components:

- High-performance network packet processing system
- Memory-efficient data cache with tracking
- Shared memory communication with object pooling
- Comprehensive memory tracking across all components

## Building and Running

All examples are automatically built when you build the Atom project. Each example creates a separate executable:

```bash
# Build all examples
mkdir build && cd build
cmake ..
make

# Run individual examples
./memory_memory          # Advanced MemoryPool example
./memory_memory_pool     # Fixed-size MemoryPool example
./memory_object          # ObjectPool example
./memory_ring            # RingBuffer example
./memory_shared          # SharedMemory example
./memory_tracker         # MemoryTracker example
./memory_short_alloc     # Arena/ShortAlloc example
./memory_utils           # Memory utilities example
./memory_integration     # Integration example
```

## Key Features Demonstrated

### Performance Optimizations

- Memory pool allocation vs standard malloc/free
- Object reuse to reduce allocation overhead
- Lock-free data structures for high throughput
- Memory reservation for predictable performance
- Custom allocators for specific use cases

### Memory Safety

- Automatic leak detection and reporting
- Tagged allocations for tracking
- RAII patterns for automatic cleanup
- Validation of objects before use
- Thread-safe operations

### Advanced Features

- Cross-process shared memory communication
- Polymorphic Memory Resource (PMR) integration
- Custom block size strategies
- Memory compaction and defragmentation
- Asynchronous operations
- Change notifications

### Real-world Applications

- Network packet processing
- Logging systems
- Data caching
- Inter-process communication
- High-frequency trading systems
- Game engines
- Embedded systems

## Best Practices

1. **Choose the Right Tool**: Use MemoryPool for frequent same-size allocations, ObjectPool for object reuse, RingBuffer for streaming data
2. **Monitor Performance**: Use the built-in statistics and tracking features to optimize memory usage
3. **Handle Errors**: Always check return values and handle exceptions appropriately
4. **Thread Safety**: Use the thread-safe variants when working in multi-threaded environments
5. **Memory Tracking**: Enable memory tracking during development to catch leaks early
6. **Resource Management**: Use RAII patterns and smart pointers for automatic cleanup

## Contributing

When adding new examples:

1. Follow the existing naming convention
2. Include comprehensive comments explaining the concepts
3. Demonstrate both basic and advanced usage
4. Include error handling and edge cases
5. Add performance comparisons where relevant
6. Update this README with the new example description
