/*
 * cron_config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef CRON_CONFIG_HPP
#define CRON_CONFIG_HPP

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

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
 * @brief Performance metrics for the cron system
 */
struct CronSystemMetrics {
    // Job statistics
    std::atomic<uint64_t> totalJobs{0};
    std::atomic<uint64_t> activeJobs{0};
    std::atomic<uint64_t> completedJobs{0};
    std::atomic<uint64_t> failedJobs{0};
    std::atomic<uint64_t> skippedJobs{0};
    
    // Execution statistics
    std::atomic<uint64_t> totalExecutions{0};
    std::atomic<uint64_t> successfulExecutions{0};
    std::atomic<uint64_t> failedExecutions{0};
    std::atomic<uint64_t> timeoutExecutions{0};
    
    // Performance metrics
    std::atomic<uint64_t> totalExecutionTime{0}; // microseconds
    std::atomic<uint64_t> minExecutionTime{UINT64_MAX};
    std::atomic<uint64_t> maxExecutionTime{0};
    std::atomic<uint64_t> schedulingLatency{0}; // microseconds
    
    // Resource usage
    std::atomic<uint64_t> memoryUsageBytes{0};
    std::atomic<uint64_t> peakMemoryUsageBytes{0};
    std::atomic<uint32_t> activeThreads{0};
    std::atomic<uint32_t> peakActiveThreads{0};
    
    // Cache statistics
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};
    std::atomic<uint64_t> cacheEvictions{0};
    
    // Error tracking
    std::atomic<uint64_t> validationErrors{0};
    std::atomic<uint64_t> securityViolations{0};
    std::atomic<uint64_t> systemErrors{0};
    
    std::chrono::steady_clock::time_point startTime{std::chrono::steady_clock::now()};
    
    void reset() noexcept {
        totalJobs = 0;
        activeJobs = 0;
        completedJobs = 0;
        failedJobs = 0;
        skippedJobs = 0;
        totalExecutions = 0;
        successfulExecutions = 0;
        failedExecutions = 0;
        timeoutExecutions = 0;
        totalExecutionTime = 0;
        minExecutionTime = UINT64_MAX;
        maxExecutionTime = 0;
        schedulingLatency = 0;
        memoryUsageBytes = 0;
        peakMemoryUsageBytes = 0;
        activeThreads = 0;
        peakActiveThreads = 0;
        cacheHits = 0;
        cacheMisses = 0;
        cacheEvictions = 0;
        validationErrors = 0;
        securityViolations = 0;
        systemErrors = 0;
        startTime = std::chrono::steady_clock::now();
    }
    
    double getSuccessRate() const noexcept {
        auto total = totalExecutions.load();
        return total > 0 ? static_cast<double>(successfulExecutions.load()) / total : 0.0;
    }
    
    double getAverageExecutionTime() const noexcept {
        auto total = totalExecutions.load();
        return total > 0 ? static_cast<double>(totalExecutionTime.load()) / total : 0.0;
    }
    
    double getCacheHitRatio() const noexcept {
        auto total = cacheHits.load() + cacheMisses.load();
        return total > 0 ? static_cast<double>(cacheHits.load()) / total : 0.0;
    }
    
    std::chrono::seconds getUptime() const noexcept {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime);
    }
};

/**
 * @brief Event types for cron system monitoring
 */
enum class CronEventType {
    JOB_CREATED,
    JOB_UPDATED,
    JOB_DELETED,
    JOB_STARTED,
    JOB_COMPLETED,
    JOB_FAILED,
    JOB_TIMEOUT,
    JOB_SKIPPED,
    SCHEDULER_STARTED,
    SCHEDULER_STOPPED,
    SYSTEM_ERROR,
    SECURITY_VIOLATION,
    RESOURCE_LIMIT_EXCEEDED,
    CACHE_OPERATION,
    HEALTH_CHECK_FAILED
};

/**
 * @brief Cron system event
 */
struct CronEvent {
    CronEventType type;
    std::string jobId;
    std::string userId;
    std::string details;
    std::chrono::steady_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;
    
    CronEvent(CronEventType t, const std::string& jid = "", const std::string& uid = "", 
              const std::string& d = "")
        : type(t), jobId(jid), userId(uid), details(d), 
          timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Event callback function type
 */
using CronEventCallback = std::function<void(const CronEvent& event)>;

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

// Convenience macros for accessing global configuration
#define CRON_CONFIG() CronConfigManager::getInstance().getConfig()
#define CRON_METRICS() CronConfigManager::getInstance().getMetrics()
#define CRON_EMIT_EVENT(type, jobId, userId, details) \
    CronConfigManager::getInstance().emitEvent(CronEvent(type, jobId, userId, details))

/**
 * @brief RAII class for measuring execution time
 */
class CronExecutionTimer {
public:
    explicit CronExecutionTimer(CronEventType type, const std::string& jobId = "")
        : type_(type), jobId_(jobId), startTime_(std::chrono::steady_clock::now()) {}
    
    ~CronExecutionTimer() {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime_);
        CronConfigManager::getInstance().updateMetrics(type_, jobId_, duration);
    }

private:
    CronEventType type_;
    std::string jobId_;
    std::chrono::steady_clock::time_point startTime_;
};

#define CRON_TIMER(type, jobId) CronExecutionTimer timer(type, jobId)

#endif // CRON_CONFIG_HPP
