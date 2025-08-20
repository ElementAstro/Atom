# Enhanced Component System - Usage Examples

## Table of Contents

1. [Basic Component Usage](#basic-component-usage)
2. [Memory Pool Examples](#memory-pool-examples)
3. [Lifecycle Management](#lifecycle-management)
4. [Serialization Examples](#serialization-examples)
5. [Lua Scripting Examples](#lua-scripting-examples)
6. [Python Scripting Examples](#python-scripting-examples)
7. [Advanced Binding Examples](#advanced-binding-examples)
8. [Sandbox Security Examples](#sandbox-security-examples)

## Basic Component Usage

### Creating and Managing Components

```cpp
#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"

int main() {
    // Get registry instance
    auto& registry = atom::components::Registry::instance();

    // Create a component
    auto component = registry.createComponent<Component>("MyComponent");

    // Set variables
    component->setVar("health", 100);
    component->setVar("name", std::string("Player"));
    component->setVar("position", std::vector<double>{10.0, 20.0, 30.0});

    // Get variables
    if (auto health = component->getVar("health")) {
        std::cout << "Health: " << health->toString() << std::endl;
    }

    // Add commands
    component->addCommand("heal", [](const std::vector<std::string>& args) {
        std::cout << "Healing component..." << std::endl;
        return true;
    });

    // Execute command
    component->executeCommand("heal", {});

    return 0;
}
```

### Custom Component Classes

```cpp
class PlayerComponent : public Component {
public:
    PlayerComponent(const std::string& name) : Component(name), health_(100), level_(1) {}

    void takeDamage(int damage) {
        health_ = std::max(0, health_ - damage);
        setVar("health", health_);

        if (health_ == 0) {
            setState(ComponentState::Inactive);
        }
    }

    void levelUp() {
        level_++;
        health_ = 100 + (level_ * 10);  // Increase max health
        setVar("level", level_);
        setVar("health", health_);
    }

    int getHealth() const { return health_; }
    int getLevel() const { return level_; }

private:
    int health_;
    int level_;
};

// Usage
auto player = registry.createComponent<PlayerComponent>("Player1");
player->takeDamage(25);
std::cout << "Player health: " << player->getHealth() << std::endl;
```

## Memory Pool Examples

### Basic Memory Pool Usage

```cpp
#include "atom/components/component_pool.hpp"

int main() {
    // Configure memory pool
    atom::components::PoolConfig config;
    config.initialPoolSize = 64;
    config.maxPoolSize = 256;
    config.chunkSize = 16;
    config.enableStatistics = true;

    // Create pool
    auto pool = std::make_unique<ComponentPool<Component>>(config);

    // Allocate components
    std::vector<std::shared_ptr<Component>> components;
    for (int i = 0; i < 100; ++i) {
        auto component = pool->allocate("Component" + std::to_string(i));
        component->setVar("id", i);
        components.push_back(component);
    }

    // Check statistics
    const auto& stats = pool->getStatistics();
    std::cout << "Total allocations: " << stats.totalAllocations.load() << std::endl;
    std::cout << "Pool hits: " << stats.poolHits.load() << std::endl;
    std::cout << "Pool misses: " << stats.poolMisses.load() << std::endl;

    // Components are automatically returned to pool when destroyed
    components.clear();

    return 0;
}
```

### Performance Comparison

```cpp
void benchmarkAllocation() {
    const int iterations = 10000;

    // Benchmark standard allocation
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto component = std::make_shared<Component>("TestComponent");
        // Use component...
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto standardTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Benchmark pool allocation
    ComponentPool<Component> pool({});
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto component = pool.allocate("TestComponent");
        // Use component...
    }
    end = std::chrono::high_resolution_clock::now();
    auto poolTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Standard allocation: " << standardTime.count() << " μs" << std::endl;
    std::cout << "Pool allocation: " << poolTime.count() << " μs" << std::endl;
    std::cout << "Speedup: " << (double)standardTime.count() / poolTime.count() << "x" << std::endl;
}
```

## Lifecycle Management

### Dependency Resolution

```cpp
#include "atom/components/lifecycle.hpp"

int main() {
    auto& lifecycle = atom::components::LifecycleManager::instance();
    auto& registry = atom::components::Registry::instance();

    // Create components
    auto renderer = registry.createComponent<Component>("Renderer");
    auto physics = registry.createComponent<Component>("Physics");
    auto game = registry.createComponent<Component>("Game");

    // Define dependencies: Game depends on Renderer and Physics
    DependencyConstraint rendererDep("Renderer", DependencyType::Required);
    DependencyConstraint physicsDep("Physics", DependencyType::Required);

    lifecycle.addDependency("Game", rendererDep);
    lifecycle.addDependency("Game", physicsDep);

    // Resolve dependencies (returns components in correct initialization order)
    auto initOrder = lifecycle.resolveDependencies("Game");

    std::cout << "Initialization order:" << std::endl;
    for (const auto& componentName : initOrder) {
        std::cout << "  " << componentName << std::endl;
    }

    return 0;
}
```

### Lifecycle Hooks

```cpp
void setupLifecycleHooks() {
    auto& lifecycle = LifecycleManager::instance();

    // Register initialization hook
    lifecycle.registerHook("Renderer", LifecyclePhase::PostConstruction,
        [](Component& component, LifecyclePhase phase) {
            std::cout << "Renderer initialized" << std::endl;
            component.setVar("initialized", true);
        });

    // Register cleanup hook
    lifecycle.registerHook("Renderer", LifecyclePhase::PreDestruction,
        [](Component& component, LifecyclePhase phase) {
            std::cout << "Cleaning up renderer resources" << std::endl;
            // Cleanup code here
        });

    // Execute lifecycle phases
    auto renderer = Registry::instance().getComponent("Renderer");
    lifecycle.executePhase(*renderer, LifecyclePhase::PostConstruction);
}
```

## Serialization Examples

### JSON Serialization

```cpp
#include "atom/components/serialization.hpp"

void serializeComponent() {
    auto& serializer = SerializationManager::instance();
    auto& registry = Registry::instance();

    // Create and configure component
    auto component = registry.createComponent<Component>("SavedComponent");
    component->setVar("level", 42);
    component->setVar("name", std::string("Hero"));
    component->setVar("inventory", std::vector<std::string>{"sword", "potion", "key"});

    // Configure serialization
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    options.includeMetadata = true;
    options.includeTimestamp = true;

    // Serialize to file
    bool success = serializer.serializeToFile(*component, "component.json", options);
    if (success) {
        std::cout << "Component serialized successfully" << std::endl;
    }

    // Load from file
    auto result = serializer.deserializeFromFile("component.json", options);
    if (result.success) {
        auto loadedComponent = result.component;
        std::cout << "Loaded component: " << loadedComponent->getName() << std::endl;

        if (auto level = loadedComponent->getVar("level")) {
            std::cout << "Level: " << level->toString() << std::endl;
        }
    }
}
```

### Binary Serialization with Compression

```cpp
void binarySerializationExample() {
    auto& serializer = SerializationManager::instance();

    // Create large component with lots of data
    auto component = Registry::instance().createComponent<Component>("LargeComponent");

    // Add lots of data
    for (int i = 0; i < 1000; ++i) {
        component->setVar("data_" + std::to_string(i),
                         std::string(100, 'A' + (i % 26)));
    }

    // Configure binary serialization with compression
    SerializationOptions options;
    options.format = SerializationFormat::Binary;
    options.enableCompression = true;
    options.compressionType = CompressionType::LZ4;
    options.compressionLevel = 9;  // Maximum compression

    // Serialize
    auto result = serializer.serialize(*component, options);
    if (result.success) {
        std::cout << "Compressed size: " << result.data.size() << " bytes" << std::endl;
        std::cout << "Compression ratio: " << result.compressionRatio << std::endl;
    }
}
```

## Lua Scripting Examples

### Basic Lua Integration

```cpp
#if ATOM_ENABLE_LUA
#include "atom/components/lua_engine.hpp"

void luaScriptingExample() {
    // Create Lua engine
    LuaConfig config;
    config.enableJIT = true;
    config.memoryLimit = 32 * 1024 * 1024;  // 32MB

    auto engine = LuaEngineFactory::create(config);

    ScriptEngineConfig engineConfig;
    engine->initialize(engineConfig);

    // Execute Lua script
    std::string script = R"(
        -- Define a function
        function calculateDamage(baseDamage, multiplier)
            return baseDamage * multiplier
        end

        -- Create a table
        player = {
            name = "Hero",
            level = 10,
            health = 100
        }

        -- Calculate damage
        damage = calculateDamage(25, 1.5)
        print("Calculated damage: " .. damage)

        return damage
    )";

    auto result = engine->executeScript(script);
    if (result.success) {
        std::cout << "Script executed successfully" << std::endl;
        if (result.returnValue.holds<double>()) {
            std::cout << "Returned damage: " << result.returnValue.get<double>() << std::endl;
        }
    }

    // Call Lua function from C++
    std::vector<ScriptValue> args = {ScriptValue(30.0), ScriptValue(2.0)};
    auto funcResult = engine->callFunction("calculateDamage", args);
    if (funcResult.success) {
        std::cout << "Function result: " << funcResult.returnValue.get<double>() << std::endl;
    }
}
#endif
```

### Advanced Lua Type Conversion

```cpp
#if ATOM_ENABLE_LUA
void luaTypeConversionExample() {
    auto engine = LuaEngineFactory::create();
    engine->initialize({});

    // Convert C++ containers to Lua
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    engine->pushContainer(numbers);
    engine->setGlobal("numbers", engine->popScriptValue());

    std::unordered_map<std::string, int> scores = {
        {"player1", 100},
        {"player2", 85},
        {"player3", 92}
    };
    engine->pushContainer(scores);
    engine->setGlobal("scores", engine->popScriptValue());

    // Use in Lua script
    std::string script = R"(
        -- Access C++ data from Lua
        print("Numbers:")
        for i, v in ipairs(numbers) do
            print("  " .. i .. ": " .. v)
        end

        print("Scores:")
        for name, score in pairs(scores) do
            print("  " .. name .. ": " .. score)
        end

        -- Return modified data
        scores["player4"] = 78
        return scores
    )";

    auto result = engine->executeScript(script);
    if (result.success) {
        // Convert result back to C++
        auto modifiedScores = engine->getContainer<std::unordered_map<std::string, int>>(-1);
        if (modifiedScores) {
            std::cout << "Modified scores from Lua:" << std::endl;
            for (const auto& [name, score] : *modifiedScores) {
                std::cout << "  " << name << ": " << score << std::endl;
            }
        }
    }
}
#endif
```

## Python Scripting Examples

### Basic Python Integration

```cpp
#if ATOM_ENABLE_PYTHON
#include "atom/components/python_engine.hpp"

void pythonScriptingExample() {
    // Create Python engine
    PythonConfig config;
    config.enableSitePackages = false;  // Isolated environment
    config.isolatedMode = true;

    auto engine = PythonEngineFactory::create(config);

    ScriptEngineConfig engineConfig;
    engine->initialize(engineConfig);

    // Execute Python script
    std::string script = R"(
import math

def calculate_distance(x1, y1, x2, y2):
    return math.sqrt((x2 - x1)**2 + (y2 - y1)**2)

# Create player data
player = {
    'name': 'Hero',
    'position': [10, 20],
    'health': 100,
    'inventory': ['sword', 'potion', 'key']
}

# Calculate distance to target
target_pos = [50, 80]
distance = calculate_distance(
    player['position'][0], player['position'][1],
    target_pos[0], target_pos[1]
)

print(f"Distance to target: {distance:.2f}")
result = distance
    )";

    auto result = engine->executeScript(script);
    if (result.success) {
        std::cout << "Python script executed successfully" << std::endl;

        // Get result
        auto distance = engine->getGlobal("result");
        if (distance && distance->holds<double>()) {
            std::cout << "Distance: " << distance->get<double>() << std::endl;
        }
    }
}
#endif
```

### pybind11-Style Bindings

```cpp
#if ATOM_ENABLE_PYTHON
void pythonBindingExample() {
    auto engine = PythonEngineFactory::create();
    engine->initialize({});

    // Create pybind11-style module
    auto m = ATOM_PYTHON_MODULE(*engine, "game");

    // Bind PlayerComponent class
    ATOM_PYTHON_CLASS(*engine, PlayerComponent)
        .def("takeDamage", &PlayerComponent::takeDamage)
        .def("levelUp", &PlayerComponent::levelUp)
        .def("getHealth", &PlayerComponent::getHealth)
        .def("getLevel", &PlayerComponent::getLevel);

    // Test the binding
    std::string script = R"(
# Create player instance (this would need proper constructor binding)
# player = PlayerComponent("TestPlayer")
# player.takeDamage(25)
# print(f"Player health: {player.getHealth()}")

print("Python binding example completed")
    )";

    engine->executeScript(script);
}
#endif
```

## Advanced Binding Examples

### Operator Overloading

```cpp
class Vector3D {
public:
    Vector3D(double x = 0, double y = 0, double z = 0) : x_(x), y_(y), z_(z) {}

    Vector3D operator+(const Vector3D& other) const {
        return Vector3D(x_ + other.x_, y_ + other.y_, z_ + other.z_);
    }

    Vector3D operator*(double scalar) const {
        return Vector3D(x_ * scalar, y_ * scalar, z_ * scalar);
    }

    bool operator==(const Vector3D& other) const {
        return x_ == other.x_ && y_ == other.y_ && z_ == other.z_;
    }

    std::string toString() const {
        return "Vector3D(" + std::to_string(x_) + ", " +
               std::to_string(y_) + ", " + std::to_string(z_) + ")";
    }

    double x_, y_, z_;
};

void bindVector3D() {
    auto engine = LuaEngineFactory::create();
    engine->initialize({});

    // Create advanced class binder
    auto binder = ATOM_BIND_CLASS(*engine, Vector3D);

    // Bind constructor
    binder.def_constructor<double, double, double>();

    // Bind operators
    ATOM_BIND_OPERATOR(binder, Add, &Vector3D::operator+);
    ATOM_BIND_OPERATOR(binder, Multiply,
        [](const Vector3D& v, double s) { return v * s; });
    ATOM_BIND_OPERATOR(binder, Equal, &Vector3D::operator==);

    // Bind properties
    ATOM_BIND_PROPERTY(binder, x,
        [](const Vector3D& v) { return v.x_; },
        [](Vector3D& v, double x) { v.x_ = x; });

    ATOM_BIND_READONLY_PROPERTY(binder, length,
        [](const Vector3D& v) {
            return std::sqrt(v.x_*v.x_ + v.y_*v.y_ + v.z_*v.z_);
        });

    // Bind string conversion
    binder.def_str(&Vector3D::toString);

    // Finalize binding
    binder.finalize();
}
```

## Sandbox Security Examples

### Secure Script Execution

```cpp
void sandboxExample() {
    // Configure sandbox with strict security
    SandboxConfig config;
    config.permissions = Permission::ComponentAccess | Permission::MemoryAccess;
    config.limits.maxMemoryUsage = 16 * 1024 * 1024;  // 16MB
    config.limits.maxExecutionTime = std::chrono::seconds(5);
    config.strictMode = true;

    // Block dangerous paths
    config.blockedPaths = {"/etc", "/sys", "/proc", "C:\\Windows"};

    // Allow only safe functions
    config.allowedFunctions = {"print", "math", "string", "table"};

    // Set violation callback
    config.violationCallback = [](const SandboxViolation& violation) {
        std::cout << "Security violation: " << violation.description << std::endl;
    };

    auto sandbox = std::make_unique<ScriptSandbox>(config);
    sandbox->initialize();

    // Execute potentially dangerous script
    std::string dangerousScript = R"(
        -- This script tries to access system resources
        local file = io.open("/etc/passwd", "r")  -- Should be blocked
        if file then
            print("Security breach!")
            file:close()
        else
            print("Access denied - security working")
        end

        -- This should work
        local result = 2 + 3
        print("Safe calculation: " .. result)
        return result
    )";

    auto result = sandbox->executeInSandbox(dangerousScript, "test_script",
                                           false, ScriptLanguage::Lua);

    if (result.success) {
        std::cout << "Script executed safely" << std::endl;
    } else {
        std::cout << "Script blocked: " << result.errorMessage << std::endl;
    }

    // Check for violations
    auto violations = sandbox->getRecentViolations(10);
    std::cout << "Violations detected: " << violations.size() << std::endl;
}
```
