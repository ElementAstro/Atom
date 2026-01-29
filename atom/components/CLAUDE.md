# atom/components - Component System Module

> **Module Version:** 1.0.0
> **Documentation Version:** 1.0.0
> **Last Updated:** 2025-01-15

---

## Navigation

[Root Directory](../../CLAUDE.md) > **components**

---

## Module Overview

The **atom::components** module provides a flexible component-based architecture for building modular applications. It offers features similar to Entity-Component-System (ECS) patterns, with support for component lifecycle management, scripting integration, and dynamic composition.

### Key Features

- **Component Registry**: Central registry for component types and instances
- **Component Pools**: Efficient pooling for component instances
- **Lifecycle Management**: Initialization, update, and shutdown phases
- **Scripting Integration**: Lua and Python scripting support (optional)
- **Type System**: Variant-based property system (Var)
- **Serialization**: JSON-based component serialization
- **Dispatch System**: Event and message dispatching

---

## Directory Structure

```
atom/components/
├── core/              # Core component system
│   ├── component.hpp
│   ├── component.cpp
│   ├── component_pool.hpp
│   ├── component_pool.cpp
│   ├── registry.hpp
│   ├── registry.cpp
│   ├── types.hpp
│   ├── module_macro.hpp
│   └── package.hpp
├── scripting/         # Scripting integration
│   ├── script_engine.hpp
│   ├── script_engine.cpp
│   ├── script_sandbox.hpp
│   ├── script_sandbox.cpp
│   ├── scripting_api.hpp
│   ├── scripting_api.cpp
│   ├── advanced_bindings.hpp
│   ├── advanced_bindings.cpp
│   ├── lua_engine.hpp
│   ├── lua_engine.cpp
│   └── python_engine.hpp
│   └── python_engine.cpp
├── lifecycle/         # Lifecycle management
│   ├── lifecycle.hpp
│   ├── lifecycle.cpp
│   ├── dispatch.hpp
│   ├── dispatch.cpp
│   └── iteration.hpp
│   └── iteration.cpp
└── data/              # Data and serialization
    ├── var.hpp
    ├── var.cpp
    ├── serialization.hpp
    ├── serialization.cpp
    └── type_conversion.hpp
```

---

## Core Components

### Component Base Class

```cpp
#include "atom/components/core/component.hpp"

using namespace atom::components;

// Define a component
class MyComponent : public Component {
public:
    // Called when component is created
    void onInitialize() override {
        ATOM_INFO("MyComponent initialized");
    }

    // Called each frame/update
    void onUpdate(float deltaTime) override {
        // Update logic here
    }

    // Called when component is destroyed
    void onShutdown() override {
        ATOM_INFO("MyComponent shut down");
    }

    ATOM_COMPONENT(MyComponent, "MyComponent")
};
```

### Component Registry

```cpp
#include "atom/components/core/registry.hpp"

using namespace atom::components;

// Get global registry
auto& registry = Registry::getInstance();

// Register component type
registry.registerComponentType<MyComponent>();

// Create component instance
auto* comp = registry.createComponent<MyComponent>("myComponent");

// Find component by name
auto* found = registry.findComponent("myComponent");

// Destroy component
registry.destroyComponent(found);
```

### Component Pool

```cpp
#include "atom/components/core/component_pool.hpp"

using namespace atom::components;

// Create a pool for a component type
ComponentPool<MyComponent> pool(100);  // Pre-allocate 100

// Acquire component from pool
auto* comp = pool.acquire();
comp->onInitialize();

// Return component to pool
pool.release(comp);
```

---

## Scripting Integration

### Lua Scripting (Optional)

```cpp
#include "atom/components/scripting/lua_engine.hpp"

using namespace atom::components;

// Create Lua engine
LuaEngine engine;

// Register component type
engine.registerComponentType<MyComponent>("MyComponent");

// Execute Lua script
engine.executeScript(R"(
    local comp = createComponent("MyComponent", "luaComponent")
    comp:setProperty("value", 42)
)");

// Call Lua function
auto result = engine.callFunction("getData", 123);
```

### Python Scripting (Optional)

```cpp
#include "atom/components/scripting/python_engine.hpp"

using namespace atom::components;

// Create Python engine
PythonEngine engine;

// Register component type
engine.registerComponentType<MyComponent>("MyComponent");

// Execute Python script
engine.executeScript(R"(
    comp = create_component("MyComponent", "pyComponent")
    comp.set_property("value", 42)
)");
```

