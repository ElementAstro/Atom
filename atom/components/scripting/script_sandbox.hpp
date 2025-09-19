/*
 * script_sandbox.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Script Sandbox Environment
Provides secure script execution with resource limits,
permission system, and isolation for safe script execution.

**************************************************/

#ifndef ATOM_COMPONENT_SCRIPT_SANDBOX_HPP
#define ATOM_COMPONENT_SCRIPT_SANDBOX_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "scripting_api.hpp"

namespace atom::components::scripting {

/**
 * @brief Security permission levels
 */
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

inline Permission operator|(Permission a, Permission b) {
    return static_cast<Permission>(static_cast<uint32_t>(a) |
                                   static_cast<uint32_t>(b));
}

inline Permission operator&(Permission a, Permission b) {
    return static_cast<Permission>(static_cast<uint32_t>(a) &
                                   static_cast<uint32_t>(b));
}

inline bool hasPermission(Permission granted, Permission required) {
    return (granted & required) == required;
}

/**
 * @brief Resource limits for script execution
 */
struct ResourceLimits {
    size_t maxMemoryUsage = 64 * 1024 * 1024;           // 64MB
    std::chrono::milliseconds maxExecutionTime{30000};  // 30 seconds
    size_t maxStackDepth = 1000;
    size_t maxFileSize = 10 * 1024 * 1024;  // 10MB
    size_t maxOpenFiles = 100;
    size_t maxNetworkConnections = 10;
    size_t maxThreads = 4;
    double maxCpuUsage = 0.8;  // 80% CPU usage
};

/**
 * @brief Sandbox violation information
 */
struct SandboxViolation {
    enum Type {
        MemoryLimit,
        TimeLimit,
        PermissionDenied,
        ResourceLimit,
        SecurityViolation
    };

    Type type;
    std::string description;
    std::string scriptName;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> context;
};

/**
 * @brief Sandbox configuration
 */
struct SandboxConfig {
    Permission permissions = Permission::ComponentAccess;
    ResourceLimits limits;
    bool enableLogging = true;
    bool enableProfiling = false;
    bool strictMode = true;  // Fail on any violation
    std::vector<std::string> allowedPaths;
    std::vector<std::string> blockedPaths;
    std::vector<std::string> allowedFunctions;
    std::vector<std::string> blockedFunctions;
    std::function<void(const SandboxViolation&)> violationCallback;
};

/**
 * @brief Resource monitor for tracking script resource usage
 */
class ResourceMonitor {
public:
    struct Usage {
        std::atomic<size_t> memoryUsage{0};
        std::atomic<size_t> peakMemoryUsage{0};
        std::atomic<std::chrono::milliseconds> executionTime{
            std::chrono::milliseconds{0}};
        std::atomic<size_t> stackDepth{0};
        std::atomic<size_t> openFiles{0};
        std::atomic<size_t> networkConnections{0};
        std::atomic<size_t> activeThreads{0};
        std::atomic<double> cpuUsage{0.0};

        // Default constructor
        Usage() = default;

        // Copy constructor
        Usage(const Usage& other)
            : memoryUsage(other.memoryUsage.load()),
              peakMemoryUsage(other.peakMemoryUsage.load()),
              executionTime(other.executionTime.load()),
              stackDepth(other.stackDepth.load()),
              openFiles(other.openFiles.load()),
              networkConnections(other.networkConnections.load()),
              activeThreads(other.activeThreads.load()),
              cpuUsage(other.cpuUsage.load()) {}

        // Assignment operator
        Usage& operator=(const Usage& other) {
            if (this != &other) {
                memoryUsage.store(other.memoryUsage.load());
                peakMemoryUsage.store(other.peakMemoryUsage.load());
                executionTime.store(other.executionTime.load());
                stackDepth.store(other.stackDepth.load());
                openFiles.store(other.openFiles.load());
                networkConnections.store(other.networkConnections.load());
                activeThreads.store(other.activeThreads.load());
                cpuUsage.store(other.cpuUsage.load());
            }
            return *this;
        }
    };

