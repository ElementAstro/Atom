/*
 * script_engine.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Script Loading and Execution System
Provides comprehensive script loading, compilation, caching,
and execution with error handling and debugging support.

**************************************************/

#ifndef ATOM_COMPONENT_SCRIPT_ENGINE_HPP
#define ATOM_COMPONENT_SCRIPT_ENGINE_HPP

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
#include <vector>

#include "scripting_api.hpp"

namespace atom::components::scripting {

/**
 * @brief Compiled script representation
 */
struct CompiledScript {
    std::string source;
    std::string filename;
    std::vector<uint8_t> bytecode;  // Compiled bytecode (engine-specific)
    std::chrono::system_clock::time_point compilationTime;
    std::chrono::microseconds compilationDuration{0};
    size_t sourceHash = 0;
    bool isValid = false;
    std::string errorMessage;

    // Debugging information
    struct DebugInfo {
        std::vector<size_t> lineNumbers;
        std::unordered_map<std::string, size_t> functionOffsets;
        std::unordered_map<size_t, std::string> sourceLines;
    } debugInfo;
};

/**
 * @brief Script cache entry
 */
struct ScriptCacheEntry {
    std::shared_ptr<CompiledScript> compiledScript;
    std::filesystem::file_time_type lastModified;
    std::chrono::system_clock::time_point lastAccessed;
    size_t accessCount = 0;
    bool isPinned = false;  // Prevent eviction
};

/**
 * @brief Script execution context
 */
struct ExecutionContext {
    std::string scriptName;
    std::unordered_map<std::string, ScriptValue> localVariables;
    std::unordered_map<std::string, ScriptValue> globalVariables;
    std::vector<std::string> callStack;
    size_t currentLine = 0;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::milliseconds timeout{30000};  // 30 seconds default
    bool isDebugging = false;

    // Debugging state
    std::vector<size_t> breakpoints;
    bool stepMode = false;
    bool stepInto = false;
    bool stepOver = false;
    std::function<void(const ExecutionContext&)> debugCallback;
};

/**
 * @brief Script loader with caching and compilation
 */
class ScriptLoader {
public:
    /**
     * @brief Configuration for script loader
     */
    struct Config {
        size_t maxCacheSize = 100;  // Maximum cached scripts
        std::chrono::minutes cacheTimeout = std::chrono::hours(1);
        bool enableCompilation = true;
        bool enableCaching = true;
        bool enableDebugInfo = true;
        std::vector<std::string> includePaths;
        std::unordered_map<std::string, std::string> preprocessorDefines;
    };

    /**
     * @brief Constructs script loader with default configuration
     */
    ScriptLoader();

    /**
     * @brief Constructs script loader with configuration
     * @param config Loader configuration
     */
    explicit ScriptLoader(const Config& config);

    /**
     * @brief Loads and compiles a script from file
     * @param filename Script filename
     * @param language Target language (Auto for detection)
     * @return Compiled script or nullptr on failure
     */
    std::shared_ptr<CompiledScript> loadScript(
        const std::string& filename,
        ScriptLanguage language = ScriptLanguage::Auto);

    /**
     * @brief Compiles script from source code
     * @param source Script source code
     * @param filename Optional filename for debugging
     * @param language Target language
     * @return Compiled script or nullptr on failure
     */
    std::shared_ptr<CompiledScript> compileScript(
        const std::string& source, const std::string& filename = "",
        ScriptLanguage language = ScriptLanguage::Auto);

    /**
     * @brief Precompiles and caches a script
     * @param filename Script filename
     * @param language Target language
     * @return True if successful
     */
    bool precompileScript(const std::string& filename,
                          ScriptLanguage language = ScriptLanguage::Auto);

    /**
     * @brief Clears the script cache
     * @param force If true, clears pinned entries too
     */
    void clearCache(bool force = false);

    /**
     * @brief Gets cache statistics
     * @return Cache statistics
     */
    struct CacheStatistics {
        size_t totalEntries = 0;
        size_t pinnedEntries = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        size_t evictions = 0;
        size_t totalMemoryUsage = 0;
        double hitRatio = 0.0;
    };

    CacheStatistics getCacheStatistics() const;

    /**
     * @brief Pins a script in cache (prevents eviction)
     * @param filename Script filename
     */
    void pinScript(const std::string& filename);

    /**
     * @brief Unpins a script from cache
     * @param filename Script filename
     */
    void unpinScript(const std::string& filename);

private:
    Config config_;
    std::unordered_map<std::string, ScriptCacheEntry> cache_;
    mutable std::shared_mutex cacheMutex_;
    mutable CacheStatistics statistics_;

    // Helper methods
    std::string readFile(const std::string& filename);
    size_t calculateHash(const std::string& source);
    void evictOldEntries();
    std::shared_ptr<CompiledScript> compileForLanguage(
        const std::string& source, const std::string& filename,
        ScriptLanguage language);
};

/**
 * @brief Script executor with debugging support
 */
class ScriptExecutor {
public:
    /**
     * @brief Execution configuration
     */
    struct Config {
        std::chrono::milliseconds defaultTimeout{30000};
        size_t maxStackDepth = 1000;
        size_t maxMemoryUsage = 64 * 1024 * 1024;  // 64MB
        bool enableProfiling = false;
        bool enableDebugging = false;
        std::function<void(const std::string&)> logCallback;
        std::function<void(const ExecutionContext&)> debugCallback;
    };

