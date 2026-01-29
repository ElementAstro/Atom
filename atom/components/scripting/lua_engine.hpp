/*
 * lua_engine.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Lua Script Engine Implementation
Provides Lua scripting support with macro-based enabling,
comprehensive binding system, and performance optimizations.

**************************************************/

#ifndef ATOM_COMPONENT_LUA_ENGINE_HPP
#define ATOM_COMPONENT_LUA_ENGINE_HPP

// Macro to enable Lua support
#ifndef ATOM_ENABLE_LUA
#define ATOM_ENABLE_LUA 0
#endif

#if ATOM_ENABLE_LUA

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "scripting_api.hpp"

// Forward declarations for Lua
struct lua_State;

namespace atom::components::scripting {

/**
 * @brief Lua-specific configuration
 */
struct LuaConfig {
    bool enableJIT = true;                     // Use LuaJIT if available
    bool enableDebug = true;                   // Enable debug information
    size_t memoryLimit = 64 * 1024 * 1024;     // 64MB memory limit
    std::vector<std::string> packagePaths;     // Additional package paths
    std::vector<std::string> cPaths;           // Additional C library paths
    bool enableStandardLibrary = true;         // Enable Lua standard library
    std::vector<std::string> disabledModules;  // Disabled standard modules
};

/**
 * @brief Lua script engine implementation
 */
class LuaEngine : public IScriptEngine {
public:
    /**
     * @brief Constructs Lua engine with configuration
     * @param config Lua-specific configuration
     */
    explicit LuaEngine(const LuaConfig& config = {});

    /**
     * @brief Destructor
     */
    ~LuaEngine() override;

    // IScriptEngine interface implementation
    bool initialize(const ScriptEngineConfig& config) override;
    ScriptResult executeScript(const std::string& script,
                               const std::string& context = "") override;
    ScriptResult executeFile(const std::string& filename) override;
    ScriptResult callFunction(
        const std::string& functionName,
        const std::vector<ScriptValue>& args = {}) override;
    void setGlobal(const std::string& name, const ScriptValue& value) override;
    std::optional<ScriptValue> getGlobal(const std::string& name) override;
    void registerFunction(const std::string& name,
                          ScriptFunction function) override;
    ScriptLanguage getLanguage() const override { return ScriptLanguage::Lua; }
    const Statistics& getStatistics() const override { return statistics_; }
    void resetStatistics() override { statistics_ = Statistics{}; }
    void shutdown() override;

    /**
     * @brief Gets the underlying Lua state
     * @return Lua state pointer
     */
    lua_State* getLuaState() const { return L_; }

    /**
     * @brief Executes Lua code with error handling
     * @param code Lua code to execute
     * @param context Execution context for error reporting
     * @return Execution result
     */
    ScriptResult executeLuaCode(const std::string& code,
                                const std::string& context = "");

    /**
     * @brief Loads and executes a Lua file
     * @param filename Lua file path
     * @return Execution result
     */
    ScriptResult loadLuaFile(const std::string& filename);

    /**
     * @brief Registers a C++ class for Lua access
     * @tparam T Class type
     * @param className Class name in Lua
     */
    template <typename T>
    void registerClass(const std::string& className);

    /**
     * @brief Registers a C++ function for Lua access
     * @tparam Func Function type
     * @param name Function name in Lua
     * @param func C++ function
     */
    template <typename Func>
    void registerCppFunction(const std::string& name, Func func);

    /**
     * @brief Creates a Lua table from C++ map
     * @param map C++ map
     * @return True if successful
     */
    bool createLuaTable(
        const std::unordered_map<std::string, ScriptValue>& map);

    /**
     * @brief Gets Lua table as C++ map
     * @param tableName Table name in Lua
     * @return C++ map representation
     */
    std::unordered_map<std::string, ScriptValue> getLuaTable(
        const std::string& tableName);

    /**
     * @brief Advanced type conversion from C++ to Lua
     * @tparam T Type to convert
     * @param value C++ value
     * @return True if conversion successful
     */
    template <typename T>
    bool pushValue(const T& value);

    /**
     * @brief Advanced type conversion from Lua to C++
     * @tparam T Target C++ type
     * @param index Stack index
     * @return Converted value
     */
    template <typename T>
    std::optional<T> getValue(int index = -1);

    /**
     * @brief Converts STL container to Lua table
     * @tparam Container STL container type
     * @param container C++ container
     * @return True if conversion successful
     */
    template <typename Container>
    bool pushContainer(const Container& container);

