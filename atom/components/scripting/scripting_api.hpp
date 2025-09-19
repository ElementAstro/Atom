/*
 * scripting_api.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Unified Scripting API Architecture
Provides a consistent interface for component scripting across
multiple scripting engines (Lua, ChaiScript) with automatic
type conversion and error handling.

**************************************************/

#ifndef ATOM_COMPONENT_SCRIPTING_API_HPP
#define ATOM_COMPONENT_SCRIPTING_API_HPP

#include <any>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

#include "../component.hpp"

// Forward declarations
class Registry;

namespace atom::components::scripting {

/**
 * @brief Supported scripting languages
 */
enum class ScriptLanguage : uint8_t {
    Lua,
    ChaiScript,
    Auto  // Auto-detect based on file extension or content
};

// Forward declaration for recursive variant
struct ScriptValue;

/**
 * @brief Script value types for cross-language compatibility
 */
struct ScriptValue {
    std::variant<std::monostate,  // nil/null
                 bool, int64_t, double, std::string, std::vector<ScriptValue>,
                 std::unordered_map<std::string, ScriptValue>>
        value;

    // Constructors for easy conversion
    ScriptValue() : value(std::monostate{}) {}
    ScriptValue(bool v) : value(v) {}
    ScriptValue(int v) : value(static_cast<int64_t>(v)) {}
    ScriptValue(int64_t v) : value(v) {}
    ScriptValue(double v) : value(v) {}
    ScriptValue(const std::string& v) : value(v) {}
    ScriptValue(const char* v) : value(std::string(v)) {}
    ScriptValue(const std::vector<ScriptValue>& v) : value(v) {}
    ScriptValue(const std::unordered_map<std::string, ScriptValue>& v)
        : value(v) {}

    // Assignment operators
    ScriptValue& operator=(
        const std::variant<std::monostate, bool, int64_t, double, std::string,
                           std::vector<ScriptValue>,
                           std::unordered_map<std::string, ScriptValue>>& v) {
        value = v;
        return *this;
    }

    // Access operators
    template <typename T>
    T& get() {
        return std::get<T>(value);
    }

    template <typename T>
    const T& get() const {
        return std::get<T>(value);
    }

    template <typename T>
    bool holds() const {
        return std::holds_alternative<T>(value);
    }
};

/**
 * @brief Script execution result
 */
struct ScriptResult {
    bool success = false;
    ScriptValue returnValue;
    std::string errorMessage;
    std::chrono::microseconds executionTime{0};
    size_t memoryUsed = 0;
};

/**
 * @brief Script function signature for callbacks
 */
using ScriptFunction =
    std::function<ScriptValue(const std::vector<ScriptValue>&)>;

/**
 * @brief Script engine configuration
 */
struct ScriptEngineConfig {
    ScriptLanguage language = ScriptLanguage::Auto;
    size_t memoryLimit = 64 * 1024 * 1024;  // 64MB
    std::chrono::milliseconds executionTimeout = std::chrono::seconds(30);
    bool enableDebug = true;
    bool enableHotReload = true;
    bool enableSandbox = true;
    std::vector<std::string> allowedModules;
    std::vector<std::string> blockedFunctions;
    std::unordered_map<std::string, ScriptValue> globalVariables;
};

/**
 * @brief Abstract script engine interface
 */
class IScriptEngine {
public:
    virtual ~IScriptEngine() = default;

    /**
     * @brief Initializes the script engine
     * @param config Engine configuration
     * @return True if initialization successful
     */
    virtual bool initialize(const ScriptEngineConfig& config) = 0;

    /**
     * @brief Executes a script from string
     * @param script Script source code
     * @param context Optional execution context
     * @return Script execution result
     */
    virtual ScriptResult executeScript(const std::string& script,
                                       const std::string& context = "") = 0;

    /**
     * @brief Executes a script from file
     * @param filename Script file path
     * @return Script execution result
     */
    virtual ScriptResult executeFile(const std::string& filename) = 0;

