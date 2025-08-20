# Enhanced Component System

A high-performance, feature-rich component system with advanced memory management, scripting integration, and comprehensive lifecycle management.

## Features

### 🚀 **Core Enhancements**

- **Memory Pooling**: Specialized memory pools for efficient component allocation
- **Advanced Lifecycle Management**: Sophisticated dependency resolution and lifecycle hooks
- **Cache-Friendly Iteration**: SIMD-optimized component processing with prefetching
- **Comprehensive Serialization**: Multi-format support (JSON, Binary, XML, MessagePack)
- **Scripting Integration**: Lua and Python scripting with sandbox security
- **Hot-Reloading**: Real-time script reloading for development

### 🔧 **Build Configuration**

#### CMake Options

```bash
# Enable Lua scripting support
cmake -DATOM_ENABLE_LUA=ON ..

# Enable Python scripting support
cmake -DATOM_ENABLE_PYTHON=ON ..

# Enable both scripting engines
cmake -DATOM_ENABLE_LUA=ON -DATOM_ENABLE_PYTHON=ON ..
```

#### Dependencies

- **Lua Support**: Requires Lua 5.4+ or LuaJIT
- **Python Support**: Requires Python 3.8+ with development headers
- **Testing**: Requires Google Test framework

## Quick Start

### Basic Component Usage

```cpp
#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"

// Create and register a component
auto& registry = atom::components::Registry::instance();
auto component = registry.createComponent<Component>("MyComponent");

// Set variables
component->setVar("value", 42);
component->setVar("name", std::string("Hello World"));

// Get variables
auto value = component->getVar("value");
if (value) {
    std::cout << "Value: " << value->toString() << std::endl;
}
```

### Memory Pool Usage

```cpp
#include "atom/components/component_pool.hpp"

// Configure memory pool
atom::components::PoolConfig config;
config.initialPoolSize = 64;
config.maxPoolSize = 256;
config.chunkSize = 16;
config.enableStatistics = true;

// Create pool
auto pool = std::make_unique<ComponentPool<Component>>(config);

// Allocate components
auto component1 = pool->allocate("Component1");
auto component2 = pool->allocate("Component2");

// Components are automatically returned to pool when destroyed
```

### Lifecycle Management

```cpp
#include "atom/components/lifecycle.hpp"

auto& lifecycle = atom::components::LifecycleManager::instance();

// Register lifecycle hooks
lifecycle.registerHook("MyComponent", LifecyclePhase::PostConstruction,
    [](Component& comp, LifecyclePhase phase) {
        std::cout << "Component " << comp.getName() << " constructed" << std::endl;
    });

// Add dependencies
DependencyConstraint dep("BaseComponent", DependencyType::Required);
lifecycle.addDependency("MyComponent", dep);

// Resolve dependency order
auto order = lifecycle.resolveDependencies("MyComponent");
```

### Serialization

```cpp
#include "atom/components/serialization.hpp"

auto& serializer = atom::components::SerializationManager::instance();

// JSON serialization
SerializationOptions options;
options.format = SerializationFormat::JSON;
options.includeMetadata = true;

auto result = serializer.serialize(*component, options);
if (result.success) {
    // Save to file
    serializer.serializeToFile(*component, "component.json", options);
}

// Deserialization
auto deserializeResult = serializer.deserializeFromFile("component.json", options);
if (deserializeResult.success) {
    auto loadedComponent = deserializeResult.component;
}
```

## Scripting Integration

### Lua Scripting (when ATOM_ENABLE_LUA=ON)

```cpp
#include "atom/components/lua_engine.hpp"

// Create Lua engine
LuaConfig config;
config.enableJIT = true;
config.memoryLimit = 64 * 1024 * 1024;  // 64MB

auto engine = LuaEngineFactory::create(config);
engine->initialize({});

// Execute Lua script
std::string script = R"(
    function greet(name)
        return "Hello, " .. name .. "!"
    end

    return greet("World")
)";

auto result = engine->executeScript(script);
if (result.success) {
    std::cout << "Result: " << result.returnValue.get<std::string>() << std::endl;
}

// Register C++ functions for Lua
engine->registerFunction("createComponent", [](const std::vector<ScriptValue>& args) {
    // Implementation
    return ScriptValue(true);
});
```

### Python Scripting (when ATOM_ENABLE_PYTHON=ON)

