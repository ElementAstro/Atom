# Enhanced Component System - Migration Guide

## Overview

This guide helps you migrate from the original Atom component system to the enhanced version with advanced features like memory pooling, scripting integration, and comprehensive lifecycle management.

## Breaking Changes

### 1. Header File Changes

**Old:**

```cpp
#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"
```

**New:**

```cpp
#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"
// Optional new features
#include "atom/components/component_pool.hpp"
#include "atom/components/scripting_api.hpp"
#include "atom/components/script_sandbox.hpp"
```

### 2. Component Creation

**Old:**

```cpp
auto component = std::make_shared<Component>("MyComponent");
registry.addComponent("MyComponent", component);
```

**New:**

```cpp
// Method 1: Direct creation (compatible)
auto component = std::make_shared<Component>("MyComponent");
registry.registerComponent("MyComponent", component);

// Method 2: Factory creation (recommended)
auto component = registry.createComponent<Component>("MyComponent");

// Method 3: Memory pool creation (high performance)
ComponentPool<Component> pool(config);
auto component = pool.allocate("MyComponent");
```

### 3. Variable Access

**Old:**

```cpp
component->setVariable("health", 100);
auto health = component->getVariable("health");
```

**New:**

```cpp
// Compatible API (no changes needed)
component->setVar("health", 100);
auto health = component->getVar("health");

// Enhanced type safety
component->setVar("health", Var(100));
if (auto health = component->getVar("health")) {
    int healthValue = health->get<int>();
}
```

## New Features Integration

### 1. Memory Pooling

Add memory pooling for improved performance:

```cpp
// Before: Standard allocation
class GameSystem {
    std::vector<std::shared_ptr<Component>> components_;

    void createComponent(const std::string& name) {
        components_.push_back(std::make_shared<Component>(name));
    }
};

// After: Memory pool allocation
class GameSystem {
    std::unique_ptr<ComponentPool<Component>> pool_;
    std::vector<std::shared_ptr<Component>> components_;

    GameSystem() {
        PoolConfig config;
        config.initialPoolSize = 64;
        config.maxPoolSize = 256;
        pool_ = std::make_unique<ComponentPool<Component>>(config);
    }

    void createComponent(const std::string& name) {
        components_.push_back(pool_->allocate(name));
    }
};
```

### 2. Lifecycle Management

Add dependency management and lifecycle hooks:

```cpp
// Before: Manual initialization order
void initializeComponents() {
    auto renderer = createComponent("Renderer");
    auto physics = createComponent("Physics");
    auto game = createComponent("Game");

    // Manual initialization
    renderer->initialize();
    physics->initialize();
    game->initialize();
}

// After: Automatic dependency resolution
void initializeComponents() {
    auto& lifecycle = LifecycleManager::instance();

    // Define dependencies
    DependencyConstraint rendererDep("Renderer", DependencyType::Required);
    DependencyConstraint physicsDep("Physics", DependencyType::Required);
    lifecycle.addDependency("Game", rendererDep);
    lifecycle.addDependency("Game", physicsDep);

    // Register hooks
    lifecycle.registerHook("Renderer", LifecyclePhase::PostConstruction,
        [](Component& comp, LifecyclePhase phase) {
            // Initialization code
        });

    // Automatic initialization in correct order
    auto initOrder = lifecycle.resolveDependencies("Game");
    for (const auto& componentName : initOrder) {
        auto component = registry.getComponent(componentName);
        lifecycle.executePhase(*component, LifecyclePhase::PostConstruction);
    }
}
```

### 3. Serialization

Replace custom serialization with the new system:

```cpp
// Before: Custom serialization
void saveComponent(const Component& component, const std::string& filename) {
    std::ofstream file(filename);
    file << "name=" << component.getName() << std::endl;
    // Manual serialization...
}

// After: Automatic serialization
void saveComponent(const Component& component, const std::string& filename) {
    auto& serializer = SerializationManager::instance();

    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    options.includeMetadata = true;

    serializer.serializeToFile(component, filename, options);
}

void loadComponent(const std::string& filename) {
    auto& serializer = SerializationManager::instance();

    SerializationOptions options;
    options.format = SerializationFormat::JSON;

    auto result = serializer.deserializeFromFile(filename, options);
    if (result.success) {
        auto component = result.component;
        // Use loaded component
    }
}
```

### 4. Scripting Integration

Add scripting capabilities:

```cpp
// Before: No scripting support
class GameLogic {
    void updatePlayer(Component& player) {
        // Hardcoded C++ logic
        auto health = player.getVar("health");
        if (health && health->get<int>() <= 0) {
            player.setState(ComponentState::Inactive);
        }
    }
};

// After: Script-driven logic
class GameLogic {
    std::unique_ptr<IScriptEngine> scriptEngine_;

    GameLogic() {
        auto& scriptingAPI = ComponentScriptingAPI::instance();
        scriptingAPI.initialize({});

        scriptEngine_ = scriptingAPI.createEngine(ScriptLanguage::Lua);

        // Load game logic script
        scriptEngine_->executeFile("game_logic.lua");
    }

    void updatePlayer(Component& player) {
        // Call script function
        std::vector<ScriptValue> args = {ScriptValue(player.getName())};
        auto result = scriptEngine_->callFunction("updatePlayer", args);
    }
};
```

## Performance Optimizations

### 1. Batch Processing

Replace individual component processing with batch operations:

