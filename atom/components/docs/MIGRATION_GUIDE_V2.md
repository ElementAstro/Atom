# Component System Migration Guide - Version 2.0

## Overview

This guide helps you migrate from the previous component system to the optimized version 2.0, which includes significant performance improvements, memory optimizations, and new features.

## Breaking Changes

### 1. Performance Statistics Structure

**Before:**

```cpp
const auto& stats = component->getPerformanceStats();
auto totalTime = stats.timing.totalExecutionTime;
```

**After:**

```cpp
const auto& stats = component->getPerformanceStats();
auto totalTime = stats.getTotalExecutionTime();  // Returns microseconds for compatibility
```

**Migration:** The performance statistics now use nanosecond precision internally but provide backward-compatible getter methods.

### 2. Component Memory Layout

**Before:**

```cpp
class MyComponent : public Component {
    // Members in any order
    std::string name_;
    int value_;
    std::vector<float> data_;
};
```

**After:**

```cpp
class alignas(64) MyComponent : public Component {
    // Hot data first (frequently accessed)
    alignas(64) int value_;

    // Cold data later
    std::string name_;
    std::vector<float> data_;

    // Padding for cache alignment if needed
    alignas(64) char padding_[...];
};
```

**Migration:** Consider cache-line alignment for performance-critical components.

## New Features

### 1. Memory Pool Integration

**New Usage:**

```cpp
// Configure memory pool for your component type
PoolConfig config;
config.initialPoolSize = 1000;
config.maxPoolSize = 10000;
config.enableStatistics = true;

// Create components using pools
auto factory = ComponentFactory::instance();
factory.configurePool<MyComponent>(config);
auto component = factory.create<MyComponent>("component_name");
```

### 2. SIMD-Optimized Containers

**New Usage:**

```cpp
// Create SIMD-friendly container
SIMDComponentContainer<MyComponent>::SIMDConfig simdConfig;
simdConfig.batchSize = 64;
simdConfig.enableSIMD = true;

SIMDComponentContainer<MyComponent> container(simdConfig);

// Add components
container.add(component);

// Process in SIMD-optimized batches
container.forEachBatch([](auto& batch) {
    for (auto& comp : batch) {
        comp->batchUpdate();
    }
});
```

### 3. Component Archetypes

**New Usage:**

```cpp
// Define archetype for components that always go together
using GameEntityArchetype = ComponentArchetype<TransformComponent, RenderComponent>;

auto archetype = std::make_shared<GameEntityArchetype>();
size_t entityId = archetype->addEntity(transform, render);

// Query and process entities
archetype->forEach([](auto& transform, auto& render) {
    // Process entity with both components
});
```

### 4. Fast Dispatch for Hot Paths

**New Usage:**

```cpp
// For performance-critical code paths
auto result = component->fastDispatch("hotFunction", args...);

// Regular dispatch for normal use
auto result = component->dispatch("normalFunction", args...);
```

## Performance Optimization Guide

### 1. Component Design Best Practices

```cpp
class OptimizedComponent : public Component {
public:
    // Cache-line aligned for optimal performance
    struct alignas(64) HotData {
        float position[3];
        float velocity[3];
        int32_t id;
        bool active;
    };

    explicit OptimizedComponent(const std::string& name) : Component(name) {
        // Register batch processing interface
        def("batchUpdate", [this]() { batchUpdate(); });
    }

    // Implement batch processing interface
    void batchUpdate() {
        if (hotData_.active) {
            for (int i = 0; i < 3; ++i) {
                hotData_.position[i] += hotData_.velocity[i] * deltaTime_;
            }
        }
    }

    float* getUpdateData() { return hotData_.position; }
    size_t getUpdateDataSize() const { return 3; }

private:
    alignas(64) HotData hotData_;
    float deltaTime_ = 0.016f;

    // Cold data (less frequently accessed)
    std::string description_;
    std::vector<std::string> tags_;
};
```

### 2. Memory Pool Configuration

```cpp
void configureOptimalPools() {
    // High-frequency components (created/destroyed often)
    PoolConfig highFreqConfig;
    highFreqConfig.initialPoolSize = 1000;
    highFreqConfig.maxPoolSize = 10000;
    highFreqConfig.chunkSize = 64;  // Cache-friendly
    highFreqConfig.enableStatistics = true;

    // Low-frequency components (long-lived)
    PoolConfig lowFreqConfig;
    lowFreqConfig.initialPoolSize = 100;
    lowFreqConfig.maxPoolSize = 500;
    lowFreqConfig.chunkSize = 16;

    auto& factory = ComponentFactory::instance();
    factory.configurePool<TransformComponent>(highFreqConfig);
    factory.configurePool<ConfigComponent>(lowFreqConfig);
}
```

