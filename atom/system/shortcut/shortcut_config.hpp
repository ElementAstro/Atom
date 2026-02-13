/*
 * shortcut_config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef SHORTCUT_CONFIG_HPP
#define SHORTCUT_CONFIG_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

/**
 * @brief Global shortcut system configuration
 */
struct ShortcutSystemConfig {
    // Detection settings
    bool enableDetection{true};
    std::chrono::milliseconds detectionInterval{50};
    size_t maxConcurrentDetections{10};
    bool enableGlobalHooks{false}; // Requires elevated privileges

    // Caching settings
    bool enableGlobalCache{true};
    std::chrono::milliseconds shortcutCacheTTL{5000};
    std::chrono::milliseconds hookCacheTTL{1000};
    std::chrono::milliseconds processCacheTTL{2000};
    size_t maxCacheEntries{2000};

    // Monitoring settings
    bool enableRealTimeMonitoring{true};
    bool enableConflictDetection{true};
    bool enableSystemStateMonitoring{true};
    std::chrono::milliseconds pollingInterval{100};
    size_t maxEventQueueSize{1000};

    // Performance settings
    bool enablePerformanceMonitoring{true};
    std::chrono::milliseconds metricsUpdateInterval{1000};
    bool enableAsyncOperations{true};
    size_t maxAsyncTasks{20};

    // Thread pool settings
    size_t minThreads{2};
    size_t maxThreads{10};
    std::chrono::seconds threadIdleTimeout{300};
    bool enableDynamicScaling{true};

    // Memory management
    bool enableMemoryPooling{true};
    size_t initialPoolSize{1024};
    size_t maxPoolSize{10240};

    // Security settings
    bool enablePermissionChecks{true};
    bool enableAuditLogging{false};
    size_t maxAuditLogSize{10000};

    // Error handling
    bool enableErrorRecovery{true};
    size_t maxRetryAttempts{3};
    std::chrono::milliseconds retryDelay{100};
};

/**
 * @brief Performance metrics for the shortcut system
 */
struct ShortcutSystemMetrics {
    // Detection statistics
    std::atomic<uint64_t> totalDetections{0};
    std::atomic<uint64_t> successfulDetections{0};
    std::atomic<uint64_t> failedDetections{0};
    std::atomic<uint64_t> cachedDetections{0};

    // Performance metrics
    std::atomic<uint64_t> totalDetectionTime{0}; // microseconds
    std::atomic<uint64_t> minDetectionTime{UINT64_MAX};
    std::atomic<uint64_t> maxDetectionTime{0};
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};

    // Monitoring statistics
    std::atomic<uint64_t> totalEvents{0};
    std::atomic<uint64_t> processedEvents{0};
    std::atomic<uint64_t> droppedEvents{0};
    std::atomic<uint64_t> conflictsDetected{0};
    std::atomic<uint64_t> conflictsResolved{0};

    // Resource usage
    std::atomic<uint64_t> memoryUsageBytes{0};
    std::atomic<uint64_t> peakMemoryUsageBytes{0};
    std::atomic<uint32_t> activeThreads{0};
    std::atomic<uint32_t> peakActiveThreads{0};

    // Error tracking
    std::atomic<uint64_t> errors{0};
    std::atomic<uint64_t> permissionErrors{0};
    std::atomic<uint64_t> systemErrors{0};
    std::atomic<uint64_t> recoveredErrors{0};

    std::chrono::steady_clock::time_point startTime{std::chrono::steady_clock::now()};

    void reset() noexcept {
        totalDetections = 0;
        successfulDetections = 0;
        failedDetections = 0;
        cachedDetections = 0;
        totalDetectionTime = 0;
        minDetectionTime = UINT64_MAX;
        maxDetectionTime = 0;
        cacheHits = 0;
        cacheMisses = 0;
        totalEvents = 0;
        processedEvents = 0;
        droppedEvents = 0;
        conflictsDetected = 0;
        conflictsResolved = 0;
        memoryUsageBytes = 0;
        peakMemoryUsageBytes = 0;
        activeThreads = 0;
        peakActiveThreads = 0;
        errors = 0;
        permissionErrors = 0;
        systemErrors = 0;
        recoveredErrors = 0;
        startTime = std::chrono::steady_clock::now();
    }

    double getSuccessRate() const noexcept {
        auto total = totalDetections.load();
        return total > 0 ? static_cast<double>(successfulDetections.load()) / total : 0.0;
    }

    double getAverageDetectionTime() const noexcept {
        auto total = totalDetections.load();
        return total > 0 ? static_cast<double>(totalDetectionTime.load()) / total : 0.0;
    }

    double getCacheHitRatio() const noexcept {
        auto total = cacheHits.load() + cacheMisses.load();
        return total > 0 ? static_cast<double>(cacheHits.load()) / total : 0.0;
    }

    double getEventProcessingRate() const noexcept {
        auto total = totalEvents.load();
        return total > 0 ? static_cast<double>(processedEvents.load()) / total : 0.0;
    }

    std::chrono::seconds getUptime() const noexcept {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime);
    }
};