    /**
     * @brief Constructs resource monitor with limits
     * @param limits Resource limits
     */
    explicit ResourceMonitor(const ResourceLimits& limits);

    /**
     * @brief Starts monitoring for a script
     * @param scriptName Script identifier
     */
    void startMonitoring(const std::string& scriptName);

    /**
     * @brief Stops monitoring for a script
     * @param scriptName Script identifier
     */
    void stopMonitoring(const std::string& scriptName);

    /**
     * @brief Checks if resource usage is within limits
     * @param scriptName Script identifier
     * @return True if within limits
     */
    bool checkLimits(const std::string& scriptName);

    /**
     * @brief Gets current usage for a script
     * @param scriptName Script identifier
     * @return Current resource usage
     */
    Usage getCurrentUsage(const std::string& scriptName) const;

    /**
     * @brief Updates memory usage
     * @param scriptName Script identifier
     * @param bytes Memory usage in bytes
     */
    void updateMemoryUsage(const std::string& scriptName, size_t bytes);

    /**
     * @brief Updates execution time
     * @param scriptName Script identifier
     * @param time Execution time
     */
    void updateExecutionTime(const std::string& scriptName,
                             std::chrono::milliseconds time);

    /**
     * @brief Increments stack depth
     * @param scriptName Script identifier
     */
    void incrementStackDepth(const std::string& scriptName);

    /**
     * @brief Decrements stack depth
     * @param scriptName Script identifier
     */
    void decrementStackDepth(const std::string& scriptName);

private:
    ResourceLimits limits_;
    std::unordered_map<std::string, Usage> usage_;
    mutable std::shared_mutex usageMutex_;
    std::thread monitorThread_;
    std::atomic<bool> monitoring_{false};

    void monitorLoop();
};

/**
 * @brief Permission manager for controlling script access
 */
class PermissionManager {
public:
    /**
     * @brief Constructs permission manager
     * @param config Sandbox configuration
     */
    explicit PermissionManager(const SandboxConfig& config);

    /**
     * @brief Checks if a script has permission for an operation
     * @param scriptName Script identifier
     * @param permission Required permission
     * @param context Additional context (e.g., file path)
     * @return True if permission granted
     */
    bool checkPermission(const std::string& scriptName, Permission permission,
                         const std::string& context = "");

    /**
     * @brief Grants permission to a script
     * @param scriptName Script identifier
     * @param permission Permission to grant
     */
    void grantPermission(const std::string& scriptName, Permission permission);

    /**
     * @brief Revokes permission from a script
     * @param scriptName Script identifier
     * @param permission Permission to revoke
     */
    void revokePermission(const std::string& scriptName, Permission permission);

    /**
     * @brief Checks if a function call is allowed
     * @param scriptName Script identifier
     * @param functionName Function name
     * @return True if allowed
     */
    bool isFunctionAllowed(const std::string& scriptName,
                           const std::string& functionName);

    /**
     * @brief Checks if a path access is allowed
     * @param scriptName Script identifier
     * @param path File path
     * @param write True for write access, false for read
     * @return True if allowed
     */
    bool isPathAllowed(const std::string& scriptName, const std::string& path,
                       bool write = false);

private:
    SandboxConfig config_;
    std::unordered_map<std::string, Permission> scriptPermissions_;
    mutable std::shared_mutex permissionsMutex_;

    bool isPathInAllowedList(const std::string& path) const;
    bool isPathInBlockedList(const std::string& path) const;
};

/**
 * @brief Main sandbox environment for secure script execution
 */
class ScriptSandbox {
public:
    /**
     * @brief Constructs sandbox with configuration
     * @param config Sandbox configuration
     */
    explicit ScriptSandbox(const SandboxConfig& config = {});

    /**
     * @brief Destructor
     */
    ~ScriptSandbox();