### Script Sandbox

```cpp
#include "atom/components/scripting/script_sandbox.hpp"

using namespace atom::components;

// Create sandboxed environment
ScriptSandbox sandbox;

// Set resource limits
sandbox.setMemoryLimit(1024 * 1024);  // 1MB
sandbox.setTimeout(5000);  // 5 seconds

// Execute untrusted code
auto result = sandbox.execute("return 1 + 1");
if (result) {
    std::cout << "Result: " << result->as<int>() << "\n";
}
```

---

## Lifecycle Management

### Lifecycle Phases

```cpp
#include "atom/components/lifecycle/lifecycle.hpp"

using namespace atom::components;

// Create lifecycle manager
LifecycleManager lifecycle;

// Register components
lifecycle.registerComponent(comp1);
lifecycle.registerComponent(comp2);

// Run lifecycle phases
lifecycle.initializeAll();  // Call onInitialize()
lifecycle.updateAll(0.016f); // Call onUpdate() with delta time
lifecycle.shutdownAll();   // Call onShutdown()
```

### Dispatch System

```cpp
#include "atom/components/lifecycle/dispatch.hpp"

using namespace atom::components;

// Create dispatcher
Dispatcher dispatcher;

// Subscribe to events
dispatcher.subscribe("UpdateEvent", [](const Event& event) {
    float delta = event.getData<float>("deltaTime");
    // Handle update
});

// Dispatch events
Event event("UpdateEvent");
event.setData("deltaTime", 0.016f);
dispatcher.dispatch(event);
```

---

## Data System

### Var (Variant Type)

```cpp
#include "atom/components/data/var.hpp"

using namespace atom::components;

// Create variants
Var v1 = 42;
Var v2 = "Hello";
Var v3 = 3.14;
Var v4 = std::vector<int>{1, 2, 3};

// Access values
if (v1.is<int>()) {
    std::cout << v1.as<int>() << "\n";
}

// Type-safe access
try {
    int value = v1.to<int>();
} catch (const VarError& e) {
    std::cerr << "Type mismatch: " << e.what() << "\n";
}

// Variant operations
Var result = v1 + v2;  // Concatenation or addition based on types
```

### Serialization

```cpp
#include "atom/components/data/serialization.hpp"

using namespace atom::components;

// Serialize component to JSON
json j = component->serialize();
std::string jsonString = j.dump();

// Deserialize from JSON
Component* comp = registry.createComponent<MyComponent>("comp");
comp->deserialize(jsonString);
```

---

## Public Interfaces

### Component Class

```cpp
class Component {
public:
    virtual ~Component() = default;

    // Lifecycle hooks
    virtual void onInitialize();
    virtual void onUpdate(float deltaTime);
    virtual void onShutdown();

    // Identification
    [[nodiscard]] virtual std::string getTypeName() const = 0;
    [[nodiscard]] std::string getName() const;
    void setName(const std::string& name);

    // State
    [[nodiscard]] bool isActive() const;
    void setActive(bool active);

    // Properties
    template <typename T>
    void setProperty(const std::string& name, const T& value);

    template <typename T>
    [[nodiscard]] T getProperty(const std::string& name) const;
};
```

### Registry Class

```cpp
class Registry {
public:
    static Registry& getInstance();

    // Type registration
    template <typename T>
    void registerComponentType();

    // Instance management
    template <typename T>
    T* createComponent(const std::string& name);

    void destroyComponent(Component* component);
    Component* findComponent(const std::string& name);

    // Query
    template <typename T>
    std::vector<T*> getComponentsOfType();

    std::vector<Component*> getAllComponents();
};
```

### ComponentPool Class

```cpp
template <typename T>
class ComponentPool {
public:
    explicit ComponentPool(size_t initialCapacity = 100);

    T* acquire();
    void release(T* component);

    size_t getActiveCount() const;
    size_t getTotalCapacity() const;
    void reserve(size_t capacity);
};
```

### ScriptEngine Class

```cpp
class ScriptEngine {
public:
    virtual ~ScriptEngine() = default;

    // Component registration
    template <typename T>
    void registerComponentType(const std::string& name);

    // Script execution
    void executeScript(const std::string& script);
    Var executeFunction(const std::string& name,
                       const std::vector<Var>& args);

    // Error handling
    bool hasError() const;
    std::string getErrorMessage() const;
};
```

---

## Dependencies

### Required Dependencies