/**
 * @brief Event types for shortcut system monitoring
 */
enum class ShortcutEventType {
    DETECTION_STARTED,
    DETECTION_COMPLETED,
    DETECTION_FAILED,
    CACHE_HIT,
    CACHE_MISS,
    CACHE_EVICTION,
    SHORTCUT_REGISTERED,
    SHORTCUT_UNREGISTERED,
    CONFLICT_DETECTED,
    CONFLICT_RESOLVED,
    SYSTEM_STATE_CHANGED,
    PERMISSION_ERROR,
    SYSTEM_ERROR,
    ERROR_RECOVERED,
    MONITORING_STARTED,
    MONITORING_STOPPED
};

/**
 * @brief Shortcut system event
 */
struct ShortcutEvent {
    ShortcutEventType type;
    std::string shortcutId;
    std::string source;
    std::string details;
    std::chrono::steady_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;

    ShortcutEvent(ShortcutEventType t, const std::string& sid = "", const std::string& src = "",
                  const std::string& d = "")
        : type(t), shortcutId(sid), source(src), details(d),
          timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Event callback function type
 */
using ShortcutEventCallback = std::function<void(const ShortcutEvent& event)>;

/**
 * @brief Centralized configuration manager for the shortcut system
 */
class ShortcutConfigManager {
public:
    static ShortcutConfigManager& getInstance();

    /**
     * @brief Get current configuration
     */
    const ShortcutSystemConfig& getConfig() const;

    /**
     * @brief Update configuration
     */
    void updateConfig(const ShortcutSystemConfig& config);

    /**
     * @brief Get performance metrics
     */
    const ShortcutSystemMetrics& getMetrics() const;

    /**
     * @brief Reset performance metrics
     */
    void resetMetrics();

    /**
     * @brief Register event callback
     */
    size_t registerEventCallback(ShortcutEventCallback callback);

    /**
     * @brief Unregister event callback
     */
    bool unregisterEventCallback(size_t id);

    /**
     * @brief Emit event to all registered callbacks
     */
    void emitEvent(const ShortcutEvent& event);

    /**
     * @brief Load configuration from file
     */
    bool loadFromFile(const std::string& filePath);

    /**
     * @brief Save configuration to file
     */
    bool saveToFile(const std::string& filePath) const;

    /**
     * @brief Update metrics
     */
    void updateMetrics(ShortcutEventType type, const std::string& shortcutId = "",
                      std::chrono::microseconds detectionTime = std::chrono::microseconds::zero());

private:
    ShortcutConfigManager();
    ~ShortcutConfigManager() = default;

    ShortcutConfigManager(const ShortcutConfigManager&) = delete;
    ShortcutConfigManager& operator=(const ShortcutConfigManager&) = delete;

    mutable std::mutex configMutex_;
    ShortcutSystemConfig config_;
    mutable ShortcutSystemMetrics metrics_;

    mutable std::mutex callbackMutex_;
    std::unordered_map<size_t, ShortcutEventCallback> callbacks_;
    std::atomic<size_t> nextCallbackId_{1};
};

// Convenience macros for accessing global configuration
#define SHORTCUT_CONFIG() ShortcutConfigManager::getInstance().getConfig()
#define SHORTCUT_METRICS() ShortcutConfigManager::getInstance().getMetrics()
#define SHORTCUT_EMIT_EVENT(type, shortcutId, source, details) \
    ShortcutConfigManager::getInstance().emitEvent(ShortcutEvent(type, shortcutId, source, details))

/**
 * @brief RAII class for measuring detection time
 */
class ShortcutDetectionTimer {
public:
    explicit ShortcutDetectionTimer(ShortcutEventType type, const std::string& shortcutId = "")
        : type_(type), shortcutId_(shortcutId), startTime_(std::chrono::steady_clock::now()) {}

    ~ShortcutDetectionTimer() {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime_);
        ShortcutConfigManager::getInstance().updateMetrics(type_, shortcutId_, duration);
    }

private:
    ShortcutEventType type_;
    std::string shortcutId_;
    std::chrono::steady_clock::time_point startTime_;
};

#define SHORTCUT_TIMER(type, shortcutId) ShortcutDetectionTimer timer(type, shortcutId)

#endif // SHORTCUT_CONFIG_HPP
