/*
 * script_sandbox.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "script_sandbox.hpp"

#include <algorithm>
#include <filesystem>
// #include <regex> // removed unused include

namespace atom::components::scripting {

// ResourceMonitor implementation
ResourceMonitor::ResourceMonitor(const ResourceLimits& limits)
    : limits_(limits) {}

void ResourceMonitor::startMonitoring(const std::string& scriptName) {
    std::unique_lock lock(usageMutex_);
    usage_[scriptName] = Usage{};

    if (!monitoring_.exchange(true)) {
        monitorThread_ = std::thread(&ResourceMonitor::monitorLoop, this);
    }
}

void ResourceMonitor::stopMonitoring(const std::string& scriptName) {
    std::unique_lock lock(usageMutex_);
    usage_.erase(scriptName);

    if (usage_.empty() && monitoring_.exchange(false)) {
        lock.unlock();
        if (monitorThread_.joinable()) {
            monitorThread_.join();
        }
    }
}

bool ResourceMonitor::checkLimits(const std::string& scriptName) {
    std::shared_lock lock(usageMutex_);
    auto it = usage_.find(scriptName);
    if (it == usage_.end())
        return true;

    const auto& usage = it->second;

    // Check memory limit
    if (usage.memoryUsage.load() > limits_.maxMemoryUsage) {
        return false;
    }

    // Check execution time limit
    if (usage.executionTime.load() > limits_.maxExecutionTime) {
        return false;
    }

    // Check stack depth limit
    if (usage.stackDepth.load() > limits_.maxStackDepth) {
        return false;
    }

    // Check file limit
    if (usage.openFiles.load() > limits_.maxOpenFiles) {
        return false;
    }

    // Check network connections limit
    if (usage.networkConnections.load() > limits_.maxNetworkConnections) {
        return false;
    }

    // Check thread limit
    if (usage.activeThreads.load() > limits_.maxThreads) {
        return false;
    }

    // Check CPU usage limit
    if (usage.cpuUsage.load() > limits_.maxCpuUsage) {
        return false;
    }

    return true;
}

ResourceMonitor::Usage ResourceMonitor::getCurrentUsage(
    const std::string& scriptName) const {
    std::shared_lock lock(usageMutex_);
    auto it = usage_.find(scriptName);
    return it != usage_.end() ? it->second : Usage{};
}

void ResourceMonitor::updateMemoryUsage(const std::string& scriptName,
                                        size_t bytes) {
    std::shared_lock lock(usageMutex_);
    auto it = usage_.find(scriptName);
    if (it != usage_.end()) {
        it->second.memoryUsage.store(bytes);

        // Update peak memory usage
        size_t current = bytes;
        size_t peak = it->second.peakMemoryUsage.load();
        while (
            current > peak &&
            !it->second.peakMemoryUsage.compare_exchange_weak(peak, current)) {
            // Retry if another thread updated peak
        }
    }
}

void ResourceMonitor::updateExecutionTime(const std::string& scriptName,
                                          std::chrono::milliseconds time) {
    std::shared_lock lock(usageMutex_);
    auto it = usage_.find(scriptName);
    if (it != usage_.end()) {
        it->second.executionTime.store(time);
    }
}

void ResourceMonitor::incrementStackDepth(const std::string& scriptName) {
    std::shared_lock lock(usageMutex_);
    auto it = usage_.find(scriptName);
    if (it != usage_.end()) {
        it->second.stackDepth.fetch_add(1);
    }
}

void ResourceMonitor::decrementStackDepth(const std::string& scriptName) {
    std::shared_lock lock(usageMutex_);
    auto it = usage_.find(scriptName);
    if (it != usage_.end()) {
        it->second.stackDepth.fetch_sub(1);
    }
}

void ResourceMonitor::monitorLoop() {
    while (monitoring_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::shared_lock lock(usageMutex_);
        for (auto& [scriptName, usage] : usage_) {
            // Update CPU usage (simplified - would use platform-specific APIs)
            // For now, just simulate CPU monitoring
            usage.cpuUsage.store(0.1);  // 10% CPU usage simulation
        }
    }
}

// PermissionManager implementation
PermissionManager::PermissionManager(const SandboxConfig& config)
    : config_(config) {}

bool PermissionManager::checkPermission(const std::string& scriptName,
                                        Permission permission,
                                        const std::string& context) {
    std::shared_lock lock(permissionsMutex_);

    auto it = scriptPermissions_.find(scriptName);
    Permission granted =
        it != scriptPermissions_.end() ? it->second : config_.permissions;

    if (!hasPermission(granted, permission)) {
        return false;
    }

    // Additional context-specific checks
    if (permission == Permission::ReadFiles ||
        permission == Permission::WriteFiles) {
        return isPathAllowed(scriptName, context,
                             permission == Permission::WriteFiles);
    }

    if (permission == Permission::ExecuteCommands) {
        return isFunctionAllowed(scriptName, context);
    }

    return true;
}

void PermissionManager::grantPermission(const std::string& scriptName,
                                        Permission permission) {
    std::unique_lock lock(permissionsMutex_);
    scriptPermissions_[scriptName] =
        scriptPermissions_[scriptName] | permission;
}

void PermissionManager::revokePermission(const std::string& scriptName,
                                         Permission permission) {
    std::unique_lock lock(permissionsMutex_);
    auto it = scriptPermissions_.find(scriptName);
    if (it != scriptPermissions_.end()) {
        it->second =
            static_cast<Permission>(static_cast<uint32_t>(it->second) &
                                    ~static_cast<uint32_t>(permission));
    }
}

bool PermissionManager::isFunctionAllowed(const std::string& /*scriptName*/,
                                          const std::string& functionName) {
    // Check blocked functions first
    if (std::find(config_.blockedFunctions.begin(),
                  config_.blockedFunctions.end(),
                  functionName) != config_.blockedFunctions.end()) {
        return false;
    }

    // If allowed functions list is not empty, check if function is in it
    if (!config_.allowedFunctions.empty()) {
        return std::find(config_.allowedFunctions.begin(),
                         config_.allowedFunctions.end(),
                         functionName) != config_.allowedFunctions.end();
    }

    return true;
}

