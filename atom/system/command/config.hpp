/*
 * config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_CONFIG_HPP
#define ATOM_SYSTEM_COMMAND_CONFIG_HPP

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Global command system configuration
 */
struct CommandSystemConfig {
    // Execution settings
    std::chrono::milliseconds defaultTimeout{30000};
    size_t defaultBufferSize{8192};
    size_t maxOutputSize{10 * 1024 * 1024}; // 10MB
    size_t maxConcurrentCommands{10};
    
    // Thread pool settings
    size_t minThreads{2};
    size_t maxThreads{std::thread::hardware_concurrency()};
    std::chrono::milliseconds threadIdleTimeout{60000};
    
    // Caching settings
    bool enableValidationCache{true};
    std::chrono::milliseconds validationCacheTTL{300000}; // 5 minutes
    size_t maxCacheEntries{1000};
    
    // Monitoring settings
    bool enableMetrics{true};
    bool enableResourceMonitoring{true};
    std::chrono::milliseconds metricsUpdateInterval{1000};
    
    // Security settings
    bool enableSecurityValidation{true};
    bool enableAuditLogging{true};
    std::string auditLogPath{"command_audit.log"};
    
    // History settings
    size_t maxHistoryEntries{10000};
    bool enableHistoryPersistence{true};
    std::string historyFilePath{"command_history.db"};
    
    // Rate limiting
    bool enableRateLimit{false};
    size_t maxCommandsPerSecond{10};
    std::chrono::milliseconds rateLimitWindow{1000};
};

/**
 * @brief Performance metrics for the command system
 */
struct CommandSystemMetrics {
    std::atomic<uint64_t> totalCommands{0};
    std::atomic<uint64_t> successfulCommands{0};
    std::atomic<uint64_t> failedCommands{0};
    std::atomic<uint64_t> timedOutCommands{0};
    std::atomic<uint64_t> blockedCommands{0};
    
    std::atomic<uint64_t> totalExecutionTime{0}; // milliseconds
    std::atomic<uint64_t> totalOutputSize{0};    // bytes
    
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};
    
    std::atomic<uint64_t> activeThreads{0};
    std::atomic<uint64_t> queuedTasks{0};
    
    std::chrono::steady_clock::time_point startTime{std::chrono::steady_clock::now()};
    
    void reset() noexcept {
        totalCommands = 0;
        successfulCommands = 0;
        failedCommands = 0;
        timedOutCommands = 0;
        blockedCommands = 0;
        totalExecutionTime = 0;
        totalOutputSize = 0;
        cacheHits = 0;
        cacheMisses = 0;
        activeThreads = 0;
        queuedTasks = 0;
        startTime = std::chrono::steady_clock::now();
    }
    
    double getSuccessRate() const noexcept {
        auto total = totalCommands.load();
        return total > 0 ? static_cast<double>(successfulCommands.load()) / total : 0.0;
    }
    
    double getCacheHitRatio() const noexcept {
        auto total = cacheHits.load() + cacheMisses.load();
        return total > 0 ? static_cast<double>(cacheHits.load()) / total : 0.0;
    }
    
    double getAverageExecutionTime() const noexcept {
        auto total = totalCommands.load();
        return total > 0 ? static_cast<double>(totalExecutionTime.load()) / total : 0.0;
    }
};

/**
 * @brief Rate limiter for command execution
 */
class RateLimiter {
public:
    explicit RateLimiter(size_t maxRequests, std::chrono::milliseconds window);
    
    /**
     * @brief Check if a request is allowed
     * @param identifier Optional identifier for per-user/per-source limiting
     * @return true if request is allowed
     */
    bool allowRequest(const std::string& identifier = "");
    
    /**
     * @brief Get current request count for identifier
     */
    size_t getCurrentCount(const std::string& identifier = "") const;
    
    /**
     * @brief Reset rate limiter
     */
    void reset();

private:
    struct RequestWindow {
        std::vector<std::chrono::steady_clock::time_point> requests;
        mutable std::mutex mutex;
    };
    
    size_t maxRequests_;
    std::chrono::milliseconds window_;
    mutable std::mutex globalMutex_;
    std::unordered_map<std::string, std::unique_ptr<RequestWindow>> windows_;
    
    void cleanupOldRequests(RequestWindow& window) const;
};

/**
 * @brief Centralized configuration manager for the command system
 */
class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    /**
     * @brief Get current configuration
     */
    const CommandSystemConfig& getConfig() const;
    
    /**
     * @brief Update configuration
     */
    void updateConfig(const CommandSystemConfig& config);
    
    /**
     * @brief Get performance metrics
     */
    const CommandSystemMetrics& getMetrics() const;
    
    /**
     * @brief Reset performance metrics
     */
    void resetMetrics();
    
    /**
     * @brief Get rate limiter instance
     */
    RateLimiter& getRateLimiter();
    
    /**
     * @brief Load configuration from file
     */
    bool loadFromFile(const std::string& filePath);
    
    /**
     * @brief Save configuration to file
     */
    bool saveToFile(const std::string& filePath) const;

private:
    ConfigManager();
    ~ConfigManager() = default;
    
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    mutable std::mutex configMutex_;
    CommandSystemConfig config_;
    mutable CommandSystemMetrics metrics_;
    std::unique_ptr<RateLimiter> rateLimiter_;
    
    void initializeRateLimiter();
};

// Convenience macros for accessing global configuration
#define COMMAND_CONFIG() ConfigManager::getInstance().getConfig()
#define COMMAND_METRICS() ConfigManager::getInstance().getMetrics()
#define COMMAND_RATE_LIMITER() ConfigManager::getInstance().getRateLimiter()

} // namespace atom::system

#endif // ATOM_SYSTEM_COMMAND_CONFIG_HPP
