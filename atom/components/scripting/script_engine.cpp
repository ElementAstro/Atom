/*
 * script_engine.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "script_engine.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace atom::components::scripting {

// ScriptLoader implementation
ScriptLoader::ScriptLoader() : config_({}) {}

ScriptLoader::ScriptLoader(const Config& config) : config_(config) {}

std::shared_ptr<CompiledScript> ScriptLoader::loadScript(
    const std::string& filename, ScriptLanguage language) {
    std::shared_lock lock(cacheMutex_);

    // Check cache first
    auto it = cache_.find(filename);
    if (it != cache_.end() && config_.enableCaching) {
        // Check if file has been modified
        std::filesystem::file_time_type lastWrite;
        try {
            lastWrite = std::filesystem::last_write_time(filename);
        } catch (const std::filesystem::filesystem_error&) {
            // File doesn't exist or can't be accessed
            cache_.erase(it);
            statistics_.cacheMisses++;
            lock.unlock();
            return nullptr;
        }

        if (lastWrite <= it->second.lastModified) {
            // Cache hit
            it->second.lastAccessed = std::chrono::system_clock::now();
            it->second.accessCount++;
            statistics_.cacheHits++;
            return it->second.compiledScript;
        } else {
            // File modified, remove from cache
            lock.unlock();
            std::unique_lock writeLock(cacheMutex_);
            cache_.erase(it);
            statistics_.evictions++;
            writeLock.unlock();
            lock.lock();
        }
    }

    statistics_.cacheMisses++;
    lock.unlock();

    // Load and compile script
    std::string source = readFile(filename);
    if (source.empty()) {
        return nullptr;
    }

    if (language == ScriptLanguage::Auto) {
        language =
            ComponentScriptingAPI::instance().detectLanguage(filename, true);
    }

    auto compiled = compileScript(source, filename, language);
    if (!compiled || !compiled->isValid) {
        return compiled;
    }

    // Cache the compiled script
    if (config_.enableCaching) {
        std::unique_lock writeLock(cacheMutex_);

        // Evict old entries if cache is full
        if (cache_.size() >= config_.maxCacheSize) {
            evictOldEntries();
        }

        ScriptCacheEntry entry;
        entry.compiledScript = compiled;
        entry.lastModified = std::filesystem::last_write_time(filename);
        entry.lastAccessed = std::chrono::system_clock::now();
        entry.accessCount = 1;

        cache_[filename] = std::move(entry);
        statistics_.totalEntries = cache_.size();
    }

    return compiled;
}

std::shared_ptr<CompiledScript> ScriptLoader::compileScript(
    const std::string& source, const std::string& filename,
    ScriptLanguage language) {
    if (language == ScriptLanguage::Auto) {
        language =
            ComponentScriptingAPI::instance().detectLanguage(source, false);
    }

    return compileForLanguage(source, filename, language);
}

bool ScriptLoader::precompileScript(const std::string& filename,
                                    ScriptLanguage language) {
    auto compiled = loadScript(filename, language);
    return compiled && compiled->isValid;
}

void ScriptLoader::clearCache(bool force) {
    std::unique_lock lock(cacheMutex_);

    if (force) {
        cache_.clear();
    } else {
        // Only remove non-pinned entries
        auto it = cache_.begin();
        while (it != cache_.end()) {
            if (!it->second.isPinned) {
                it = cache_.erase(it);
                statistics_.evictions++;
            } else {
                ++it;
            }
        }
    }

    statistics_.totalEntries = cache_.size();
    statistics_.pinnedEntries =
        std::count_if(cache_.begin(), cache_.end(),
                      [](const auto& pair) { return pair.second.isPinned; });
}

ScriptLoader::CacheStatistics ScriptLoader::getCacheStatistics() const {
    std::shared_lock lock(cacheMutex_);

    CacheStatistics stats = statistics_;
    stats.totalEntries = cache_.size();
    stats.pinnedEntries =
        std::count_if(cache_.begin(), cache_.end(),
                      [](const auto& pair) { return pair.second.isPinned; });

    // Calculate memory usage
    stats.totalMemoryUsage = 0;
    for (const auto& [filename, entry] : cache_) {
        stats.totalMemoryUsage += entry.compiledScript->source.size();
        stats.totalMemoryUsage += entry.compiledScript->bytecode.size();
        stats.totalMemoryUsage += filename.size();
    }

    // Calculate hit ratio
    uint64_t totalAccesses = stats.cacheHits + stats.cacheMisses;
    stats.hitRatio = totalAccesses > 0
                         ? static_cast<double>(stats.cacheHits) / totalAccesses
                         : 0.0;

    return stats;
}

void ScriptLoader::pinScript(const std::string& filename) {
    std::unique_lock lock(cacheMutex_);
    auto it = cache_.find(filename);
    if (it != cache_.end()) {
        it->second.isPinned = true;
    }
}

void ScriptLoader::unpinScript(const std::string& filename) {
    std::unique_lock lock(cacheMutex_);
    auto it = cache_.find(filename);
    if (it != cache_.end()) {
        it->second.isPinned = false;
    }
}

std::string ScriptLoader::readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return "";
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

size_t ScriptLoader::calculateHash(const std::string& source) {
    return std::hash<std::string>{}(source);
}

void ScriptLoader::evictOldEntries() {
    if (cache_.empty())
        return;

    // Find the oldest non-pinned entry
    auto oldestIt = cache_.end();
    std::chrono::system_clock::time_point oldestTime =
        std::chrono::system_clock::now();

    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (!it->second.isPinned && it->second.lastAccessed < oldestTime) {
            oldestTime = it->second.lastAccessed;
            oldestIt = it;
        }
    }

    if (oldestIt != cache_.end()) {
        cache_.erase(oldestIt);
        statistics_.evictions++;
    }
}

std::shared_ptr<CompiledScript> ScriptLoader::compileForLanguage(
    const std::string& source, const std::string& filename,
    ScriptLanguage language) {
    auto compiled = std::make_shared<CompiledScript>();
    compiled->source = source;
    compiled->filename = filename;
    compiled->sourceHash = calculateHash(source);
    compiled->compilationTime = std::chrono::system_clock::now();

    const auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // In a real implementation, this would use actual script engine
        // compilation For now, we'll simulate compilation
        switch (language) {
            case ScriptLanguage::Lua:
                // Simulate Lua compilation
                compiled->bytecode.assign(source.begin(), source.end());
                compiled->isValid = true;
                break;

            case ScriptLanguage::Python:
                // Simulate Python compilation
                compiled->bytecode.assign(source.begin(), source.end());
                compiled->isValid = true;
                break;

            default:
                compiled->isValid = false;
                compiled->errorMessage = "Unsupported script language";
                break;
        }

        // Generate debug information if enabled
        if (config_.enableDebugInfo && compiled->isValid) {
            std::istringstream sourceStream(source);
            std::string line;
            size_t lineNumber = 1;

            while (std::getline(sourceStream, line)) {
                compiled->debugInfo.lineNumbers.push_back(lineNumber);
                compiled->debugInfo.sourceLines[lineNumber] = line;
                lineNumber++;
            }
        }

    } catch (const std::exception& e) {
        compiled->isValid = false;
        compiled->errorMessage = "Compilation error: " + std::string(e.what());
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    compiled->compilationDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                              startTime);

    return compiled;
}

// ScriptExecutor implementation
ScriptExecutor::ScriptExecutor(std::shared_ptr<IScriptEngine> engine)
    : engine_(std::move(engine)), config_({}) {}

ScriptExecutor::ScriptExecutor(std::shared_ptr<IScriptEngine> engine,
                               const Config& config)
    : engine_(std::move(engine)), config_(config) {}

ScriptResult ScriptExecutor::execute(const CompiledScript& script,
                                     ExecutionContext& context) {
    const auto startTime = std::chrono::high_resolution_clock::now();

    ScriptResult result;
    context.startTime = std::chrono::steady_clock::now();

    try {
        if (!script.isValid) {
            result.success = false;
            result.errorMessage =
                "Invalid compiled script: " + script.errorMessage;
            return result;
        }

        // Set up execution context
        context.scriptName = script.filename;
        context.currentLine = 1;

        // Execute the script using the engine
        if (engine_) {
            // Convert bytecode back to source for execution (simplified)
            std::string sourceCode(script.bytecode.begin(),
                                   script.bytecode.end());
            result = engine_->executeScript(sourceCode, script.filename);
        } else {
            result.success = false;
            result.errorMessage = "No script engine available";
        }

        statistics_.totalExecutions++;
        if (result.success) {
            statistics_.successfulExecutions++;
        } else {
            statistics_.failedExecutions++;
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = "Execution error: " + std::string(e.what());
        statistics_.failedExecutions++;
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTime =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                              startTime);

    updateStatistics(result);

    return result;
}

ScriptResult ScriptExecutor::executeFunction(
    const CompiledScript& script, const std::string& functionName,
    const std::vector<ScriptValue>& args, ExecutionContext& context) {
    ScriptResult result;

    try {
        if (!script.isValid) {
            result.success = false;
            result.errorMessage =
                "Invalid compiled script: " + script.errorMessage;
            return result;
        }

        // Set up execution context
        context.scriptName = script.filename;
        context.callStack.push_back(functionName);

        // Execute the function using the engine
        if (engine_) {
            result = engine_->callFunction(functionName, args);
        } else {
            result.success = false;
            result.errorMessage = "No script engine available";
        }

        // Update function call statistics
        statistics_.functionCallCounts[functionName]++;

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage =
            "Function execution error: " + std::string(e.what());
    }

    updateStatistics(result);

    return result;
}

void ScriptExecutor::setBreakpoint(const std::string& filename, size_t line) {
    std::lock_guard lock(breakpointsMutex_);
    breakpoints_[filename].push_back(line);
}

void ScriptExecutor::removeBreakpoint(const std::string& filename,
                                      size_t line) {
    std::lock_guard lock(breakpointsMutex_);
    auto it = breakpoints_.find(filename);
    if (it != breakpoints_.end()) {
        auto& lines = it->second;
        lines.erase(std::remove(lines.begin(), lines.end(), line), lines.end());
        if (lines.empty()) {
            breakpoints_.erase(it);
        }
    }
}

void ScriptExecutor::clearBreakpoints() {
    std::lock_guard lock(breakpointsMutex_);
    breakpoints_.clear();
}

bool ScriptExecutor::checkTimeout(const ExecutionContext& context) {
    auto elapsed = std::chrono::steady_clock::now() - context.startTime;
    return elapsed > context.timeout;
}

void ScriptExecutor::updateStatistics(const ScriptResult& result) {
    statistics_.totalExecutionTime += result.executionTime;

    if (statistics_.totalExecutions > 0) {
        statistics_.averageExecutionTime =
            std::chrono::microseconds{statistics_.totalExecutionTime.count() /
                                      statistics_.totalExecutions};
    }

    if (result.memoryUsed > statistics_.peakMemoryUsage) {
        statistics_.peakMemoryUsage = result.memoryUsed;
    }
}

void ScriptExecutor::handleBreakpoint(ExecutionContext& context) {
    if (config_.debugCallback) {
        config_.debugCallback(context);
    }
}

// ScriptManager implementation
ScriptManager& ScriptManager::instance() {
    static ScriptManager instance;
    return instance;
}

bool ScriptManager::initialize(const ScriptLoader::Config& loaderConfig,
                               const ScriptExecutor::Config& executorConfig) {
    if (initialized_) {
        return true;
    }

    loader_ = std::make_unique<ScriptLoader>(loaderConfig);

    // Create a default script engine (would be replaced with actual engine)
    engine_ =
        ComponentScriptingAPI::instance().createEngine(ScriptLanguage::Lua);
    if (!engine_) {
        return false;
    }

    executor_ = std::make_unique<ScriptExecutor>(engine_, executorConfig);

    initialized_ = true;
    return true;
}

ScriptResult ScriptManager::executeFile(const std::string& filename,
                                        ScriptLanguage language) {
    if (!initialized_) {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Script manager not initialized";
        return result;
    }

    auto compiled = loader_->loadScript(filename, language);
    if (!compiled || !compiled->isValid) {
        ScriptResult result;
        result.success = false;
        result.errorMessage =
            compiled ? compiled->errorMessage : "Failed to load script";
        return result;
    }

    ExecutionContext context;
    return executor_->execute(*compiled, context);
}

ScriptResult ScriptManager::executeSource(const std::string& source,
                                          const std::string& filename,
                                          ScriptLanguage language) {
    if (!initialized_) {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Script manager not initialized";
        return result;
    }

    auto compiled = loader_->compileScript(source, filename, language);
    if (!compiled || !compiled->isValid) {
        ScriptResult result;
        result.success = false;
        result.errorMessage =
            compiled ? compiled->errorMessage : "Failed to compile script";
        return result;
    }

    ExecutionContext context;
    return executor_->execute(*compiled, context);
}

ScriptResult ScriptManager::callFunction(const std::string& filename,
                                         const std::string& functionName,
                                         const std::vector<ScriptValue>& args) {
    if (!initialized_) {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Script manager not initialized";
        return result;
    }

    auto compiled = loader_->loadScript(filename);
    if (!compiled || !compiled->isValid) {
        ScriptResult result;
        result.success = false;
        result.errorMessage =
            compiled ? compiled->errorMessage : "Failed to load script";
        return result;
    }

    ExecutionContext context;
    return executor_->executeFunction(*compiled, functionName, args, context);
}

void ScriptManager::shutdown() {
    executor_.reset();
    loader_.reset();
    engine_.reset();
    initialized_ = false;
}

}  // namespace atom::components::scripting