```cpp
// Before: Individual processing
void updateComponents(std::vector<Component>& components) {
    for (auto& component : components) {
        // Process each component individually
        updateComponent(component);
    }
}

// After: Batch processing
void updateComponents(std::vector<Component>& components) {
    ComponentBatchProcessor::Config config;
    config.batchSize = 64;
    config.enableSIMD = true;
    config.enablePrefetch = true;

    ComponentBatchProcessor processor(config);

    processor.processBatches(components.data(), components.size(),
        [](Component* batch, size_t count) {
            // Process batch of components efficiently
            for (size_t i = 0; i < count; ++i) {
                updateComponent(batch[i]);
            }
        });
}
```

### 2. Cache-Friendly Data Layout

Optimize data layout for better cache performance:

```cpp
// Before: Pointer-based storage
class ComponentManager {
    std::vector<std::shared_ptr<Component>> components_;
};

// After: Cache-friendly storage with memory pools
class ComponentManager {
    ComponentPool<Component> pool_;
    std::vector<Component*> activeComponents_;  // Raw pointers for cache efficiency

    void update() {
        // Components are allocated contiguously in memory pool
        // Better cache locality during iteration
        for (auto* component : activeComponents_) {
            component->update();
        }
    }
};
```

## Build System Changes

### CMake Configuration

Update your CMakeLists.txt to enable new features:

```cmake
# Before: Basic component system
find_package(atom_component REQUIRED)
target_link_libraries(your_target atom_component)

# After: Enhanced component system with optional features
find_package(atom_component REQUIRED)

# Enable scripting support (optional)
option(ENABLE_LUA_SCRIPTING "Enable Lua scripting" ON)
option(ENABLE_PYTHON_SCRIPTING "Enable Python scripting" ON)

if(ENABLE_LUA_SCRIPTING)
    target_compile_definitions(your_target PRIVATE ATOM_ENABLE_LUA=1)
    find_package(Lua REQUIRED)
    target_link_libraries(your_target ${LUA_LIBRARIES})
endif()

if(ENABLE_PYTHON_SCRIPTING)
    target_compile_definitions(your_target PRIVATE ATOM_ENABLE_PYTHON=1)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    target_link_libraries(your_target ${Python3_LIBRARIES})
endif()

target_link_libraries(your_target atom_component)
```

## Testing Migration

### 1. Compatibility Testing

Ensure your existing code still works:

```cpp
// Create compatibility test
void testBackwardCompatibility() {
    // Test original API still works
    auto& registry = Registry::instance();
    auto component = std::make_shared<Component>("TestComponent");

    // Original methods should still work
    component->setVar("test", 42);
    auto value = component->getVar("test");
    assert(value && value->get<int>() == 42);

    std::cout << "Backward compatibility: PASSED" << std::endl;
}
```

### 2. Performance Testing

Compare performance before and after migration:

```cpp
void performanceComparison() {
    const int componentCount = 10000;
    const int iterations = 100;

    // Test original allocation
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::vector<std::shared_ptr<Component>> components;
        for (int j = 0; j < componentCount; ++j) {
            components.push_back(std::make_shared<Component>("Test"));
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto originalTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Test pool allocation
    ComponentPool<Component> pool({});
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::vector<std::shared_ptr<Component>> components;
        for (int j = 0; j < componentCount; ++j) {
            components.push_back(pool.allocate("Test"));
        }
    }
    end = std::chrono::high_resolution_clock::now();
    auto poolTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Original: " << originalTime.count() << " μs" << std::endl;
    std::cout << "Pool: " << poolTime.count() << " μs" << std::endl;
    std::cout << "Improvement: " << (double)originalTime.count() / poolTime.count() << "x" << std::endl;
}
```

## Common Migration Issues

### 1. Header Include Order

**Problem:** Compilation errors due to missing includes.

**Solution:** Include headers in the correct order:

```cpp
// Core headers first
#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"

// Feature headers second
#include "atom/components/component_pool.hpp"
#include "atom/components/lifecycle.hpp"
#include "atom/components/serialization.hpp"

// Scripting headers last (if enabled)
#if ATOM_ENABLE_LUA
#include "atom/components/lua_engine.hpp"
#endif

#if ATOM_ENABLE_PYTHON
#include "atom/components/python_engine.hpp"
#endif
```

### 2. Linking Issues

**Problem:** Undefined symbols when using new features.

**Solution:** Ensure all required libraries are linked:

```cmake
target_link_libraries(your_target
    atom_component
    ${LUA_LIBRARIES}        # If using Lua
    ${Python3_LIBRARIES}    # If using Python
    Threads::Threads        # For threading support
)
```

### 3. Runtime Configuration

**Problem:** Features not working at runtime.

**Solution:** Proper initialization:

```cpp
int main() {
    // Initialize scripting API if using scripts
    auto& scriptingAPI = ComponentScriptingAPI::instance();
    ScriptEngineConfig config;
    config.memoryLimit = 64 * 1024 * 1024;  // 64MB
    scriptingAPI.initialize(config);

    // Your application code

    // Cleanup
    scriptingAPI.shutdown();
    return 0;
}
```

## Gradual Migration Strategy

1. **Phase 1:** Update build system and test compatibility
2. **Phase 2:** Replace component creation with factory methods
3. **Phase 3:** Add memory pooling for performance-critical components
4. **Phase 4:** Implement lifecycle management for complex dependencies
5. **Phase 5:** Add serialization for persistent components
6. **Phase 6:** Integrate scripting for flexible game logic
7. **Phase 7:** Optimize with batch processing and SIMD

Each phase can be implemented independently, allowing for gradual migration without breaking existing functionality.
