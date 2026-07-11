/*
 * cron_config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef CRON_CONFIG_HPP
#define CRON_CONFIG_HPP

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

#include "cron_event_types.hpp"
#include "cron_metrics_types.hpp"

/**
 * @brief Global cron system configuration
 */
struct CronSystemConfig {
    // Job management settings
    size_t maxJobs{10000};
    size_t maxJobsPerUser{1000};
    size_t maxExecutionHistory{100};
    std::chrono::minutes jobTimeout{60};

    // Scheduler settings
    std::chrono::milliseconds schedulerInterval{1000};
    size_t maxConcurrentJobs{50};
    bool enableJobDependencies{true};
    bool enableJobPriorities{true};

    // Thread pool settings
    size_t minThreads{2};
    size_t maxThreads{20};
    std::chrono::seconds threadIdleTimeout{300};
    bool enableDynamicScaling{true};

    // Storage settings
    bool enablePersistence{true};
    std::chrono::minutes saveInterval{5};
    bool enableCompression{true};
    bool enableEncryption{false};
    size_t maxBackupFiles{5};

    // Monitoring settings
    bool enableMetrics{true};
    std::chrono::seconds metricsUpdateInterval{10};
    bool enableHealthChecks{true};
    std::chrono::minutes healthCheckInterval{5};

    // Security settings
    bool enableAuthentication{true};
    bool enableAuthorization{true};
    bool enableAuditLogging{true};
    size_t maxAuditLogSize{10000};

    // Performance settings
    bool enableCaching{true};
    size_t cacheSize{1000};
    std::chrono::minutes cacheTTL{30};
    bool enableMemoryPooling{true};

    // Resource limits
    size_t maxMemoryUsageMB{512};
    double maxCpuUsagePercent{80.0};
    size_t maxFileDescriptors{1000};
};

/**
 * @brief Centralized configuration manager for the cron system
 */
class CronConfigManager {
public:
    static CronConfigManager& getInstance();

    /**
     * @brief Get current configuration
     */
    const CronSystemConfig& getConfig() const;

    /**
     * @brief Update configuration
     */
    void updateConfig(const CronSystemConfig& config);

    /**
     * @brief Get performance metrics
     */
    const CronSystemMetrics& getMetrics() const;

    /**
     * @brief Mutable access to live metrics (atomic counters are updated
     * in-place by schedulers, thread pools, etc., so a const reference is
     * insufficient).
     */
    CronSystemMetrics& getMetrics();

    /**
     * @brief Reset performance metrics
     */
    void resetMetrics();

    /**
     * @brief Register event callback
     */
    size_t registerEventCallback(CronEventCallback callback);

    /**
     * @brief Unregister event callback
     */
    bool unregisterEventCallback(size_t id);

    /**
     * @brief Emit event to all registered callbacks
     */
    void emitEvent(const CronEvent& event);

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
    void updateMetrics(CronEventType type, const std::string& jobId = "",
                      std::chrono::microseconds executionTime = std::chrono::microseconds::zero());

private:
    CronConfigManager();
    ~CronConfigManager() = default;

    CronConfigManager(const CronConfigManager&) = delete;
    CronConfigManager& operator=(const CronConfigManager&) = delete;

    mutable std::mutex configMutex_;
    CronSystemConfig config_;
    mutable CronSystemMetrics metrics_;

    mutable std::mutex callbackMutex_;
    std::unordered_map<size_t, CronEventCallback> callbacks_;
    std::atomic<size_t> nextCallbackId_{1};
};

// Convenience macro for accessing global configuration
#define CRON_CONFIG() CronConfigManager::getInstance().getConfig()

#endif  // CRON_CONFIG_HPP