bool PermissionManager::isPathAllowed(const std::string& /*scriptName*/,
                                      const std::string& path, bool /*write*/) {
    std::filesystem::path normalizedPath =
        std::filesystem::weakly_canonical(path);
    std::string pathStr = normalizedPath.string();

    // Check blocked paths first
    if (isPathInBlockedList(pathStr)) {
        return false;
    }

    // Check allowed paths
    if (!config_.allowedPaths.empty()) {
        return isPathInAllowedList(pathStr);
    }

    return true;
}

bool PermissionManager::isPathInAllowedList(const std::string& path) const {
    for (const auto& allowedPath : config_.allowedPaths) {
        if (path.find(allowedPath) == 0) {  // Path starts with allowed path
            return true;
        }
    }
    return false;
}

bool PermissionManager::isPathInBlockedList(const std::string& path) const {
    for (const auto& blockedPath : config_.blockedPaths) {
        if (path.find(blockedPath) == 0) {  // Path starts with blocked path
            return true;
        }
    }
    return false;
}

// ScriptSandbox implementation
ScriptSandbox::ScriptSandbox(const SandboxConfig& config) : config_(config) {
    resourceMonitor_ = std::make_unique<ResourceMonitor>(config_.limits);
    permissionManager_ = std::make_unique<PermissionManager>(config_);
}

ScriptSandbox::~ScriptSandbox() { shutdown(); }

bool ScriptSandbox::initialize() {
    if (initialized_) {
        return true;
    }

    initialized_ = true;
    return true;
}

std::unique_ptr<IScriptEngine> ScriptSandbox::createSandboxedEngine(
    ScriptLanguage language, const std::string& scriptName) {
    if (!initialized_) {
        return nullptr;
    }

    // Create underlying engine
    auto engine = ComponentScriptingAPI::instance().createEngine(language);
    if (!engine) {
        return nullptr;
    }

    // Wrap in sandboxed engine
    auto sandboxedEngine = std::make_unique<SandboxedScriptEngine>(
        std::move(engine), this, scriptName);

    // Setup sandbox restrictions
    setupSandboxedEngine(*sandboxedEngine, scriptName);

    return sandboxedEngine;  // NRVO, avoid redundant std::move warning
}