### 3. Parallel Processing

```cpp
void processComponentsInParallel() {
    ParallelComponentProcessor::ParallelConfig config;
    config.numThreads = std::thread::hardware_concurrency();
    config.enableWorkStealing = true;
    config.minBatchSize = 32;

    ParallelComponentProcessor processor(config);

    std::vector<MyComponent> components = getComponents();

    processor.processParallel(components, [](MyComponent& comp) {
        comp.update();
    });
}
```

## Migration Checklist

### Phase 1: Basic Migration

- [ ] Update component headers to include new optimized base class
- [ ] Replace direct performance stats access with getter methods
- [ ] Test existing functionality with new system

### Phase 2: Memory Optimization

- [ ] Analyze component memory layouts
- [ ] Add cache-line alignment to performance-critical components
- [ ] Configure memory pools for different component types
- [ ] Measure memory usage improvements

### Phase 3: Performance Optimization

- [ ] Implement batch processing interfaces for suitable components
- [ ] Use SIMD containers for large component collections
- [ ] Replace hot-path dispatch calls with fastDispatch
- [ ] Add parallel processing for CPU-intensive operations

### Phase 4: Advanced Features

- [ ] Implement component archetypes for related components
- [ ] Use query engine for complex component selection
- [ ] Add performance monitoring and profiling
- [ ] Optimize based on benchmark results

## Common Migration Issues

### Issue 1: Compilation Errors with Performance Stats

**Problem:** Code accessing `stats.timing.totalExecutionTime` fails to compile.

**Solution:** Use the new getter methods:

```cpp
// Old
auto time = stats.timing.totalExecutionTime;

// New
auto time = stats.getTotalExecutionTime();
```

### Issue 2: Memory Layout Changes

**Problem:** Component size or alignment has changed.

**Solution:** Review component layout and add explicit alignment:

```cpp
class alignas(64) MyComponent : public Component {
    // Ensure proper alignment
};
```

### Issue 3: Performance Regression

**Problem:** Code is slower after migration.

**Solution:**

1. Configure memory pools appropriately
2. Use batch processing for iteration-heavy code
3. Apply cache-line alignment to hot data structures
4. Use fastDispatch for performance-critical paths

## Testing Your Migration

### 1. Functional Testing

```cpp
void testBasicFunctionality() {
    auto component = std::make_shared<MyComponent>("test");

    // Test initialization
    ASSERT_TRUE(component->initialize());

    // Test command dispatch
    auto result = component->dispatch("testCommand");
    ASSERT_TRUE(result.has_value());

    // Test cleanup
    ASSERT_TRUE(component->destroy());
}
```

### 2. Performance Testing

```cpp
void benchmarkPerformance() {
    ComponentBenchmarkSuite suite;
    auto results = suite.runAllBenchmarks();

    // Verify performance improvements
    for (const auto& result : results) {
        std::cout << result.name << ": "
                  << result.operationsPerSecond << " ops/sec\n";
    }
}
```

### 3. Memory Usage Testing

```cpp
void testMemoryUsage() {
    auto& factory = ComponentFactory::instance();

    // Create many components
    std::vector<std::shared_ptr<MyComponent>> components;
    for (int i = 0; i < 1000; ++i) {
        components.push_back(factory.create<MyComponent>("test_" + std::to_string(i)));
    }

    // Check pool statistics
    const auto& stats = factory.getPoolStatistics<MyComponent>();
    std::cout << "Hit ratio: " << stats.getHitRatio() * 100 << "%\n";
    std::cout << "Memory usage: " << factory.getTotalMemoryUsage() / 1024 << " KB\n";
}
```

## Support and Resources

- **Examples**: See `atom/components/examples/optimized_usage_examples.cpp`
- **Benchmarks**: Use `atom/components/benchmarks/component_benchmarks.hpp`
- **Performance Guide**: Read `atom/components/docs/PERFORMANCE_GUIDE.md`
- **API Reference**: Check `atom/components/docs/API_REFERENCE.md`

For questions or issues, please refer to the project documentation or create an issue in the repository.