    /**
     * @brief Constructs script executor with default configuration
     * @param engine Script engine instance
     */
    explicit ScriptExecutor(std::shared_ptr<IScriptEngine> engine);

    /**
     * @brief Constructs script executor
     * @param engine Script engine instance
     * @param config Executor configuration
     */
    ScriptExecutor(std::shared_ptr<IScriptEngine> engine, const Config& config);

    /**
     * @brief Executes a compiled script
     * @param script Compiled script
     * @param context Execution context
     * @return Execution result
     */
    ScriptResult execute(const CompiledScript& script,
                         ExecutionContext& context);

    /**
     * @brief Executes a script function
     * @param script Compiled script
     * @param functionName Function name
     * @param args Function arguments
     * @param context Execution context
     * @return Execution result
     */
    ScriptResult executeFunction(const CompiledScript& script,
                                 const std::string& functionName,
                                 const std::vector<ScriptValue>& args,
                                 ExecutionContext& context);

    /**
     * @brief Sets a breakpoint
     * @param filename Script filename
     * @param line Line number
     */
    void setBreakpoint(const std::string& filename, size_t line);

    /**
     * @brief Removes a breakpoint
     * @param filename Script filename
     * @param line Line number
     */
    void removeBreakpoint(const std::string& filename, size_t line);

    /**
     * @brief Clears all breakpoints
     */
    void clearBreakpoints();

    /**
     * @brief Gets execution statistics
     * @return Execution statistics
     */
    struct ExecutionStatistics {
        uint64_t totalExecutions = 0;
        uint64_t successfulExecutions = 0;
        uint64_t failedExecutions = 0;
        std::chrono::microseconds totalExecutionTime{0};
        std::chrono::microseconds averageExecutionTime{0};
        size_t peakMemoryUsage = 0;
        std::unordered_map<std::string, uint64_t> functionCallCounts;
    };

    const ExecutionStatistics& getStatistics() const { return statistics_; }
    void resetStatistics() { statistics_ = ExecutionStatistics{}; }

private:
    std::shared_ptr<IScriptEngine> engine_;
    Config config_;
    ExecutionStatistics statistics_;
    std::unordered_map<std::string, std::vector<size_t>> breakpoints_;
    mutable std::mutex breakpointsMutex_;

    // Helper methods
    bool checkTimeout(const ExecutionContext& context);
    void updateStatistics(const ScriptResult& result);
    void handleBreakpoint(ExecutionContext& context);
};

/**
 * @brief Integrated script manager combining loading and execution
 */
class ScriptManager {
public:
    /**
     * @brief Gets the singleton instance
     * @return Reference to script manager
     */
    static ScriptManager& instance();

    /**
     * @brief Initializes the script manager
     * @param loaderConfig Loader configuration
     * @param executorConfig Executor configuration
     * @return True if successful
     */
    bool initialize(const ScriptLoader::Config& loaderConfig = {},
                    const ScriptExecutor::Config& executorConfig = {});

    /**
     * @brief Loads and executes a script file
     * @param filename Script filename
     * @param language Target language
     * @return Execution result
     */
    ScriptResult executeFile(const std::string& filename,
                             ScriptLanguage language = ScriptLanguage::Auto);

    /**
     * @brief Compiles and executes script source
     * @param source Script source code
     * @param filename Optional filename for debugging
     * @param language Target language
     * @return Execution result
     */
    ScriptResult executeSource(const std::string& source,
                               const std::string& filename = "",
                               ScriptLanguage language = ScriptLanguage::Auto);

    /**
     * @brief Calls a function in a loaded script
     * @param filename Script filename
     * @param functionName Function name
     * @param args Function arguments
     * @return Execution result
     */
    ScriptResult callFunction(const std::string& filename,
                              const std::string& functionName,
                              const std::vector<ScriptValue>& args = {});

    /**
     * @brief Gets the script loader
     * @return Reference to script loader
     */
    ScriptLoader& getLoader() { return *loader_; }

    /**
     * @brief Gets the script executor
     * @return Reference to script executor
     */
    ScriptExecutor& getExecutor() { return *executor_; }

    /**
     * @brief Shutdown the script manager
     */
    void shutdown();

private:
    ScriptManager() = default;
    ~ScriptManager() = default;

    ScriptManager(const ScriptManager&) = delete;
    ScriptManager& operator=(const ScriptManager&) = delete;

    bool initialized_ = false;
    std::unique_ptr<ScriptLoader> loader_;
    std::unique_ptr<ScriptExecutor> executor_;
    std::shared_ptr<IScriptEngine> engine_;
};

}  // namespace atom::components::scripting

#endif  // ATOM_COMPONENT_SCRIPT_ENGINE_HPP