    /**
     * @brief Initializes the sandbox
     * @return True if successful
     */
    bool initialize();

    /**
     * @brief Creates a sandboxed script engine
     * @param language Script language
     * @param scriptName Script identifier
     * @return Sandboxed script engine
     */
    std::unique_ptr<IScriptEngine> createSandboxedEngine(
        ScriptLanguage language, const std::string& scriptName);

    /**
     * @brief Executes a script in the sandbox
     * @param script Script source or filename
     * @param scriptName Script identifier
     * @param isFile True if script parameter is a filename
     * @param language Script language
     * @return Execution result
     */
    ScriptResult executeInSandbox(
        const std::string& script, const std::string& scriptName,
        bool isFile = false, ScriptLanguage language = ScriptLanguage::Auto);

    /**
     * @brief Terminates a running script
     * @param scriptName Script identifier
     * @return True if terminated successfully
     */
    bool terminateScript(const std::string& scriptName);

    /**
     * @brief Gets sandbox statistics
     * @return Sandbox statistics
     */
    struct Statistics {
        uint64_t totalExecutions = 0;
        uint64_t successfulExecutions = 0;
        uint64_t violationsDetected = 0;
        uint64_t scriptsTerminated = 0;
        std::chrono::microseconds totalExecutionTime{0};
        std::unordered_map<SandboxViolation::Type, uint64_t> violationsByType;
    };

    const Statistics& getStatistics() const { return statistics_; }
    void resetStatistics() { statistics_ = Statistics{}; }

    /**
     * @brief Gets recent violations
     * @param limit Maximum number of violations to return
     * @return Vector of recent violations
     */
    std::vector<SandboxViolation> getRecentViolations(size_t limit = 100) const;

    /**
     * @brief Updates sandbox configuration
     * @param config New configuration
     */
    void updateConfig(const SandboxConfig& config);

    /**
     * @brief Shutdown the sandbox
     */
    void shutdown();

protected:
    std::unique_ptr<PermissionManager> permissionManager_;

    // Helper methods
    ScriptFunction createSandboxedFunction(const std::string& functionName,
                                           const std::string& scriptName,
                                           ScriptFunction originalFunction);

private:
    SandboxConfig config_;
    std::unique_ptr<ResourceMonitor> resourceMonitor_;
    Statistics statistics_;
    std::vector<SandboxViolation> violations_;
    mutable std::mutex violationsMutex_;
    std::unordered_set<std::string> activeScripts_;
    mutable std::shared_mutex activeScriptsMutex_;
    bool initialized_ = false;

    void recordViolation(const SandboxViolation& violation);
    void setupSandboxedEngine(IScriptEngine& engine,
                              const std::string& scriptName);

    friend class SandboxedScriptEngine;
};

/**
 * @brief Sandboxed script engine wrapper
 */
class SandboxedScriptEngine : public IScriptEngine {
public:
    /**
     * @brief Constructs sandboxed engine wrapper
     * @param engine Underlying script engine
     * @param sandbox Sandbox instance
     * @param scriptName Script identifier
     */
    SandboxedScriptEngine(std::unique_ptr<IScriptEngine> engine,
                          ScriptSandbox* sandbox,
                          const std::string& scriptName);

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
    ScriptLanguage getLanguage() const override;
    const Statistics& getStatistics() const override;
    void resetStatistics() override;
    void shutdown() override;

protected:
    ScriptValue cppToScript(const std::any& value) override;
    std::any scriptToCpp(const ScriptValue& value,
                         const std::type_info& targetType) override;

private:
    std::unique_ptr<IScriptEngine> engine_;
    ScriptSandbox* sandbox_;
    std::string scriptName_;

    bool checkPermissionForOperation(const std::string& operation,
                                     const std::string& context = "");
    ScriptFunction wrapFunction(const std::string& name,
                                ScriptFunction function);
};

}  // namespace atom::components::scripting

#endif  // ATOM_COMPONENT_SCRIPT_SANDBOX_HPP
