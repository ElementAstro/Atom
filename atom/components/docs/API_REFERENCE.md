# Enhanced Component System - API Reference

## Table of Contents

1. [Core Components](#core-components)
2. [Memory Management](#memory-management)
3. [Lifecycle Management](#lifecycle-management)
4. [Serialization](#serialization)
5. [Scripting Engines](#scripting-engines)
6. [Advanced Bindings](#advanced-bindings)
7. [Type Conversion](#type-conversion)
8. [Sandbox Security](#sandbox-security)

## Core Components

### Component Class

The base component class providing fundamental functionality.

```cpp
class Component {
public:
    Component(const std::string& name);

    // Basic properties
    const std::string& getName() const;
    ComponentState getState() const;
    void setState(ComponentState state);

    // Variable management
    void setVar(const std::string& name, const Var& value);
    std::optional<Var> getVar(const std::string& name) const;
    bool hasVar(const std::string& name) const;
    void removeVar(const std::string& name);

    // Command system
    void addCommand(const std::string& name, CommandFunction func);
    bool executeCommand(const std::string& name, const std::vector<std::string>& args);

    // Performance monitoring
    const PerformanceMetrics& getPerformanceMetrics() const;
};
```

### Registry Class

Central registry for component management.

```cpp
class Registry {
public:
    static Registry& instance();

    // Component management
    template <typename T, typename... Args>
    std::shared_ptr<T> createComponent(const std::string& name, Args&&... args);

    std::shared_ptr<Component> getComponent(const std::string& name) const;
    std::vector<std::shared_ptr<Component>> getAllComponents() const;
    bool removeComponent(const std::string& name);

    // Component information
    const ComponentInfo& getComponentInfo(const std::string& name) const;
    bool updateComponentInfo(const std::string& name, const ComponentInfo& info);

    // File operations
    bool loadComponentFromFile(const std::string& path);
    bool watchComponentChanges(bool enable = true);
};
```

## Memory Management

### ComponentPool

Specialized memory pool for efficient component allocation.

```cpp
template <typename T>
class ComponentPool {
public:
    explicit ComponentPool(const PoolConfig& config);

    // Allocation
    std::shared_ptr<T> allocate(const std::string& name);
    void deallocate(std::shared_ptr<T> component);

    // Pool management
    void resize(size_t newSize);
    void defragment();
    void clear();

    // Statistics
    const PoolStatistics& getStatistics() const;
    void resetStatistics();
};
```

### PoolConfig

Configuration for memory pools.

```cpp
struct PoolConfig {
    size_t initialPoolSize = 32;
    size_t maxPoolSize = 1024;
    size_t chunkSize = 8;
    bool enableStatistics = true;
    bool enableDefragmentation = true;
    std::chrono::milliseconds defragmentationInterval{5000};
};
```

## Lifecycle Management

### LifecycleManager

Manages component lifecycle and dependencies.

```cpp
class LifecycleManager {
public:
    static LifecycleManager& instance();

    // Lifecycle hooks
    void registerHook(const std::string& componentName,
                     LifecyclePhase phase,
                     LifecycleHook hook);

    bool executePhase(Component& component, LifecyclePhase phase);

    // Dependency management
    void addDependency(const std::string& componentName,
                      const DependencyConstraint& dependency);

    std::vector<std::string> resolveDependencies(const std::string& componentName);
    bool validateDependencies(const std::string& componentName);
};
```

### Lifecycle Phases

```cpp
enum class LifecyclePhase {
    PreConstruction,
    PostConstruction,
    PreInitialization,
    PostInitialization,
    PreActivation,
    PostActivation,
    PreDeactivation,
    PostDeactivation,
    PreDestruction,
    PostDestruction
};
```

## Serialization

### SerializationManager

Handles component serialization in multiple formats.

```cpp
class SerializationManager {
public:
    static SerializationManager& instance();

    // Serialization
    SerializationResult serialize(const Component& component,
                                 const SerializationOptions& options);

    bool serializeToFile(const Component& component,
                        const std::string& filename,
                        const SerializationOptions& options);

    // Deserialization
    DeserializationResult deserialize(const std::vector<uint8_t>& data,
                                     const SerializationOptions& options);

    DeserializationResult deserializeFromFile(const std::string& filename,
                                             const SerializationOptions& options);
};
```

### Serialization Options

```cpp
struct SerializationOptions {
    SerializationFormat format = SerializationFormat::JSON;
    bool includeMetadata = true;
    bool includeTimestamp = true;
    bool enableCompression = false;
    CompressionType compressionType = CompressionType::None;
    int compressionLevel = 6;
};

enum class SerializationFormat {
    JSON,
    Binary,
    XML,
    MessagePack
};
```

## Scripting Engines

### Lua Engine

Lua scripting engine with JIT support.

```cpp
class LuaEngine : public IScriptEngine {
public:
    explicit LuaEngine(const LuaConfig& config = {});

    // Script execution
    ScriptResult executeScript(const std::string& script,
                              const std::string& context = "") override;
    ScriptResult executeFile(const std::string& filename) override;
    ScriptResult callFunction(const std::string& functionName,
                             const std::vector<ScriptValue>& args = {}) override;

    // Global variables
    void setGlobal(const std::string& name, const ScriptValue& value) override;
    std::optional<ScriptValue> getGlobal(const std::string& name) override;

    // Function registration
    void registerFunction(const std::string& name, ScriptFunction function) override;

    // Advanced type conversion
    template <typename T>
    bool pushValue(const T& value);

    template <typename T>
    std::optional<T> getValue(int index = -1);

    template <typename Container>
    bool pushContainer(const Container& container);

    template <typename Container>
    std::optional<Container> getContainer(int index = -1);
};
```

### Python Engine

Python scripting engine with pybind11-compatible API.

```cpp
class PythonEngine : public IScriptEngine {
public:
    explicit PythonEngine(const PythonConfig& config = {});

    // Script execution
    ScriptResult executeScript(const std::string& script,
                              const std::string& context = "") override;
    ScriptResult executeFile(const std::string& filename) override;

    // Module management
    PyObjectWrapper importModule(const std::string& moduleName);
    PyObjectWrapper getMainModule() const;
    PyObjectWrapper getGlobals() const;

    // Enhanced execution
    ScriptResult executePythonCode(const std::string& code,
                                  const std::string& context = "",
                                  const std::string& mode = "exec");
};
```

### pybind11-Compatible Bindings

```cpp
namespace py {
    class module {
    public:
        explicit module(PythonEngine& engine, const std::string& name = "__main__");

        template <typename Func>
        module& def(const std::string& name, Func func, const std::string& doc = "");

        template <typename T>
        class_<T> class_(const std::string& name, const std::string& doc = "");

        template <typename T>
        module& attr(const std::string& name, const T& value);
    };

    template <typename T>
    class class_ {
    public:
        template <typename... Args>
        class_& def(const std::string& name = "__init__");

        template <typename Func>
        class_& def(const std::string& name, Func func, const std::string& doc = "");

        template <typename Func>
        class_& def_static(const std::string& name, Func func, const std::string& doc = "");

        template <typename Getter, typename Setter>
        class_& def_property(const std::string& name, Getter getter, Setter setter);

        template <typename Getter>
        class_& def_property_readonly(const std::string& name, Getter getter);
    };
}
```

## Advanced Bindings

### AdvancedClassBinder

Comprehensive class binding with full feature support.

```cpp
template <typename T, typename ScriptEngine>
class AdvancedClassBinder {
public:
    AdvancedClassBinder(ScriptEngine& engine, const std::string& className);

    // Constructor binding
    template <typename... Args>
    AdvancedClassBinder& def_constructor();

    // Method binding
    template <typename Func>
    AdvancedClassBinder& def_method(const std::string& name, Func func,
                                   const std::string& doc = "");

    template <typename Func>
    AdvancedClassBinder& def_static_method(const std::string& name, Func func,
                                          const std::string& doc = "");

    // Operator binding
    OperatorBinder<T>& operators();
    PropertyBinder<T>& properties();

    // Enum binding
    template <typename E>
    AdvancedClassBinder& def_enum(const std::string& enumName,
                                 const std::vector<std::pair<E, std::string>>& values);

    // String conversion
    template <typename Func>
    AdvancedClassBinder& def_str(Func func);

    template <typename Func>
    AdvancedClassBinder& def_repr(Func func);

    void finalize();
};
```

### Operator Binding

```cpp
template <typename T>
class OperatorBinder {
public:
    template <typename Op>
    OperatorBinder& def_operator(OperatorType op, Op func);

    template <typename Op>
    OperatorBinder& def_comparison(OperatorType op, Op func);

    template <typename IndexType, typename ReturnType>
    OperatorBinder& def_index(std::function<ReturnType(const T&, IndexType)> getter,
                             std::function<void(T&, IndexType, const ReturnType&)> setter = nullptr);

    template <typename ReturnType, typename... Args>
    OperatorBinder& def_call(std::function<ReturnType(T&, Args...)> func);
};

enum class OperatorType {
    Add, Subtract, Multiply, Divide, Modulo,
    Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual,
    Index, Call, ToString, Length, Iterator
};
```

## Type Conversion

### TypeConverter

Generic type converter for automatic C++/script type conversion.

```cpp
template <typename ScriptEngine>
class TypeConverter {
public:
    explicit TypeConverter(ScriptEngine& engine);

    // Basic conversion
    template <typename T>
    bool toScript(const T& value);

    template <typename T>
    std::optional<T> fromScript(int index = -1);

    // Container conversion
    template <typename Container>
    bool containerToScript(const Container& container);

    template <typename Container>
    std::optional<Container> containerFromScript(int index = -1);

    // Advanced types
    template <typename T>
    bool optionalToScript(const std::optional<T>& opt);

    template <typename... Args>
    bool tupleToScript(const std::tuple<Args...>& tuple);

    template <typename... Args>
    bool variantToScript(const std::variant<Args...>& variant);
};
```

## Sandbox Security

### ScriptSandbox

Secure script execution environment.

```cpp
class ScriptSandbox {
public:
    explicit ScriptSandbox(const SandboxConfig& config = {});

    bool initialize();
    void shutdown();

    // Sandboxed execution
    std::unique_ptr<IScriptEngine> createSandboxedEngine(ScriptLanguage language,
                                                         const std::string& scriptName);

    ScriptResult executeInSandbox(const std::string& script,
                                 const std::string& scriptName,
                                 bool isFile = false,
                                 ScriptLanguage language = ScriptLanguage::Auto);

    bool terminateScript(const std::string& scriptName);

    // Statistics and monitoring
    const Statistics& getStatistics() const;
    std::vector<SandboxViolation> getRecentViolations(size_t limit = 100) const;

    void updateConfig(const SandboxConfig& config);
};
```

### Sandbox Configuration

```cpp
struct SandboxConfig {
    Permission permissions = Permission::ComponentAccess;
    ResourceLimits limits;
    bool enableLogging = true;
    bool enableProfiling = false;
    bool strictMode = true;
    std::vector<std::string> allowedPaths;
    std::vector<std::string> blockedPaths;
    std::vector<std::string> allowedFunctions;
    std::vector<std::string> blockedFunctions;
    std::function<void(const SandboxViolation&)> violationCallback;
};

struct ResourceLimits {
    size_t maxMemoryUsage = 64 * 1024 * 1024;  // 64MB
    std::chrono::milliseconds maxExecutionTime{30000};  // 30 seconds
    size_t maxStackDepth = 1000;
    size_t maxFileSize = 10 * 1024 * 1024;  // 10MB
    size_t maxOpenFiles = 100;
    size_t maxNetworkConnections = 10;
    size_t maxThreads = 4;
    double maxCpuUsage = 0.8;  // 80% CPU usage
};

enum class Permission : uint32_t {
    None = 0,
    ReadFiles = 1 << 0,
    WriteFiles = 1 << 1,
    ExecuteCommands = 1 << 2,
    NetworkAccess = 1 << 3,
    SystemInfo = 1 << 4,
    ComponentAccess = 1 << 5,
    RegistryAccess = 1 << 6,
    MemoryAccess = 1 << 7,
    ThreadAccess = 1 << 8,
    All = 0xFFFFFFFF
};
```