```cpp
#include "atom/components/python_engine.hpp"

// Create Python engine
PythonConfig config;
config.enableSitePackages = false;
config.isolatedMode = true;

auto engine = PythonEngineFactory::create(config);
engine->initialize({});

// Execute Python script
std::string script = R"(
def calculate(x, y):
    return x * y + 10

result = calculate(5, 3)
)";

auto result = engine->executeScript(script);
if (result.success) {
    auto value = engine->getGlobal("result");
    if (value) {
        std::cout << "Result: " << value->get<int64_t>() << std::endl;
    }
}
```

### Sandboxed Execution

```cpp
#include "atom/components/script_sandbox.hpp"

// Configure sandbox
SandboxConfig sandboxConfig;
sandboxConfig.permissions = Permission::ComponentAccess | Permission::MemoryAccess;
sandboxConfig.limits.maxMemoryUsage = 32 * 1024 * 1024;  // 32MB
sandboxConfig.limits.maxExecutionTime = std::chrono::seconds(10);

auto sandbox = std::make_unique<ScriptSandbox>(sandboxConfig);
sandbox->initialize();

// Execute script in sandbox
auto result = sandbox->executeInSandbox(script, "test_script", false, ScriptLanguage::Lua);
```

## Performance Optimization

### Cache-Friendly Iteration

```cpp
#include "atom/components/iteration.hpp"

// Configure batch processor
ComponentBatchProcessor::Config config;
config.batchSize = 64;
config.enableSIMD = true;
config.enablePrefetch = true;

ComponentBatchProcessor processor(config);

// Process components in batches
std::vector<Component> components(1000);
processor.processBatches(components.data(), components.size(),
    [](Component* batch, size_t count) {
        // Process batch of components
        for (size_t i = 0; i < count; ++i) {
            // Process batch[i]
        }
    });
```

### SIMD Operations

```cpp
// SIMD-optimized operations (when AVX2 is available)
std::vector<float> data(1000, 1.0f);
processor.simdBatchUpdate(data.data(), data.size(),
    [](float x) { return x * 2.0f + 1.0f; });
```

## Testing

### Running Tests

```bash
# Build with testing enabled
mkdir build && cd build
cmake -DATOM_ENABLE_LUA=ON -DATOM_ENABLE_PYTHON=ON ..
make

# Run all tests
make run_tests

# Run specific test categories
make run_benchmarks
make run_scripting_tests

# Run with memory checking (Linux)
make run_tests_valgrind

# Generate coverage report (GCC)
make coverage
```

### Test Categories

- **Memory Pool Tests**: Allocation/deallocation performance and correctness
- **Lifecycle Tests**: Dependency resolution and hook execution
- **Serialization Tests**: Multi-format serialization/deserialization
- **Scripting Tests**: Lua and Python engine integration
- **Integration Tests**: End-to-end workflow testing
- **Performance Benchmarks**: Performance measurement and regression testing

## Architecture

### Memory Management

- **Component Pools**: Specialized allocators for different component types
- **Cache-Line Alignment**: 64-byte alignment for optimal cache performance
- **Chunk-Based Allocation**: Spatial locality optimization
- **Automatic Defragmentation**: Background memory compaction

### Scripting Architecture

- **Unified API**: Common interface for all scripting engines
- **Type-Safe Conversion**: Automatic C++/Script type conversion
- **Sandbox Security**: Resource limits and permission system
- **Hot-Reloading**: File watching and automatic script reloading

### Serialization System

- **Multi-Format Support**: JSON, Binary, XML, MessagePack
- **Schema Validation**: Version checking and field validation
- **Compression**: Optional data compression
- **Metadata**: Timestamps, versions, and custom metadata

## Best Practices

### Performance

1. Use memory pools for frequently allocated components
2. Enable SIMD optimizations for batch processing
3. Use cache-friendly iteration patterns
4. Profile with the built-in performance monitoring

### Security

1. Always use sandboxed execution for untrusted scripts
2. Set appropriate resource limits
3. Use permission system to restrict script capabilities
4. Validate all script inputs

### Development

1. Use hot-reloading for rapid script development
2. Enable debug information in development builds
3. Use comprehensive test suite for validation
4. Monitor memory usage and performance metrics

## Documentation

- **[API Reference](docs/API_REFERENCE.md)**: Complete API documentation
- **[Usage Examples](docs/EXAMPLES.md)**: Comprehensive usage examples
- **[Migration Guide](docs/MIGRATION_GUIDE.md)**: Guide for upgrading from the original system
- **[Performance Guide](docs/PERFORMANCE_GUIDE.md)**: Performance optimization techniques

## Contributing

1. Follow the existing code style and patterns
2. Add tests for new features
3. Update documentation
4. Ensure backward compatibility
5. Test with both Lua and Python engines enabled

## License

Copyright (C) 2023-2024 Max Qian <lightapt.com>

This project is part of the Atom component system.
