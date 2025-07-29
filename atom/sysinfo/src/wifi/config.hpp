/*
 * config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - WiFi Configuration and Performance Tuning

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_WIFI_CONFIG_HPP
#define ATOM_SYSTEM_MODULE_WIFI_CONFIG_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief WiFi module configuration parameters
 */
struct WiFiConfig {
    // Timeout settings
    std::chrono::milliseconds ping_timeout{5000};
    std::chrono::milliseconds command_timeout{10000};
    std::chrono::milliseconds connection_timeout{30000};
    std::chrono::milliseconds scan_timeout{15000};

    // Cache settings
    std::chrono::seconds cache_ttl_wifi_info{15};
    std::chrono::seconds cache_ttl_network_stats{10};
    std::chrono::seconds cache_ttl_interface_list{120};
    std::chrono::seconds cache_ttl_available_networks{30};
    size_t max_cache_size{100};

    // Performance settings
    int max_concurrent_operations{4};
    size_t buffer_size{65536};
    int retry_attempts{3};
    std::chrono::milliseconds retry_delay{1000};

    // Quality analysis settings
    int jitter_packet_count{10};
    std::chrono::milliseconds jitter_interval{100};
    int packet_loss_test_count{50};
    std::chrono::seconds throughput_test_duration{5};

    // Monitoring settings
    std::chrono::seconds monitoring_interval{30};
    std::chrono::seconds quality_assessment_interval{300};
    size_t max_history_size{1000};
    bool enable_background_monitoring{true};

    // Error handling settings
    size_t max_error_history{500};
    bool enable_error_callbacks{true};
    std::chrono::minutes error_rate_window{60};

    // Platform-specific settings
    std::string preferred_ping_host{"8.8.8.8"};
    std::string backup_ping_host{"1.1.1.1"};
    std::string speed_test_server{"speedtest.net"};

    // Advanced settings
    bool enable_ipv6{true};
    bool enable_detailed_logging{false};
    bool enable_performance_metrics{true};
    double signal_strength_threshold{-70.0}; // dBm
    double packet_loss_threshold{5.0}; // percentage
    double jitter_threshold{50.0}; // milliseconds
} ATOM_ALIGNAS(64);

/**
 * @brief Performance optimization settings
 */
struct PerformanceConfig {
    // Memory optimization
    bool enable_memory_pooling{true};
    size_t memory_pool_size{1024 * 1024}; // 1MB
    bool enable_string_interning{true};

    // CPU optimization
    bool enable_parallel_processing{true};
    int thread_pool_size{0}; // 0 = auto-detect
    bool enable_cpu_affinity{false};

    // I/O optimization
    bool enable_async_io{true};
    size_t io_buffer_size{32768};
    int max_concurrent_io{8};

    // Network optimization
    bool enable_connection_pooling{true};
    int max_connections_per_host{4};
    std::chrono::seconds connection_keep_alive{30};

    // Cache optimization
    bool enable_adaptive_caching{true};
    double cache_hit_ratio_target{0.8};
    bool enable_cache_compression{false};
} ATOM_ALIGNAS(32);

/**
 * @brief WiFi configuration manager
 */
class WiFiConfigManager {
public:
    WiFiConfigManager();
    ~WiFiConfigManager() = default;

    // Disable copy constructor and assignment
    WiFiConfigManager(const WiFiConfigManager&) = delete;
    WiFiConfigManager& operator=(const WiFiConfigManager&) = delete;

    // Move constructor and assignment
    WiFiConfigManager(WiFiConfigManager&&) noexcept = default;
    WiFiConfigManager& operator=(WiFiConfigManager&&) noexcept = default;

    /**
     * @brief Load configuration from file
     * @param config_file Path to configuration file
     * @return True if loaded successfully
     */
    ATOM_NODISCARD auto loadFromFile(const std::string& config_file) -> bool;

    /**
     * @brief Save configuration to file
     * @param config_file Path to configuration file
     * @return True if saved successfully
     */
    ATOM_NODISCARD auto saveToFile(const std::string& config_file) const -> bool;

    /**
     * @brief Load configuration from environment variables
     */
    void loadFromEnvironment();

    /**
     * @brief Get current WiFi configuration
     * @return Current WiFi configuration
     */
    ATOM_NODISCARD auto getWiFiConfig() const -> const WiFiConfig&;

    /**
     * @brief Get current performance configuration
     * @return Current performance configuration
     */
    ATOM_NODISCARD auto getPerformanceConfig() const -> const PerformanceConfig&;

    /**
     * @brief Update WiFi configuration
     * @param config New WiFi configuration
     */
    void updateWiFiConfig(const WiFiConfig& config);

    /**
     * @brief Update performance configuration
     * @param config New performance configuration
     */
    void updatePerformanceConfig(const PerformanceConfig& config);

    /**
     * @brief Set a specific configuration parameter
     * @param key Parameter key
     * @param value Parameter value
     * @return True if parameter was set successfully
     */
    ATOM_NODISCARD auto setParameter(const std::string& key, const std::string& value) -> bool;

    /**
     * @brief Get a specific configuration parameter
     * @param key Parameter key
     * @return Parameter value, or empty string if not found
     */
    ATOM_NODISCARD auto getParameter(const std::string& key) const -> std::string;

    /**
     * @brief Reset configuration to defaults
     */
    void resetToDefaults();

    /**
     * @brief Validate current configuration
     * @return Vector of validation errors (empty if valid)
     */
    ATOM_NODISCARD auto validateConfiguration() const -> std::vector<std::string>;

    /**
     * @brief Get configuration as string (for debugging)
     * @return Formatted configuration string
     */
    ATOM_NODISCARD auto toString() const -> std::string;

    /**
     * @brief Auto-tune configuration based on system capabilities
     */
    void autoTune();