    /**
     * @brief Calls a script function
     * @param functionName Function name
     * @param args Function arguments
     * @return Script execution result
     */
    virtual ScriptResult callFunction(
        const std::string& functionName,
        const std::vector<ScriptValue>& args = {}) = 0;

    /**
     * @brief Sets a global variable
     * @param name Variable name
     * @param value Variable value
     */
    virtual void setGlobal(const std::string& name,
                           const ScriptValue& value) = 0;

    /**
     * @brief Gets a global variable
     * @param name Variable name
     * @return Variable value
     */
    virtual std::optional<ScriptValue> getGlobal(const std::string& name) = 0;

    /**
     * @brief Registers a C++ function for script access
     * @param name Function name in script
     * @param function C++ function
     */
    virtual void registerFunction(const std::string& name,
                                  ScriptFunction function) = 0;

    /**
     * @brief Registers a component type for script access
     * @tparam T Component type
     * @param typeName Type name in script
     */
    template <typename T>
    void registerComponentType(const std::string& typeName);

    /**
     * @brief Gets the supported language
     * @return Script language
     */
    virtual ScriptLanguage getLanguage() const = 0;

    /**
     * @brief Gets engine statistics
     * @return Engine statistics
     */
    struct Statistics {
        uint64_t scriptsExecuted = 0;
        uint64_t functionsExecuted = 0;
        uint64_t errorsEncountered = 0;
        std::chrono::microseconds totalExecutionTime{0};
        size_t peakMemoryUsage = 0;
        size_t currentMemoryUsage = 0;
    };

    virtual const Statistics& getStatistics() const = 0;
    virtual void resetStatistics() = 0;

    /**
     * @brief Cleanup and shutdown
     */
    virtual void shutdown() = 0;

protected:
    // Helper methods for type conversion
    virtual ScriptValue cppToScript(const std::any& value) = 0;
    virtual std::any scriptToCpp(const ScriptValue& value,
                                 const std::type_info& targetType) = 0;
};

/**
 * @brief Component scripting API - main interface for component script
 * integration
 */
class ComponentScriptingAPI {
public:
    /**
     * @brief Gets the singleton instance
     * @return Reference to the scripting API
     */
    static ComponentScriptingAPI& instance();

    /**
     * @brief Initializes the scripting system
     * @param config Default engine configuration
     * @return True if initialization successful
     */
    bool initialize(const ScriptEngineConfig& config = {});

    /**
     * @brief Creates a script engine for a specific language
     * @param language Target scripting language
     * @param config Engine configuration
     * @return Unique pointer to script engine
     */
    std::unique_ptr<IScriptEngine> createEngine(
        ScriptLanguage language, const ScriptEngineConfig& config = {});

    /**
     * @brief Executes a script with automatic engine selection
     * @param script Script source or filename
     * @param isFile True if script parameter is a filename
     * @param language Preferred language (Auto for auto-detection)
     * @return Script execution result
     */
    ScriptResult execute(const std::string& script, bool isFile = false,
                         ScriptLanguage language = ScriptLanguage::Auto);

    /**
     * @brief Registers component API functions
     * @param engine Target script engine
     */
    void registerComponentAPI(IScriptEngine& engine);

    /**
     * @brief Registers a component instance for script access
     * @param name Component name in script
     * @param component Component instance
     * @param engine Target script engine
     */
    void registerComponent(const std::string& name,
                           std::shared_ptr<Component> component,
                           IScriptEngine& engine);

    /**
     * @brief Auto-detects script language from content or filename
     * @param script Script content or filename
     * @param isFile True if script parameter is a filename
     * @return Detected language
     */
    ScriptLanguage detectLanguage(const std::string& script,
                                  bool isFile = false);

    /**
     * @brief Gets global scripting statistics
     * @return Global statistics
     */
    struct GlobalStatistics {
        std::unordered_map<ScriptLanguage, IScriptEngine::Statistics>
            engineStats;
        uint64_t totalEnginesCreated = 0;
        uint64_t activeEngines = 0;
        std::chrono::microseconds totalInitializationTime{0};
    };