    /**
     * @brief Converts Lua table to STL container
     * @tparam Container STL container type
     * @param index Stack index
     * @return Converted container
     */
    template <typename Container>
    std::optional<Container> getContainer(int index = -1);

protected:
    ScriptValue cppToScript(const std::any& value) override;
    std::any scriptToCpp(const ScriptValue& value,
                         const std::type_info& targetType) override;

private:
    lua_State* L_ = nullptr;
    LuaConfig luaConfig_;
    Statistics statistics_;
    std::unordered_map<std::string, ScriptFunction> registeredFunctions_;

    // Lua utility methods
    bool initializeLua();
    void setupSandbox();
    void setupMemoryLimit();
    void setupPackagePaths();
    void registerBasicFunctions();

    // Type conversion helpers
    void pushScriptValue(const ScriptValue& value);
    ScriptValue popScriptValue();
    ScriptValue luaValueToScriptValue(int index);
    void scriptValueToLua(const ScriptValue& value);

    // Error handling
    std::string getLastLuaError();
    void handleLuaError(int result, const std::string& context);

    // Memory management
    static void* luaAllocator(void* ud, void* ptr, size_t osize, size_t nsize);
    static int luaPanic(lua_State* L);

    // C function wrappers
    static int luaFunctionWrapper(lua_State* L);

    // Debug hooks
    static void luaDebugHook(lua_State* L, lua_Debug* ar);
};

/**
 * @brief Lua binding helper macros and functions
 */
namespace lua_binding {

/**
 * @brief Registers a component type for Lua access
 * @tparam T Component type
 * @param engine Lua engine
 * @param typeName Type name in Lua
 */
template <typename T>
void registerComponentType(LuaEngine& engine, const std::string& typeName);

/**
 * @brief Creates Lua bindings for a C++ class
 * @tparam T Class type
 * @param engine Lua engine
 * @param className Class name in Lua
 * @return Binding helper object
 */
template <typename T>
class LuaClassBinding {
public:
    explicit LuaClassBinding(LuaEngine& engine, const std::string& className);

    /**
     * @brief Binds a constructor
     * @tparam Args Constructor argument types
     * @return Reference to this binding
     */
    template <typename... Args>
    LuaClassBinding& constructor();

    /**
     * @brief Binds a method
     * @tparam Func Method type
     * @param name Method name in Lua
     * @param func Method pointer
     * @return Reference to this binding
     */
    template <typename Func>
    LuaClassBinding& method(const std::string& name, Func func);

    /**
     * @brief Binds a property getter
     * @tparam Func Getter type
     * @param name Property name in Lua
     * @param getter Getter function
     * @return Reference to this binding
     */
    template <typename Func>
    LuaClassBinding& property_readonly(const std::string& name, Func getter);

    /**
     * @brief Binds a property with getter and setter
     * @tparam Getter Getter type
     * @tparam Setter Setter type
     * @param name Property name in Lua
     * @param getter Getter function
     * @param setter Setter function
     * @return Reference to this binding
     */
    template <typename Getter, typename Setter>
    LuaClassBinding& property(const std::string& name, Getter getter,
                              Setter setter);

private:
    LuaEngine& engine_;
    std::string className_;
};

}  // namespace lua_binding

/**
 * @brief Lua binding macros for easy integration
 */
#define ATOM_LUA_BIND_CLASS(engine, className)                            \
    atom::components::scripting::lua_binding::LuaClassBinding<className>( \
        engine, #className)

#define ATOM_LUA_BIND_FUNCTION(engine, funcName, func) \
    engine.registerCppFunction(#funcName, func)

#define ATOM_LUA_BIND_COMPONENT(engine, ComponentType)               \
    atom::components::scripting::lua_binding::registerComponentType< \
        ComponentType>(engine, #ComponentType)

/**
 * @brief Lua engine factory
 */
class LuaEngineFactory {
public:
    /**
     * @brief Creates a Lua engine instance
     * @param config Lua configuration
     * @return Unique pointer to Lua engine
     */
    static std::unique_ptr<LuaEngine> create(const LuaConfig& config = {});

    /**
     * @brief Checks if Lua is available
     * @return True if Lua support is compiled in
     */
    static bool isAvailable() { return true; }

    /**
     * @brief Gets Lua version information
     * @return Version string
     */
    static std::string getVersion();
};

}  // namespace atom::components::scripting

#else  // ATOM_ENABLE_LUA

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace atom::components::scripting {

// Stub implementations when Lua is disabled
class LuaEngine {
public:
    explicit LuaEngine(const void* = nullptr) {}
    static bool isAvailable() { return false; }
};

class LuaEngineFactory {
public:
    static std::unique_ptr<LuaEngine> create(const void* = nullptr) {
        return nullptr;
    }
    static bool isAvailable() { return false; }
    static std::string getVersion() { return "Lua support not compiled in"; }
};

}  // namespace atom::components::scripting

#endif  // ATOM_ENABLE_LUA

#endif  // ATOM_COMPONENT_LUA_ENGINE_HPP
