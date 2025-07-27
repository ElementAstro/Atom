/*
 * env_config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_ENV_CONFIG_HPP
#define ATOM_SYSTEM_ENV_CONFIG_HPP

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;

/**
 * @brief Global environment system configuration
 */
struct EnvSystemConfig {
    // Caching settings
    bool enableGlobalCache{true};
    std::chrono::milliseconds cacheTTL{300000}; // 5 minutes
    size_t maxCacheEntries{10000};
    bool enablePathCache{true};
    std::chrono::milliseconds pathCacheTTL{60000}; // 1 minute
    
    // Performance settings
    bool enableBatchOperations{true};
    size_t batchSize{100};
    bool enableAsyncOperations{true};
    size_t maxAsyncTasks{10};
    
    // File I/O settings
    bool enableAtomicWrites{true};
    bool enableBackups{true};
    size_t maxBackups{5};
    bool enableCompression{false};
    
    // Security settings
    bool enableEncryption{false};
    String encryptionKey{""};
    bool enableValidation{true};
    bool strictValidation{false};
    
    // Monitoring settings
    bool enableMetrics{true};
    std::chrono::milliseconds metricsUpdateInterval{1000};
    bool enableEventLogging{true};
    
    // Memory management
    bool enableMemoryPooling{true};
    size_t initialPoolSize{1024};
    size_t maxPoolSize{10240};
};

/**
 * @brief Performance metrics for the environment system
 */
struct EnvSystemMetrics {
    // Operation counters
    std::atomic<uint64_t> totalOperations{0};
    std::atomic<uint64_t> getOperations{0};
    std::atomic<uint64_t> setOperations{0};
    std::atomic<uint64_t> unsetOperations{0};
    std::atomic<uint64_t> batchOperations{0};
    
    // Performance metrics
    std::atomic<uint64_t> totalExecutionTime{0}; // microseconds
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};
    std::atomic<uint64_t> fileOperations{0};
    std::atomic<uint64_t> pathOperations{0};
    
    // Error tracking
    std::atomic<uint64_t> errors{0};
    std::atomic<uint64_t> validationFailures{0};
    std::atomic<uint64_t> fileErrors{0};
    
    // Memory usage
    std::atomic<uint64_t> memoryAllocated{0};
    std::atomic<uint64_t> memoryFreed{0};
    std::atomic<uint64_t> peakMemoryUsage{0};
    
    std::chrono::steady_clock::time_point startTime{std::chrono::steady_clock::now()};
    
    void reset() noexcept {
        totalOperations = 0;
        getOperations = 0;
        setOperations = 0;
        unsetOperations = 0;
        batchOperations = 0;
        totalExecutionTime = 0;
        cacheHits = 0;
        cacheMisses = 0;
        fileOperations = 0;
        pathOperations = 0;
        errors = 0;
        validationFailures = 0;
        fileErrors = 0;
        memoryAllocated = 0;
        memoryFreed = 0;
        peakMemoryUsage = 0;
        startTime = std::chrono::steady_clock::now();
    }
    
    double getCacheHitRatio() const noexcept {
        auto total = cacheHits.load() + cacheMisses.load();
        return total > 0 ? static_cast<double>(cacheHits.load()) / total : 0.0;
    }
    
    double getAverageExecutionTime() const noexcept {
        auto total = totalOperations.load();
        return total > 0 ? static_cast<double>(totalExecutionTime.load()) / total : 0.0;
    }
    
    double getErrorRate() const noexcept {
        auto total = totalOperations.load();
        return total > 0 ? static_cast<double>(errors.load()) / total : 0.0;
    }
};

/**
 * @brief Event types for environment system monitoring
 */
enum class EnvEventType {
    VARIABLE_SET,
    VARIABLE_GET,
    VARIABLE_UNSET,
    BATCH_OPERATION,
    FILE_OPERATION,
    PATH_OPERATION,
    CACHE_HIT,
    CACHE_MISS,
    ERROR_OCCURRED,
    VALIDATION_FAILED
};

/**
 * @brief Environment system event
 */
struct EnvEvent {
    EnvEventType type;
    String key;
    String value;
    String details;
    std::chrono::steady_clock::time_point timestamp;
    
    EnvEvent(EnvEventType t, const String& k = "", const String& v = "", const String& d = "")
        : type(t), key(k), value(v), details(d), timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Event callback function type
 */
using EnvEventCallback = std::function<void(const EnvEvent& event)>;

/**
 * @brief Centralized configuration manager for the environment system
 */
class EnvConfigManager {
public:
    static EnvConfigManager& getInstance();
    
    /**
     * @brief Get current configuration
     */
    const EnvSystemConfig& getConfig() const;
    
    /**
     * @brief Update configuration
     */
    void updateConfig(const EnvSystemConfig& config);
    
    /**
     * @brief Get performance metrics
     */
    const EnvSystemMetrics& getMetrics() const;
    
    /**
     * @brief Reset performance metrics
     */
    void resetMetrics();
    
    /**
     * @brief Register event callback
     */
    size_t registerEventCallback(EnvEventCallback callback);
    
    /**
     * @brief Unregister event callback
     */
    bool unregisterEventCallback(size_t id);
    
    /**
     * @brief Emit event to all registered callbacks
     */
    void emitEvent(const EnvEvent& event);
    
    /**
     * @brief Load configuration from file
     */
    bool loadFromFile(const String& filePath);
    
    /**
     * @brief Save configuration to file
     */
    bool saveToFile(const String& filePath) const;
    
    /**
     * @brief Update metrics
     */
    void updateMetrics(EnvEventType type, const String& key = "", 
                      std::chrono::microseconds executionTime = std::chrono::microseconds::zero());

private:
    EnvConfigManager();
    ~EnvConfigManager() = default;
    
    EnvConfigManager(const EnvConfigManager&) = delete;
    EnvConfigManager& operator=(const EnvConfigManager&) = delete;
    
    mutable std::mutex configMutex_;
    EnvSystemConfig config_;
    mutable EnvSystemMetrics metrics_;
    
    mutable std::mutex callbackMutex_;
    HashMap<size_t, EnvEventCallback> callbacks_;
    std::atomic<size_t> nextCallbackId_{1};
};

// Convenience macros for accessing global configuration
#define ENV_CONFIG() EnvConfigManager::getInstance().getConfig()
#define ENV_METRICS() EnvConfigManager::getInstance().getMetrics()
#define ENV_EMIT_EVENT(type, key, value, details) \
    EnvConfigManager::getInstance().emitEvent(EnvEvent(type, key, value, details))

/**
 * @brief RAII class for measuring execution time
 */
class EnvExecutionTimer {
public:
    explicit EnvExecutionTimer(EnvEventType type, const String& key = "")
        : type_(type), key_(key), startTime_(std::chrono::steady_clock::now()) {}
    
    ~EnvExecutionTimer() {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime_);
        EnvConfigManager::getInstance().updateMetrics(type_, key_, duration);
    }

private:
    EnvEventType type_;
    String key_;
    std::chrono::steady_clock::time_point startTime_;
};

#define ENV_TIMER(type, key) EnvExecutionTimer timer(type, key)

} // namespace atom::utils

#endif // ATOM_SYSTEM_ENV_CONFIG_HPP
