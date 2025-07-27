/**
 * @file config.hpp
 * @brief Configuration management for locale functionality
 *
 * This file contains configuration management, caching strategies,
 * and performance optimization utilities for the locale system.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_LOCALE_CONFIG_HPP
#define ATOM_SYSINFO_LOCALE_CONFIG_HPP

#include "common.hpp"
#include <chrono>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>

namespace atom::system::locale {

/**
 * @enum CacheStrategy
 * @brief Different caching strategies for locale data
 */
enum class CacheStrategy {
    None,           /**< No caching */
    Memory,         /**< In-memory caching only */
    Persistent,     /**< Persistent caching to disk */
    Hybrid          /**< Combination of memory and persistent caching */
};

/**
 * @enum LogLevel
 * @brief Logging levels for locale operations
 */
enum class LogLevel {
    None,           /**< No logging */
    Error,          /**< Error messages only */
    Warning,        /**< Warnings and errors */
    Info,           /**< Informational messages */
    Debug,          /**< Debug information */
    Trace           /**< Detailed trace information */
};

/**
 * @struct CacheConfig
 * @brief Configuration for caching behavior
 */
struct CacheConfig {
    CacheStrategy strategy{CacheStrategy::Memory};      /**< Caching strategy to use */
    std::chrono::seconds timeout{300};                  /**< Cache timeout in seconds */
    size_t maxMemoryEntries{1000};                      /**< Maximum entries in memory cache */
    size_t maxMemorySize{10 * 1024 * 1024};            /**< Maximum memory cache size in bytes */
    std::string persistentCachePath;                    /**< Path for persistent cache files */
    bool enableCompression{true};                       /**< Enable cache compression */
    bool enableEncryption{false};                       /**< Enable cache encryption */
    std::chrono::seconds persistentCacheTimeout{86400}; /**< Persistent cache timeout (24 hours) */
};

/**
 * @struct PerformanceConfig
 * @brief Configuration for performance optimizations
 */
struct PerformanceConfig {
    bool enableLazyLoading{true};                       /**< Enable lazy loading of locale data */
    bool enablePreloading{false};                       /**< Enable preloading of common locales */
    std::vector<std::string> preloadLocales;            /**< Locales to preload */
    size_t threadPoolSize{4};                           /**< Thread pool size for async operations */
    bool enableAsyncOperations{true};                   /**< Enable asynchronous operations */
    std::chrono::milliseconds operationTimeout{5000};   /**< Timeout for locale operations */
    bool enableMetrics{false};                          /**< Enable performance metrics collection */
};

/**
 * @struct LocaleConfig
 * @brief Main configuration structure for locale system
 */
struct LocaleConfig {
    std::string defaultLocale{"en_US.UTF-8"};           /**< Default locale to use */
    std::vector<std::string> fallbackLocales{"C", "POSIX"}; /**< Fallback locales */
    CacheConfig cache;                                   /**< Cache configuration */
    PerformanceConfig performance;                       /**< Performance configuration */
    LogLevel logLevel{LogLevel::Warning};                /**< Logging level */
    bool enableValidation{true};                         /**< Enable locale validation */
    bool enableCompatibilityChecks{true};               /**< Enable compatibility checks */
    std::unordered_map<std::string, std::string> customMappings; /**< Custom locale mappings */
    std::string configFilePath;                          /**< Path to configuration file */
};

/**
 * @class LocaleConfigManager
 * @brief Manager for locale system configuration
 */
class LocaleConfigManager {
public:
    /**
     * @brief Gets the singleton instance
     * @return Reference to the configuration manager instance
     */
    static auto getInstance() -> LocaleConfigManager&;
    
    /**
     * @brief Loads configuration from file
     * @param filePath Path to configuration file
     * @return true if loaded successfully, false otherwise
     */
    auto loadFromFile(const std::string& filePath) -> bool;
    
    /**
     * @brief Saves configuration to file
     * @param filePath Path to configuration file
     * @return true if saved successfully, false otherwise
     */
    auto saveToFile(const std::string& filePath) -> bool;
    
    /**
     * @brief Loads configuration from environment variables
     */
    void loadFromEnvironment();
    
    /**
     * @brief Gets the current configuration
     * @return Reference to the current configuration
     */
    auto getConfig() -> const LocaleConfig&;
    
    /**
     * @brief Sets the configuration
     * @param config New configuration to set
     */
    void setConfig(const LocaleConfig& config);
    
    /**
     * @brief Updates a specific configuration value
     * @param key Configuration key
     * @param value Configuration value
     */
    void updateConfig(const std::string& key, const std::string& value);
    
    /**
     * @brief Resets configuration to defaults
     */
    void resetToDefaults();
    
    /**
     * @brief Validates the current configuration
     * @return true if configuration is valid, false otherwise
     */
    auto validateConfig() -> bool;
    
