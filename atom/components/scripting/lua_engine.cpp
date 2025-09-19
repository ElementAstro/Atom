/*
 * lua_engine.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "lua_engine.hpp"

#if ATOM_ENABLE_LUA

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <algorithm>
#include <fstream>
#include <sstream>

namespace atom::components::scripting {

// LuaEngine implementation
LuaEngine::LuaEngine(const LuaConfig& config) : luaConfig_(config) {}

LuaEngine::~LuaEngine() { shutdown(); }

bool LuaEngine::initialize(const ScriptEngineConfig& config) {
    if (L_ != nullptr) {
        return true;  // Already initialized
    }

    return initializeLua();
}

bool LuaEngine::initializeLua() {
    // Create Lua state with custom allocator for memory limiting
    L_ = lua_newstate(luaAllocator, this);
    if (!L_) {
        return false;
    }

    // Set panic function
    lua_atpanic(L_, luaPanic);

    // Open standard libraries if enabled
    if (luaConfig_.enableStandardLibrary) {
        luaL_openlibs(L_);

        // Disable specific modules if requested
        for (const auto& module : luaConfig_.disabledModules) {
            lua_pushnil(L_);
            lua_setglobal(L_, module.c_str());
        }
    }

    // Setup sandbox restrictions
    setupSandbox();

    // Setup memory limits
    setupMemoryLimit();

    // Setup package paths
    setupPackagePaths();

    // Register basic functions
    registerBasicFunctions();

    // Setup debug hooks if enabled
    if (luaConfig_.enableDebug) {
        lua_sethook(L_, luaDebugHook, LUA_MASKLINE | LUA_MASKCALL | LUA_MASKRET,
                    0);
    }

    return true;
}

ScriptResult LuaEngine::executeScript(const std::string& script,
                                      const std::string& context) {
    return executeLuaCode(script, context);
}

ScriptResult LuaEngine::executeFile(const std::string& filename) {
    return loadLuaFile(filename);
}

ScriptResult LuaEngine::executeLuaCode(const std::string& code,
                                       const std::string& context) {
    if (!L_) {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Lua engine not initialized";
        return result;
    }

    const auto startTime = std::chrono::high_resolution_clock::now();
    ScriptResult result;

    // Compile the code
    int compileResult =
        luaL_loadbuffer(L_, code.c_str(), code.size(), context.c_str());
    if (compileResult != LUA_OK) {
        result.success = false;
        result.errorMessage = getLastLuaError();
        lua_pop(L_, 1);  // Remove error message
        return result;
    }

    // Execute the code
    int execResult = lua_pcall(L_, 0, LUA_MULTRET, 0);
    if (execResult != LUA_OK) {
        result.success = false;
        result.errorMessage = getLastLuaError();
        lua_pop(L_, 1);  // Remove error message
    } else {
        result.success = true;

        // Get return value if any
        if (lua_gettop(L_) > 0) {
            result.returnValue = popScriptValue();
        }
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTime =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                              startTime);

    // Update statistics
    statistics_.scriptsExecuted++;
    if (result.success) {
        statistics_.successfulExecutions++;
    } else {
        statistics_.failedExecutions++;
    }
    statistics_.totalExecutionTime += result.executionTime;

    return result;
}

ScriptResult LuaEngine::loadLuaFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Cannot open file: " + filename;
        return result;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    return executeLuaCode(buffer.str(), filename);
}

ScriptResult LuaEngine::callFunction(const std::string& functionName,
                                     const std::vector<ScriptValue>& args) {
    if (!L_) {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Lua engine not initialized";
        return result;
    }

    const auto startTime = std::chrono::high_resolution_clock::now();
    ScriptResult result;

    // Get the function
    lua_getglobal(L_, functionName.c_str());
    if (!lua_isfunction(L_, -1)) {
        result.success = false;
        result.errorMessage = "Function not found: " + functionName;
        lua_pop(L_, 1);
        return result;
    }

    // Push arguments
    for (const auto& arg : args) {
        pushScriptValue(arg);
    }

    // Call the function
    int callResult = lua_pcall(L_, static_cast<int>(args.size()), 1, 0);
    if (callResult != LUA_OK) {
        result.success = false;
        result.errorMessage = getLastLuaError();
        lua_pop(L_, 1);
    } else {
        result.success = true;

        // Get return value
        if (lua_gettop(L_) > 0) {
            result.returnValue = popScriptValue();
        }
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTime =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                              startTime);

    // Update statistics
    statistics_.functionsExecuted++;
    statistics_.totalExecutionTime += result.executionTime;

    return result;
}

void LuaEngine::setGlobal(const std::string& name, const ScriptValue& value) {
    if (!L_)
        return;

    pushScriptValue(value);
    lua_setglobal(L_, name.c_str());
}

std::optional<ScriptValue> LuaEngine::getGlobal(const std::string& name) {
    if (!L_)
        return std::nullopt;

    lua_getglobal(L_, name.c_str());
    if (lua_isnil(L_, -1)) {
        lua_pop(L_, 1);
        return std::nullopt;
    }

    ScriptValue value = popScriptValue();
    return value;
}

void LuaEngine::registerFunction(const std::string& name,
                                 ScriptFunction function) {
    if (!L_)
        return;

    // Store the function in our registry
    registeredFunctions_[name] = function;

    // Create a Lua C function wrapper
    lua_pushlightuserdata(L_, this);
    lua_pushstring(L_, name.c_str());
    lua_pushcclosure(L_, luaFunctionWrapper, 2);
    lua_setglobal(L_, name.c_str());
}

void LuaEngine::shutdown() {
    if (L_) {
        lua_close(L_);
        L_ = nullptr;
    }
    registeredFunctions_.clear();
}

ScriptValue LuaEngine::cppToScript(const std::any& value) {
    // Simplified conversion - would need more comprehensive type handling
    return ScriptValue();
}

std::any LuaEngine::scriptToCpp(const ScriptValue& value,
                                const std::type_info& targetType) {
    // Simplified conversion - would need more comprehensive type handling
    return std::any();
}

// Helper methods
void LuaEngine::setupSandbox() {
    // Remove dangerous functions in sandbox mode
    const char* dangerousFunctions[] = {"os.execute", "os.exit",  "os.remove",
                                        "os.rename",  "io.popen", "loadfile",
                                        "dofile",     "require"};

    for (const char* func : dangerousFunctions) {
        lua_pushnil(L_);
        lua_setglobal(L_, func);
    }
}

void LuaEngine::setupMemoryLimit() {
    // Memory limiting is handled by the custom allocator
}

void LuaEngine::setupPackagePaths() {
    if (luaConfig_.packagePaths.empty())
        return;

    // Get package.path
    lua_getglobal(L_, "package");
    lua_getfield(L_, -1, "path");

    std::string currentPath = lua_tostring(L_, -1);
    lua_pop(L_, 1);

    // Add new paths
    for (const auto& path : luaConfig_.packagePaths) {
        currentPath += ";" + path;
    }

    // Set new path
    lua_pushstring(L_, currentPath.c_str());
    lua_setfield(L_, -2, "path");
    lua_pop(L_, 1);  // Remove package table
}

void LuaEngine::registerBasicFunctions() {
    // Register basic utility functions
    registerFunction(
        "print", [](const std::vector<ScriptValue>& args) -> ScriptValue {
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0)
                    std::cout << "\t";

                if (args[i].holds<std::string>()) {
                    std::cout << args[i].get<std::string>();
                } else if (args[i].holds<int64_t>()) {
                    std::cout << args[i].get<int64_t>();
                } else if (args[i].holds<double>()) {
                    std::cout << args[i].get<double>();
                } else if (args[i].holds<bool>()) {
                    std::cout << (args[i].get<bool>() ? "true" : "false");
                }
            }
            std::cout << std::endl;
            return ScriptValue();
        });
}

void LuaEngine::pushScriptValue(const ScriptValue& value) {
    if (value.holds<std::monostate>()) {
        lua_pushnil(L_);
    } else if (value.holds<bool>()) {
        lua_pushboolean(L_, value.get<bool>() ? 1 : 0);
    } else if (value.holds<int64_t>()) {
        lua_pushinteger(L_, value.get<int64_t>());
    } else if (value.holds<double>()) {
        lua_pushnumber(L_, value.get<double>());
    } else if (value.holds<std::string>()) {
        lua_pushstring(L_, value.get<std::string>().c_str());
    } else {
        lua_pushnil(L_);  // Fallback for unsupported types
    }
}

ScriptValue LuaEngine::popScriptValue() { return luaValueToScriptValue(-1); }

ScriptValue LuaEngine::luaValueToScriptValue(int index) {
    int type = lua_type(L_, index);

    switch (type) {
        case LUA_TNIL:
            lua_pop(L_, 1);
            return ScriptValue();

        case LUA_TBOOLEAN: {
            bool value = lua_toboolean(L_, index) != 0;
            lua_pop(L_, 1);
            return ScriptValue(value);
        }

        case LUA_TNUMBER: {
            if (lua_isinteger(L_, index)) {
                int64_t value = lua_tointeger(L_, index);
                lua_pop(L_, 1);
                return ScriptValue(value);
            } else {
                double value = lua_tonumber(L_, index);
                lua_pop(L_, 1);
                return ScriptValue(value);
            }
        }

        case LUA_TSTRING: {
            std::string value = lua_tostring(L_, index);
            lua_pop(L_, 1);
            return ScriptValue(value);
        }

        default:
            lua_pop(L_, 1);
            return ScriptValue();  // Unsupported type
    }
}

std::string LuaEngine::getLastLuaError() {
    if (!L_ || lua_gettop(L_) == 0) {
        return "Unknown Lua error";
    }

    std::string error = lua_tostring(L_, -1);
    return error;
}

// Static callback functions
void* LuaEngine::luaAllocator(void* ud, void* ptr, size_t osize, size_t nsize) {
    LuaEngine* engine = static_cast<LuaEngine*>(ud);

    if (nsize == 0) {
        free(ptr);
        return nullptr;
    }

    // Check memory limit
    if (nsize > engine->luaConfig_.memoryLimit) {
        return nullptr;  // Allocation would exceed limit
    }

    return realloc(ptr, nsize);
}

int LuaEngine::luaPanic(lua_State* L) {
    const char* msg = lua_tostring(L, -1);
    std::cerr << "Lua panic: " << (msg ? msg : "unknown error") << std::endl;
    return 0;
}

int LuaEngine::luaFunctionWrapper(lua_State* L) {
    // Get engine and function name from upvalues
    LuaEngine* engine =
        static_cast<LuaEngine*>(lua_touserdata(L, lua_upvalueindex(1)));
    const char* funcName = lua_tostring(L, lua_upvalueindex(2));

    // Find the registered function
    auto it = engine->registeredFunctions_.find(funcName);
    if (it == engine->registeredFunctions_.end()) {
        lua_pushstring(L, "Function not found");
        lua_error(L);
        return 0;
    }

    // Convert arguments
    int argc = lua_gettop(L);
    std::vector<ScriptValue> args;
    args.reserve(argc);

    for (int i = 1; i <= argc; ++i) {
        args.push_back(engine->luaValueToScriptValue(i));
    }

    // Clear the stack
    lua_settop(L, 0);

    // Call the function
    ScriptValue result = it->second(args);

    // Push result
    engine->pushScriptValue(result);

    return 1;  // Number of return values
}

void LuaEngine::luaDebugHook(lua_State* L, lua_Debug* ar) {
    // Debug hook implementation would go here
    // For now, just a placeholder
}

// LuaEngineFactory implementation
std::unique_ptr<LuaEngine> LuaEngineFactory::create(const LuaConfig& config) {
    return std::make_unique<LuaEngine>(config);
}

std::string LuaEngineFactory::getVersion() { return LUA_VERSION; }

// Advanced Type Conversion System Implementation

// Template specializations for common types
template <>
bool LuaEngine::pushValue<int>(const int& value) {
    if (!L_)
        return false;
    lua_pushinteger(L_, value);
    return true;
}

template <>
bool LuaEngine::pushValue<double>(const double& value) {
    if (!L_)
        return false;
    lua_pushnumber(L_, value);
    return true;
}

template <>
bool LuaEngine::pushValue<std::string>(const std::string& value) {
    if (!L_)
        return false;
    lua_pushstring(L_, value.c_str());
    return true;
}

template <>
bool LuaEngine::pushValue<bool>(const bool& value) {
    if (!L_)
        return false;
    lua_pushboolean(L_, value ? 1 : 0);
    return true;
}

template <>
std::optional<int> LuaEngine::getValue<int>(int index) {
    if (!L_ || !lua_isinteger(L_, index))
        return std::nullopt;
    return static_cast<int>(lua_tointeger(L_, index));
}

template <>
std::optional<double> LuaEngine::getValue<double>(int index) {
    if (!L_ || !lua_isnumber(L_, index))
        return std::nullopt;
    return lua_tonumber(L_, index);
}

template <>
std::optional<std::string> LuaEngine::getValue<std::string>(int index) {
    if (!L_ || !lua_isstring(L_, index))
        return std::nullopt;
    return std::string(lua_tostring(L_, index));
}

template <>
std::optional<bool> LuaEngine::getValue<bool>(int index) {
    if (!L_ || !lua_isboolean(L_, index))
        return std::nullopt;
    return lua_toboolean(L_, index) != 0;
}

// STL Container Conversions

template <>
bool LuaEngine::pushContainer<std::vector<int>>(
    const std::vector<int>& container) {
    if (!L_)
        return false;

    lua_createtable(L_, static_cast<int>(container.size()), 0);

    for (size_t i = 0; i < container.size(); ++i) {
        lua_pushinteger(L_, i + 1);  // Lua arrays are 1-indexed
        lua_pushinteger(L_, container[i]);
        lua_settable(L_, -3);
    }

    return true;
}

template <>
bool LuaEngine::pushContainer<std::vector<std::string>>(
    const std::vector<std::string>& container) {
    if (!L_)
        return false;

    lua_createtable(L_, static_cast<int>(container.size()), 0);

    for (size_t i = 0; i < container.size(); ++i) {
        lua_pushinteger(L_, i + 1);
        lua_pushstring(L_, container[i].c_str());
        lua_settable(L_, -3);
    }

    return true;
}

template <>
bool LuaEngine::pushContainer<std::unordered_map<std::string, int>>(
    const std::unordered_map<std::string, int>& container) {
    if (!L_)
        return false;

    lua_createtable(L_, 0, static_cast<int>(container.size()));

    for (const auto& [key, value] : container) {
        lua_pushstring(L_, key.c_str());
        lua_pushinteger(L_, value);
        lua_settable(L_, -3);
    }

    return true;
}

template <>
std::optional<std::vector<int>> LuaEngine::getContainer<std::vector<int>>(
    int index) {
    if (!L_ || !lua_istable(L_, index))
        return std::nullopt;

    std::vector<int> result;
    size_t len = lua_rawlen(L_, index);
    result.reserve(len);

    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L_, index, i);
        if (lua_isinteger(L_, -1)) {
            result.push_back(static_cast<int>(lua_tointeger(L_, -1)));
        }
        lua_pop(L_, 1);
    }

    return result;
}

template <>
std::optional<std::vector<std::string>>
LuaEngine::getContainer<std::vector<std::string>>(int index) {
    if (!L_ || !lua_istable(L_, index))
        return std::nullopt;

    std::vector<std::string> result;
    size_t len = lua_rawlen(L_, index);
    result.reserve(len);

    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L_, index, i);
        if (lua_isstring(L_, -1)) {
            result.emplace_back(lua_tostring(L_, -1));
        }
        lua_pop(L_, 1);
    }

    return result;
}

template <>
std::optional<std::unordered_map<std::string, int>>
LuaEngine::getContainer<std::unordered_map<std::string, int>>(int index) {
    if (!L_ || !lua_istable(L_, index))
        return std::nullopt;

    std::unordered_map<std::string, int> result;

    lua_pushnil(L_);  // First key
    while (lua_next(L_, index) != 0) {
        // Key is at index -2, value at index -1
        if (lua_isstring(L_, -2) && lua_isinteger(L_, -1)) {
            std::string key = lua_tostring(L_, -2);
            int value = static_cast<int>(lua_tointeger(L_, -1));
            result[key] = value;
        }
        lua_pop(L_, 1);  // Remove value, keep key for next iteration
    }

    return result;
}

}  // namespace atom::components::scripting

#endif  // ATOM_ENABLE_LUA