- **atom::error**: Error handling framework
- **atom::type**: Type utilities
- **fmt**: Enhanced formatting
- **spdlog**: Logging framework

### Optional Dependencies

| Feature | Dependency | CMake Option |
|---------|------------|--------------|
| Lua scripting | Lua 5.3+ | `ATOM_ENABLE_LUA=ON` |
| Python scripting | Python 3 | `ATOM_ENABLE_PYTHON=ON` |

### Platform-Specific Notes

- Windows: No additional dependencies
- Linux: May need `liblua5.3-dev` or `python3-dev`
- macOS: Lua via Homebrew, Python via Xcode

---

## Build Configuration

### CMake Options

```cmake
# Build components module
-DBUILD_COMPONENTS=ON

# Enable Lua scripting
-ATOM_ENABLE_LUA=ON

# Enable Python scripting
-ATOM_ENABLE_PYTHON=ON
```

### Module Configuration

The components module is built as a shared library to support dynamic scripting integration.

---

## Usage Examples

### Basic Component

```cpp
#include "atom/components/core/component.hpp"
#include "atom/components/core/registry.hpp"

using namespace atom::components;

class PlayerController : public Component {
public:
    void onUpdate(float deltaTime) override {
        // Update player logic
        position += velocity * deltaTime;
    }

    Vec3 position{0, 0, 0};
    Vec3 velocity{0, 0, 0};

    ATOM_COMPONENT(PlayerController, "PlayerController")
};

// Usage
auto& registry = Registry::getInstance();
auto* player = registry.createComponent<PlayerController>("player");
player->position = Vec3{10, 0, 0};
```

### Component Pooling

```cpp
#include "atom/components/core/component_pool.hpp"

using namespace atom::components;

// Create pool for Bullet components
ComponentPool<Bullet> bulletPool(1000);

// Spawn bullet
auto* bullet = bulletPool.acquire();
bullet->position = spawnPosition;
bullet->velocity = direction * speed;

// Later, when bullet is destroyed
bulletPool.release(bullet);
```

### Event-Driven Components

```cpp
#include "atom/components/lifecycle/dispatch.hpp"

class HealthComponent : public Component {
public:
    void onInitialize() override {
        // Subscribe to damage events
        Dispatcher::getInstance().subscribe("DamageEvent",
            [this](const Event& e) {
                float damage = e.getData<float>("damage");
                health -= damage;
                if (health <= 0) {
                    Dispatcher::getInstance().dispatch(
                        Event("DeathEvent").setData("entity", getName())
                    );
                }
            }
        );
    }

    float health{100.0f};
};
```

---

## Testing

### Test Organization

Tests are located in `tests/components/`:

- `test_component.cpp`: Component base class tests
- `test_registry.cpp`: Registry tests
- `test_pool.cpp`: Component pool tests
- `test_lifecycle.cpp`: Lifecycle tests
- `test_scripting.cpp`: Scripting integration tests

### Running Tests

```bash
# Build tests
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run component tests
ctest -R components_ --output-on-failure
```

---

## Best Practices

### Component Design

**DO:**

- Keep components focused on single responsibility
- Use composition over inheritance
- Implement all lifecycle hooks
- Name components descriptively

**DON'T:**

- Store heavy resources in components (use shared handles)
- Create circular dependencies between components
- Store raw pointers to other components

### Memory Management

```cpp
// Good: Use component pools
ComponentPool<Enemy> enemyPool(100);
auto* enemy = enemyPool.acquire();

// Bad: Frequent allocation/deallocation
for (int i = 0; i < 1000; ++i) {
    auto* enemy = new Enemy();  // Expensive!
    // ...
    delete enemy;
}
```

### Scripting Safety

```cpp
// Always sandbox untrusted scripts
ScriptSandbox sandbox;
sandbox.setMemoryLimit(10 * 1024 * 1024);  // 10MB
sandbox.setTimeout(5000);  // 5 seconds

try {
    sandbox.execute(untrustedCode);
} catch (const ScriptException& e) {
    std::cerr << "Script error: " << e.what() << "\n";
}
```

---

## Related Modules

- **atom::meta**: Reflection and type traits
- **atom::system**: Process and system integration
- **atom::async**: Async component updates
- **atom::type**: Type utilities

---

## Change Log

### 2025-01-15

- Initial module documentation
- Documented core, scripting, lifecycle components
- Added usage examples and best practices

---

**Maintained By:** Atom Framework Team