    /**
     * @brief Gets configuration as JSON string
     * @return JSON representation of configuration
     */
    auto toJson() -> std::string;
    
    /**
     * @brief Loads configuration from JSON string
     * @param json JSON string containing configuration
     * @return true if loaded successfully, false otherwise
     */
    auto fromJson(const std::string& json) -> bool;

private:
    LocaleConfigManager() = default;
    LocaleConfig config_;
    mutable std::mutex configMutex_;
};

/**
 * @class LocaleCache
 * @brief Advanced caching system for locale data
 */
class LocaleCache {
public:
    /**
     * @brief Constructor
     * @param config Cache configuration
     */
    explicit LocaleCache(const CacheConfig& config);
    
    /**
     * @brief Destructor
     */
    ~LocaleCache();
    
    /**
     * @brief Stores locale information in cache
     * @param key Cache key
     * @param info Locale information to cache
     */
    void store(const std::string& key, const LocaleInfo& info);
    
    /**
     * @brief Retrieves locale information from cache
     * @param key Cache key
     * @return Optional locale information if found
     */
    auto retrieve(const std::string& key) -> std::optional<LocaleInfo>;
    
    /**
     * @brief Checks if a key exists in cache
     * @param key Cache key to check
     * @return true if key exists, false otherwise
     */
    auto exists(const std::string& key) -> bool;
    
    /**
     * @brief Removes an entry from cache
     * @param key Cache key to remove
     */
    void remove(const std::string& key);
    
    /**
     * @brief Clears all cache entries
     */
    void clear();
    
    /**
     * @brief Gets cache statistics
     * @return Map of cache statistics
     */
    auto getStatistics() -> std::unordered_map<std::string, size_t>;
    
    /**
     * @brief Performs cache maintenance (cleanup expired entries)
     */
    void maintenance();
    
    /**
     * @brief Sets cache configuration
     * @param config New cache configuration
     */
    void setConfig(const CacheConfig& config);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @class PerformanceMonitor
 * @brief Performance monitoring and metrics collection
 */
class PerformanceMonitor {
public:
    /**
     * @brief Performance metrics structure
     */
    struct Metrics {
        size_t totalOperations{0};
        size_t successfulOperations{0};
        size_t failedOperations{0};
        double averageExecutionTime{0.0};
        double minExecutionTime{0.0};
        double maxExecutionTime{0.0};
        size_t cacheHits{0};
        size_t cacheMisses{0};
        std::chrono::system_clock::time_point lastReset;
    };
    
    /**
     * @brief Gets the singleton instance
     * @return Reference to the performance monitor instance
     */
    static auto getInstance() -> PerformanceMonitor&;
    
    /**
     * @brief Records an operation execution time
     * @param operation Operation name
     * @param executionTime Execution time in milliseconds
     * @param success Whether the operation was successful
     */
    void recordOperation(const std::string& operation, double executionTime, bool success = true);
    
    /**
     * @brief Records a cache hit
     * @param operation Operation name
     */
    void recordCacheHit(const std::string& operation);
    
    /**
     * @brief Records a cache miss
     * @param operation Operation name
     */
    void recordCacheMiss(const std::string& operation);
    
    /**
     * @brief Gets metrics for a specific operation
     * @param operation Operation name
     * @return Metrics for the operation
     */
    auto getMetrics(const std::string& operation) -> Metrics;
    
    /**
     * @brief Gets all collected metrics
     * @return Map of operation names to metrics
     */
    auto getAllMetrics() -> std::unordered_map<std::string, Metrics>;
    
    /**
     * @brief Resets metrics for a specific operation
     * @param operation Operation name
     */
    void resetMetrics(const std::string& operation);
    
    /**
     * @brief Resets all metrics
     */
    void resetAllMetrics();
    
    /**
     * @brief Enables or disables metrics collection
     * @param enabled Whether to enable metrics collection
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Checks if metrics collection is enabled
     * @return true if enabled, false otherwise
     */
    auto isEnabled() const -> bool;

private:
    PerformanceMonitor() = default;
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief RAII class for automatic performance measurement
 */
class PerformanceTimer {
public:
    /**
     * @brief Constructor - starts timing
     * @param operation Operation name
     */
    explicit PerformanceTimer(const std::string& operation);
    
    /**
     * @brief Destructor - records timing
     */
    ~PerformanceTimer();
    
    /**
     * @brief Marks the operation as failed
     */
    void markFailed();

private:
    std::string operation_;
    std::chrono::high_resolution_clock::time_point startTime_;
    bool failed_{false};
};

/**
 * @brief Macro for easy performance timing
 */
#define LOCALE_PERFORMANCE_TIMER(operation) \
    atom::system::locale::PerformanceTimer timer(operation)

} // namespace atom::system::locale

#endif // ATOM_SYSINFO_LOCALE_CONFIG_HPP