    /**
     * @brief Register configuration change callback
     * @param callback Function to call when configuration changes
     */
    void registerChangeCallback(std::function<void(const WiFiConfig&, const PerformanceConfig&)> callback);

private:
    void notifyConfigurationChange();
    void applyEnvironmentOverrides();
    auto parseTimeValue(const std::string& value) -> std::chrono::milliseconds;
    auto parseSizeValue(const std::string& value) -> size_t;
    auto parseBoolValue(const std::string& value) -> bool;

    WiFiConfig wifi_config_;
    PerformanceConfig performance_config_;
    std::vector<std::function<void(const WiFiConfig&, const PerformanceConfig&)>> change_callbacks_;
    mutable std::mutex config_mutex_;
};

/**
 * @brief Performance monitor for tracking optimization effectiveness
 */
class PerformanceMonitor {
public:
    PerformanceMonitor() = default;
    ~PerformanceMonitor() = default;

    // Disable copy constructor and assignment
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;

    // Move constructor and assignment
    PerformanceMonitor(PerformanceMonitor&&) noexcept = default;
    PerformanceMonitor& operator=(PerformanceMonitor&&) noexcept = default;

    /**
     * @brief Record operation timing
     * @param operation_name Name of the operation
     * @param duration Duration of the operation
     */
    void recordTiming(const std::string& operation_name, std::chrono::microseconds duration);

    /**
     * @brief Record memory usage
     * @param operation_name Name of the operation
     * @param memory_bytes Memory used in bytes
     */
    void recordMemoryUsage(const std::string& operation_name, size_t memory_bytes);

    /**
     * @brief Record cache hit/miss
     * @param cache_name Name of the cache
     * @param hit True for cache hit, false for miss
     */
    void recordCacheAccess(const std::string& cache_name, bool hit);

    /**
     * @brief Get performance statistics
     * @param duration Time period to analyze
     * @return Performance statistics as string
     */
    ATOM_NODISCARD auto getPerformanceStats(std::chrono::hours duration = std::chrono::hours{24}) const -> std::string;

    /**
     * @brief Get cache hit ratio
     * @param cache_name Name of the cache
     * @param duration Time period to analyze
     * @return Cache hit ratio (0.0 to 1.0)
     */
    ATOM_NODISCARD auto getCacheHitRatio(const std::string& cache_name,
                                        std::chrono::hours duration = std::chrono::hours{1}) const -> double;

    /**
     * @brief Get average operation time
     * @param operation_name Name of the operation
     * @param duration Time period to analyze
     * @return Average operation time in microseconds
     */
    ATOM_NODISCARD auto getAverageOperationTime(const std::string& operation_name,
                                               std::chrono::hours duration = std::chrono::hours{1}) const -> std::chrono::microseconds;

    /**
     * @brief Clear performance data
     */
    void clearData();

private:
    struct TimingRecord {
        std::chrono::steady_clock::time_point timestamp;
        std::chrono::microseconds duration;
    };

    struct MemoryRecord {
        std::chrono::steady_clock::time_point timestamp;
        size_t memory_bytes;
    };

    struct CacheRecord {
        std::chrono::steady_clock::time_point timestamp;
        bool hit;
    };

    mutable std::mutex monitor_mutex_;
    std::unordered_map<std::string, std::vector<TimingRecord>> timing_data_;
    std::unordered_map<std::string, std::vector<MemoryRecord>> memory_data_;
    std::unordered_map<std::string, std::vector<CacheRecord>> cache_data_;
};

/**
 * @brief RAII class for automatic performance timing
 */
class PerformanceTimer {
public:
    explicit PerformanceTimer(const std::string& operation_name, PerformanceMonitor& monitor);
    ~PerformanceTimer();

    // Disable copy constructor and assignment
    PerformanceTimer(const PerformanceTimer&) = delete;
    PerformanceTimer& operator=(const PerformanceTimer&) = delete;

    // Move constructor and assignment
    PerformanceTimer(PerformanceTimer&&) noexcept = default;
    PerformanceTimer& operator=(PerformanceTimer&&) noexcept = default;

private:
    std::string operation_name_;
    PerformanceMonitor& monitor_;
    std::chrono::steady_clock::time_point start_time_;
};

/**
 * @brief Get global configuration manager
 * @return Reference to global configuration manager
 */
ATOM_NODISCARD auto getGlobalConfigManager() -> WiFiConfigManager&;

/**
 * @brief Get global performance monitor
 * @return Reference to global performance monitor
 */
ATOM_NODISCARD auto getGlobalPerformanceMonitor() -> PerformanceMonitor&;

/**
 * @brief Initialize configuration system
 * @param config_file Optional configuration file path
 * @return True if initialization was successful
 */
ATOM_NODISCARD auto initializeConfiguration(const std::string& config_file = "") -> bool;

/**
 * @brief Shutdown configuration system
 */
void shutdownConfiguration();

} // namespace atom::system

// Convenience macros for performance monitoring
#define WIFI_PERF_TIMER(operation_name) \
    atom::system::PerformanceTimer perf_timer(operation_name, atom::system::getGlobalPerformanceMonitor())

#define WIFI_RECORD_MEMORY(operation_name, bytes) \
    atom::system::getGlobalPerformanceMonitor().recordMemoryUsage(operation_name, bytes)

#define WIFI_RECORD_CACHE_HIT(cache_name) \
    atom::system::getGlobalPerformanceMonitor().recordCacheAccess(cache_name, true)

#define WIFI_RECORD_CACHE_MISS(cache_name) \
    atom::system::getGlobalPerformanceMonitor().recordCacheAccess(cache_name, false)

#endif // ATOM_SYSTEM_MODULE_WIFI_CONFIG_HPP