ScriptResult ScriptSandbox::executeInSandbox(const std::string& script,
                                             const std::string& scriptName,
                                             bool isFile,
                                             ScriptLanguage language) {
    if (!initialized_) {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Sandbox not initialized";
        return result;
    }

    // Start resource monitoring
    resourceMonitor_->startMonitoring(scriptName);

    {
        std::unique_lock lock(activeScriptsMutex_);
        activeScripts_.insert(scriptName);
    }

    const auto startTime = std::chrono::high_resolution_clock::now();

    // Create sandboxed engine
    auto engine = createSandboxedEngine(language, scriptName);
    if (!engine) {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Failed to create sandboxed engine";
        return result;
    }

    // Execute script
    ScriptResult result;
    try {
        if (isFile) {
            result = engine->executeFile(script);
        } else {
            result = engine->executeScript(script);
        }

        statistics_.totalExecutions++;
        if (result.success) {
            statistics_.successfulExecutions++;
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage =
            "Sandbox execution error: " + std::string(e.what());

        // Record violation
        SandboxViolation violation;
        violation.type = SandboxViolation::SecurityViolation;
        violation.description = result.errorMessage;
        violation.scriptName = scriptName;
        violation.timestamp = std::chrono::system_clock::now();
        recordViolation(violation);
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    const auto executionTime =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                              startTime);
    statistics_.totalExecutionTime += executionTime;

    // Update resource usage
    resourceMonitor_->updateExecutionTime(
        scriptName,
        std::chrono::duration_cast<std::chrono::milliseconds>(executionTime));

    // Cleanup
    {
        std::unique_lock lock(activeScriptsMutex_);
        activeScripts_.erase(scriptName);
    }

    resourceMonitor_->stopMonitoring(scriptName);

    return result;
}

bool ScriptSandbox::terminateScript(const std::string& scriptName) {
    std::shared_lock lock(activeScriptsMutex_);
    if (activeScripts_.find(scriptName) == activeScripts_.end()) {
        return false;  // Script not running
    }

    // In a real implementation, this would forcefully terminate the script
    // For now, just remove from active scripts
    lock.unlock();
    std::unique_lock writeLock(activeScriptsMutex_);
    activeScripts_.erase(scriptName);

    statistics_.scriptsTerminated++;
    return true;
}

std::vector<SandboxViolation> ScriptSandbox::getRecentViolations(
    size_t limit) const {
    std::lock_guard lock(violationsMutex_);

    std::vector<SandboxViolation> recent;
    size_t count = std::min(limit, violations_.size());

    if (count > 0) {
        recent.assign(violations_.end() - count, violations_.end());
    }

    return recent;
}

void ScriptSandbox::updateConfig(const SandboxConfig& config) {
    config_ = config;
    resourceMonitor_ = std::make_unique<ResourceMonitor>(config_.limits);
    permissionManager_ = std::make_unique<PermissionManager>(config_);
}

void ScriptSandbox::shutdown() {
    if (!initialized_)
        return;

    // Terminate all active scripts
    std::unique_lock lock(activeScriptsMutex_);
    for (const auto& scriptName : activeScripts_) {
        resourceMonitor_->stopMonitoring(scriptName);
    }
    activeScripts_.clear();
    lock.unlock();

    initialized_ = false;
}

void ScriptSandbox::recordViolation(const SandboxViolation& violation) {
    std::lock_guard lock(violationsMutex_);

    violations_.push_back(violation);
    statistics_.violationsDetected++;
    statistics_.violationsByType[violation.type]++;

    // Limit violation history size
    if (violations_.size() > 1000) {
        violations_.erase(violations_.begin(), violations_.begin() + 100);
    }

    // Call violation callback if configured
    if (config_.violationCallback) {
        config_.violationCallback(violation);
    }
}

void ScriptSandbox::setupSandboxedEngine(IScriptEngine& engine,
                                         const std::string& /*scriptName*/) {
    // Register sandboxed API functions
    ComponentScriptingAPI::instance().registerComponentAPI(engine);

    // The actual sandboxing would be implemented in the SandboxedScriptEngine
    // wrapper
}

ScriptFunction ScriptSandbox::createSandboxedFunction(
    const std::string& functionName, const std::string& scriptName,
    ScriptFunction originalFunction) {
    return [this, functionName, scriptName, originalFunction](
               const std::vector<ScriptValue>& args) -> ScriptValue {
        // Check permissions before executing
        if (!permissionManager_->isFunctionAllowed(scriptName, functionName)) {
            SandboxViolation violation;
            violation.type = SandboxViolation::PermissionDenied;
            violation.description =
                "Function call not allowed: " + functionName;
            violation.scriptName = scriptName;
            violation.timestamp = std::chrono::system_clock::now();
            recordViolation(violation);

            return ScriptValue();  // Return null/empty value
        }

        // Check resource limits before execution
        if (!resourceMonitor_->checkLimits(scriptName)) {
            SandboxViolation violation;
            violation.type = SandboxViolation::ResourceLimit;
            violation.description =
                "Resource limit exceeded for function: " + functionName;
            violation.scriptName = scriptName;
            violation.timestamp = std::chrono::system_clock::now();
            recordViolation(violation);

            return ScriptValue();
        }

        // Execute original function
        return originalFunction(args);
    };
}

// SandboxedScriptEngine implementation
SandboxedScriptEngine::SandboxedScriptEngine(
    std::unique_ptr<IScriptEngine> engine, ScriptSandbox* sandbox,
    const std::string& scriptName)
    : engine_(std::move(engine)), sandbox_(sandbox), scriptName_(scriptName) {}

bool SandboxedScriptEngine::initialize(const ScriptEngineConfig& config) {
    return engine_->initialize(config);
}

ScriptResult SandboxedScriptEngine::executeScript(const std::string& script,
                                                  const std::string& context) {
    if (!checkPermissionForOperation("execute_script", context)) {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Permission denied for script execution";
        return result;
    }

    return engine_->executeScript(script, context);
}

ScriptResult SandboxedScriptEngine::executeFile(const std::string& filename) {
    if (!checkPermissionForOperation("execute_file", filename)) {
        ScriptResult result;
        result.success = false;
        result.errorMessage =
            "Permission denied for file execution: " + filename;
        return result;
    }

    return engine_->executeFile(filename);
}

ScriptResult SandboxedScriptEngine::callFunction(
    const std::string& functionName, const std::vector<ScriptValue>& args) {
    if (!checkPermissionForOperation("call_function", functionName)) {
        ScriptResult result;
        result.success = false;
        result.errorMessage =
            "Permission denied for function call: " + functionName;
        return result;
    }

    return engine_->callFunction(functionName, args);
}

void SandboxedScriptEngine::setGlobal(const std::string& name,
                                      const ScriptValue& value) {
    if (checkPermissionForOperation("set_global", name)) {
        engine_->setGlobal(name, value);
    }
}

std::optional<ScriptValue> SandboxedScriptEngine::getGlobal(
    const std::string& name) {
    if (!checkPermissionForOperation("get_global", name)) {
        return std::nullopt;
    }

    return engine_->getGlobal(name);
}

void SandboxedScriptEngine::registerFunction(const std::string& name,
                                             ScriptFunction function) {
    // Wrap function with sandbox checks
    auto wrappedFunction = wrapFunction(name, function);
    engine_->registerFunction(name, wrappedFunction);
}

ScriptLanguage SandboxedScriptEngine::getLanguage() const {
    return engine_->getLanguage();
}

const IScriptEngine::Statistics& SandboxedScriptEngine::getStatistics() const {
    return engine_->getStatistics();
}

void SandboxedScriptEngine::resetStatistics() { engine_->resetStatistics(); }

void SandboxedScriptEngine::shutdown() { engine_->shutdown(); }

ScriptValue SandboxedScriptEngine::cppToScript(const std::any& /*value*/) {
    // Since we can't call protected methods on another instance,
    // we need to delegate this to the wrapped engine through a different
    // mechanism For now, return a default value - this needs proper
    // implementation
    return ScriptValue{};
}

std::any SandboxedScriptEngine::scriptToCpp(
    const ScriptValue& /*value*/, const std::type_info& /*targetType*/) {
    // Since we can't call protected methods on another instance,
    // we need to delegate this to the wrapped engine through a different
    // mechanism For now, return a default value - this needs proper
    // implementation
    return std::any{};
}

bool SandboxedScriptEngine::checkPermissionForOperation(
    const std::string& operation, const std::string& context) {
    // Map operations to permissions
    Permission required = Permission::None;

    if (operation == "execute_script" || operation == "execute_file") {
        required = Permission::ComponentAccess;
    } else if (operation == "call_function") {
        required = Permission::ExecuteCommands;
    } else if (operation == "set_global" || operation == "get_global") {
        required = Permission::MemoryAccess;
    }

    return sandbox_->permissionManager_->checkPermission(scriptName_, required,
                                                         context);
}

ScriptFunction SandboxedScriptEngine::wrapFunction(const std::string& name,
                                                   ScriptFunction function) {
    return sandbox_->createSandboxedFunction(name, scriptName_, function);
}

}  // namespace atom::components::scripting