    const GlobalStatistics& getGlobalStatistics() const { return globalStats_; }
    void resetGlobalStatistics();

    /**
     * @brief Shutdown the scripting system
     */
    void shutdown();

private:
    ComponentScriptingAPI() = default;
    ~ComponentScriptingAPI() = default;

    ComponentScriptingAPI(const ComponentScriptingAPI&) = delete;
    ComponentScriptingAPI& operator=(const ComponentScriptingAPI&) = delete;

    // Core API functions exposed to scripts
    static ScriptValue createComponent(const std::vector<ScriptValue>& args);
    static ScriptValue getComponent(const std::vector<ScriptValue>& args);
    static ScriptValue removeComponent(const std::vector<ScriptValue>& args);
    static ScriptValue listComponents(const std::vector<ScriptValue>& args);
    static ScriptValue callCommand(const std::vector<ScriptValue>& args);
    static ScriptValue getVariable(const std::vector<ScriptValue>& args);
    static ScriptValue setVariable(const std::vector<ScriptValue>& args);
    static ScriptValue addEventListener(const std::vector<ScriptValue>& args);
    static ScriptValue removeEventListener(
        const std::vector<ScriptValue>& args);
    static ScriptValue emitEvent(const std::vector<ScriptValue>& args);

    // Utility functions
    static ScriptValue log(const std::vector<ScriptValue>& args);
    static ScriptValue sleep(const std::vector<ScriptValue>& args);
    static ScriptValue getCurrentTime(const std::vector<ScriptValue>& args);

    bool initialized_ = false;
    ScriptEngineConfig defaultConfig_;
    GlobalStatistics globalStats_;
    std::vector<std::unique_ptr<IScriptEngine>> engines_;
};

/**
 * @brief Script hot-reloading manager
 */
class ScriptHotReloader {
public:
    /**
     * @brief Starts watching a script file for changes
     * @param filename Script file to watch
     * @param engine Script engine to reload with
     * @param callback Optional callback on reload
     */
    void watchFile(const std::string& filename, IScriptEngine& engine,
                   std::function<void(bool)> callback = nullptr);

    /**
     * @brief Stops watching a file
     * @param filename File to stop watching
     */
    void unwatchFile(const std::string& filename);

    /**
     * @brief Manually triggers reload of a watched file
     * @param filename File to reload
     * @return True if reload successful
     */
    bool reloadFile(const std::string& filename);

    /**
     * @brief Gets list of watched files
     * @return Vector of watched filenames
     */
    std::vector<std::string> getWatchedFiles() const;

    /**
     * @brief Starts the file watching service
     */
    void start();

    /**
     * @brief Stops the file watching service
     */
    void stop();

private:
    struct WatchedFile {
        std::string filename;
        IScriptEngine* engine;
        std::function<void(bool)> callback;
        std::filesystem::file_time_type lastModified;
    };

    std::vector<WatchedFile> watchedFiles_;
    std::atomic<bool> running_{false};
    std::thread watcherThread_;
    mutable std::mutex watchedFilesMutex_;

    void watcherLoop();
};

/**
 * @brief Utility functions for script value conversion
 */
namespace utils {

/**
 * @brief Converts C++ value to ScriptValue
 * @tparam T C++ type
 * @param value C++ value
 * @return ScriptValue
 */
template <typename T>
ScriptValue toScriptValue(const T& value);

/**
 * @brief Converts ScriptValue to C++ value
 * @tparam T Target C++ type
 * @param value ScriptValue
 * @return C++ value
 */
template <typename T>
T fromScriptValue(const ScriptValue& value);

/**
 * @brief Converts ScriptValue to string representation
 * @param value ScriptValue
 * @return String representation
 */
std::string scriptValueToString(const ScriptValue& value);

/**
 * @brief Parses string to ScriptValue
 * @param str String representation
 * @return ScriptValue
 */
ScriptValue stringToScriptValue(const std::string& str);

}  // namespace utils

}  // namespace atom::components::scripting

#endif  // ATOM_COMPONENT_SCRIPTING_API_HPP
