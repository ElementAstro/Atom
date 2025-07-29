/*
 * config.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - WiFi Configuration Implementation

**************************************************/

#include "config.hpp"
#include "atom/log/loguru.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <thread>

namespace atom::system {

WiFiConfigManager::WiFiConfigManager() {
    // Initialize with default values (already done by struct initialization)
    LOG_F(INFO, "WiFiConfigManager initialized with default configuration");
}

auto WiFiConfigManager::loadFromFile(const std::string& config_file) -> bool {
    std::lock_guard lock(config_mutex_);
    
    std::ifstream file(config_file);
    if (!file.is_open()) {
        LOG_F(ERROR, "Failed to open configuration file: {}", config_file);
        return false;
    }
    
    std::string line;
    int line_number = 0;
    
    while (std::getline(file, line)) {
        line_number++;
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse key=value pairs
        size_t equals_pos = line.find('=');
        if (equals_pos == std::string::npos) {
            LOG_F(WARNING, "Invalid configuration line {} in {}: {}", line_number, config_file, line);
            continue;
        }
        
        std::string key = line.substr(0, equals_pos);
        std::string value = line.substr(equals_pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        if (!setParameter(key, value)) {
            LOG_F(WARNING, "Failed to set configuration parameter: {} = {}", key, value);
        }
    }
    
    LOG_F(INFO, "Configuration loaded from file: {}", config_file);
    notifyConfigurationChange();
    return true;
}

auto WiFiConfigManager::saveToFile(const std::string& config_file) const -> bool {
    std::lock_guard lock(config_mutex_);
    
    std::ofstream file(config_file);
    if (!file.is_open()) {
        LOG_F(ERROR, "Failed to create configuration file: {}", config_file);
        return false;
    }
    
    file << "# WiFi Module Configuration\n";
    file << "# Generated automatically - modify with care\n\n";
    
    // Write timeout settings
    file << "# Timeout settings (milliseconds)\n";
    file << "ping_timeout=" << wifi_config_.ping_timeout.count() << "\n";
    file << "command_timeout=" << wifi_config_.command_timeout.count() << "\n";
    file << "connection_timeout=" << wifi_config_.connection_timeout.count() << "\n";
    file << "scan_timeout=" << wifi_config_.scan_timeout.count() << "\n\n";
    
    // Write cache settings
    file << "# Cache settings\n";
    file << "cache_ttl_wifi_info=" << wifi_config_.cache_ttl_wifi_info.count() << "\n";
    file << "cache_ttl_network_stats=" << wifi_config_.cache_ttl_network_stats.count() << "\n";
    file << "cache_ttl_interface_list=" << wifi_config_.cache_ttl_interface_list.count() << "\n";
    file << "cache_ttl_available_networks=" << wifi_config_.cache_ttl_available_networks.count() << "\n";
    file << "max_cache_size=" << wifi_config_.max_cache_size << "\n\n";
    
    // Write performance settings
    file << "# Performance settings\n";
    file << "max_concurrent_operations=" << wifi_config_.max_concurrent_operations << "\n";
    file << "buffer_size=" << wifi_config_.buffer_size << "\n";
    file << "retry_attempts=" << wifi_config_.retry_attempts << "\n";
    file << "retry_delay=" << wifi_config_.retry_delay.count() << "\n\n";
    
    // Write monitoring settings
    file << "# Monitoring settings\n";
    file << "monitoring_interval=" << wifi_config_.monitoring_interval.count() << "\n";
    file << "quality_assessment_interval=" << wifi_config_.quality_assessment_interval.count() << "\n";
    file << "max_history_size=" << wifi_config_.max_history_size << "\n";
    file << "enable_background_monitoring=" << (wifi_config_.enable_background_monitoring ? "true" : "false") << "\n\n";
    
    // Write host settings
    file << "# Host settings\n";
    file << "preferred_ping_host=" << wifi_config_.preferred_ping_host << "\n";
    file << "backup_ping_host=" << wifi_config_.backup_ping_host << "\n";
    file << "speed_test_server=" << wifi_config_.speed_test_server << "\n\n";
    
    LOG_F(INFO, "Configuration saved to file: {}", config_file);
    return true;
}

void WiFiConfigManager::loadFromEnvironment() {
    std::lock_guard lock(config_mutex_);
    
    // Check for environment variable overrides
    const char* env_ping_timeout = std::getenv("WIFI_PING_TIMEOUT");
    if (env_ping_timeout) {
        wifi_config_.ping_timeout = parseTimeValue(env_ping_timeout);
    }
    
    const char* env_cache_size = std::getenv("WIFI_MAX_CACHE_SIZE");
    if (env_cache_size) {
        wifi_config_.max_cache_size = parseSizeValue(env_cache_size);
    }
    
    const char* env_monitoring = std::getenv("WIFI_ENABLE_MONITORING");
    if (env_monitoring) {
        wifi_config_.enable_background_monitoring = parseBoolValue(env_monitoring);
    }
    
    const char* env_ping_host = std::getenv("WIFI_PING_HOST");
    if (env_ping_host) {
        wifi_config_.preferred_ping_host = env_ping_host;
    }
    
    LOG_F(INFO, "Configuration loaded from environment variables");
    notifyConfigurationChange();
}

auto WiFiConfigManager::getWiFiConfig() const -> const WiFiConfig& {
    std::lock_guard lock(config_mutex_);
    return wifi_config_;
}

auto WiFiConfigManager::getPerformanceConfig() const -> const PerformanceConfig& {
    std::lock_guard lock(config_mutex_);
    return performance_config_;
}

void WiFiConfigManager::updateWiFiConfig(const WiFiConfig& config) {
    std::lock_guard lock(config_mutex_);
    wifi_config_ = config;
    LOG_F(INFO, "WiFi configuration updated");
    notifyConfigurationChange();
}

void WiFiConfigManager::updatePerformanceConfig(const PerformanceConfig& config) {
    std::lock_guard lock(config_mutex_);
    performance_config_ = config;
    LOG_F(INFO, "Performance configuration updated");
    notifyConfigurationChange();
}

auto WiFiConfigManager::setParameter(const std::string& key, const std::string& value) -> bool {
    // Timeout settings
    if (key == "ping_timeout") {
        wifi_config_.ping_timeout = parseTimeValue(value);
        return true;
    } else if (key == "command_timeout") {
        wifi_config_.command_timeout = parseTimeValue(value);
        return true;
    } else if (key == "connection_timeout") {
        wifi_config_.connection_timeout = parseTimeValue(value);
        return true;
    } else if (key == "scan_timeout") {
        wifi_config_.scan_timeout = parseTimeValue(value);
        return true;
    }
    
    // Cache settings
    else if (key == "cache_ttl_wifi_info") {
        wifi_config_.cache_ttl_wifi_info = std::chrono::seconds(std::stoi(value));
        return true;
    } else if (key == "cache_ttl_network_stats") {
        wifi_config_.cache_ttl_network_stats = std::chrono::seconds(std::stoi(value));
        return true;
    } else if (key == "max_cache_size") {
        wifi_config_.max_cache_size = parseSizeValue(value);
        return true;
    }
    
    // Performance settings
    else if (key == "max_concurrent_operations") {
        wifi_config_.max_concurrent_operations = std::stoi(value);
        return true;
    } else if (key == "buffer_size") {
        wifi_config_.buffer_size = parseSizeValue(value);
        return true;
    } else if (key == "retry_attempts") {
        wifi_config_.retry_attempts = std::stoi(value);
        return true;
    }
    
    // Boolean settings
    else if (key == "enable_background_monitoring") {
        wifi_config_.enable_background_monitoring = parseBoolValue(value);
        return true;
    } else if (key == "enable_ipv6") {
        wifi_config_.enable_ipv6 = parseBoolValue(value);
        return true;
    }
    
    // String settings
    else if (key == "preferred_ping_host") {
        wifi_config_.preferred_ping_host = value;
        return true;
    } else if (key == "backup_ping_host") {
        wifi_config_.backup_ping_host = value;
        return true;
    } else if (key == "speed_test_server") {
        wifi_config_.speed_test_server = value;
        return true;
    }
    
    LOG_F(WARNING, "Unknown configuration parameter: {}", key);
    return false;
}

auto WiFiConfigManager::getParameter(const std::string& key) const -> std::string {
    std::lock_guard lock(config_mutex_);
    
    if (key == "ping_timeout") {
        return std::to_string(wifi_config_.ping_timeout.count());
    } else if (key == "max_cache_size") {
        return std::to_string(wifi_config_.max_cache_size);
    } else if (key == "enable_background_monitoring") {
        return wifi_config_.enable_background_monitoring ? "true" : "false";
    } else if (key == "preferred_ping_host") {
        return wifi_config_.preferred_ping_host;
    }
    
    return "";
}

void WiFiConfigManager::resetToDefaults() {
    std::lock_guard lock(config_mutex_);
    wifi_config_ = WiFiConfig{};
    performance_config_ = PerformanceConfig{};
    LOG_F(INFO, "Configuration reset to defaults");
    notifyConfigurationChange();
}

auto WiFiConfigManager::validateConfiguration() const -> std::vector<std::string> {
    std::lock_guard lock(config_mutex_);
    std::vector<std::string> errors;
    
    // Validate timeout values
    if (wifi_config_.ping_timeout.count() <= 0) {
        errors.push_back("ping_timeout must be positive");
    }
    if (wifi_config_.command_timeout.count() <= 0) {
        errors.push_back("command_timeout must be positive");
    }
    
    // Validate cache settings
    if (wifi_config_.max_cache_size == 0) {
        errors.push_back("max_cache_size must be greater than 0");
    }
    
    // Validate performance settings
    if (wifi_config_.max_concurrent_operations <= 0) {
        errors.push_back("max_concurrent_operations must be positive");
    }
    if (wifi_config_.buffer_size == 0) {
        errors.push_back("buffer_size must be greater than 0");
    }
    
    // Validate host settings
    if (wifi_config_.preferred_ping_host.empty()) {
        errors.push_back("preferred_ping_host cannot be empty");
    }
    
    return errors;
}

auto WiFiConfigManager::toString() const -> std::string {
    std::lock_guard lock(config_mutex_);
    std::ostringstream oss;
    
    oss << "WiFi Configuration:\n";
    oss << "  Ping Timeout: " << wifi_config_.ping_timeout.count() << "ms\n";
    oss << "  Command Timeout: " << wifi_config_.command_timeout.count() << "ms\n";
    oss << "  Max Cache Size: " << wifi_config_.max_cache_size << "\n";
    oss << "  Max Concurrent Operations: " << wifi_config_.max_concurrent_operations << "\n";
    oss << "  Buffer Size: " << wifi_config_.buffer_size << " bytes\n";
    oss << "  Preferred Ping Host: " << wifi_config_.preferred_ping_host << "\n";
    oss << "  Background Monitoring: " << (wifi_config_.enable_background_monitoring ? "enabled" : "disabled") << "\n";
    
    return oss.str();
}

void WiFiConfigManager::autoTune() {
    std::lock_guard lock(config_mutex_);
    
    // Auto-detect optimal thread count
    unsigned int hardware_threads = std::thread::hardware_concurrency();
    if (hardware_threads > 0) {
        wifi_config_.max_concurrent_operations = std::min(static_cast<int>(hardware_threads), 8);
        performance_config_.thread_pool_size = hardware_threads;
    }
    
    // Adjust buffer size based on available memory (simplified)
    wifi_config_.buffer_size = 65536; // Default to 64KB
    
    // Enable optimizations based on system capabilities
    performance_config_.enable_parallel_processing = (hardware_threads > 1);
    performance_config_.enable_async_io = true;
    
    LOG_F(INFO, "Configuration auto-tuned for system capabilities");
    notifyConfigurationChange();
}

void WiFiConfigManager::registerChangeCallback(std::function<void(const WiFiConfig&, const PerformanceConfig&)> callback) {
    std::lock_guard lock(config_mutex_);
    change_callbacks_.push_back(std::move(callback));
    LOG_F(INFO, "Configuration change callback registered");
}

void WiFiConfigManager::notifyConfigurationChange() {
    std::lock_guard lock(config_mutex_);
    for (const auto& callback : change_callbacks_) {
        try {
            callback(wifi_config_, performance_config_);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error in configuration change callback: {}", e.what());
        }
    }
}

auto WiFiConfigManager::parseTimeValue(const std::string& value) -> std::chrono::milliseconds {
    try {
        return std::chrono::milliseconds(std::stoi(value));
    } catch (const std::exception&) {
        LOG_F(ERROR, "Invalid time value: {}", value);
        return std::chrono::milliseconds(5000); // Default fallback
    }
}

auto WiFiConfigManager::parseSizeValue(const std::string& value) -> size_t {
    try {
        return static_cast<size_t>(std::stoull(value));
    } catch (const std::exception&) {
        LOG_F(ERROR, "Invalid size value: {}", value);
        return 100; // Default fallback
    }
}

auto WiFiConfigManager::parseBoolValue(const std::string& value) -> bool {
    std::string lower_value = value;
    std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
    return (lower_value == "true" || lower_value == "1" || lower_value == "yes" || lower_value == "on");
}

// PerformanceMonitor implementation
void PerformanceMonitor::recordTiming(const std::string& operation_name, std::chrono::microseconds duration) {
    std::lock_guard lock(monitor_mutex_);
    timing_data_[operation_name].push_back({std::chrono::steady_clock::now(), duration});

    // Maintain reasonable history size
    auto& records = timing_data_[operation_name];
    if (records.size() > 1000) {
        records.erase(records.begin(), records.begin() + 100);
    }
}

void PerformanceMonitor::recordMemoryUsage(const std::string& operation_name, size_t memory_bytes) {
    std::lock_guard lock(monitor_mutex_);
    memory_data_[operation_name].push_back({std::chrono::steady_clock::now(), memory_bytes});

    // Maintain reasonable history size
    auto& records = memory_data_[operation_name];
    if (records.size() > 1000) {
        records.erase(records.begin(), records.begin() + 100);
    }
}

void PerformanceMonitor::recordCacheAccess(const std::string& cache_name, bool hit) {
    std::lock_guard lock(monitor_mutex_);
    cache_data_[cache_name].push_back({std::chrono::steady_clock::now(), hit});

    // Maintain reasonable history size
    auto& records = cache_data_[cache_name];
    if (records.size() > 1000) {
        records.erase(records.begin(), records.begin() + 100);
    }
}

auto PerformanceMonitor::getPerformanceStats(std::chrono::hours duration) const -> std::string {
    std::lock_guard lock(monitor_mutex_);
    std::ostringstream stats;

    auto cutoff_time = std::chrono::steady_clock::now() - duration;

    stats << "Performance Statistics (Last " << duration.count() << " hours)\n";
    stats << "=================================================\n\n";

    // Timing statistics
    stats << "Operation Timings:\n";
    for (const auto& [operation, records] : timing_data_) {
        std::vector<std::chrono::microseconds> recent_timings;
        for (const auto& record : records) {
            if (record.timestamp >= cutoff_time) {
                recent_timings.push_back(record.duration);
            }
        }

        if (!recent_timings.empty()) {
            auto total = std::accumulate(recent_timings.begin(), recent_timings.end(), std::chrono::microseconds(0));
            auto avg = total / recent_timings.size();
            auto min_time = *std::min_element(recent_timings.begin(), recent_timings.end());
            auto max_time = *std::max_element(recent_timings.begin(), recent_timings.end());

            stats << "  " << operation << ":\n";
            stats << "    Count: " << recent_timings.size() << "\n";
            stats << "    Average: " << avg.count() << " μs\n";
            stats << "    Min: " << min_time.count() << " μs\n";
            stats << "    Max: " << max_time.count() << " μs\n\n";
        }
    }

    // Cache statistics
    stats << "Cache Statistics:\n";
    for (const auto& [cache_name, records] : cache_data_) {
        int hits = 0, total = 0;
        for (const auto& record : records) {
            if (record.timestamp >= cutoff_time) {
                if (record.hit) hits++;
                total++;
            }
        }

        if (total > 0) {
            double hit_ratio = static_cast<double>(hits) / total;
            stats << "  " << cache_name << ":\n";
            stats << "    Total Accesses: " << total << "\n";
            stats << "    Hits: " << hits << "\n";
            stats << "    Hit Ratio: " << std::fixed << std::setprecision(2) << (hit_ratio * 100) << "%\n\n";
        }
    }

    return stats.str();
}

auto PerformanceMonitor::getCacheHitRatio(const std::string& cache_name, std::chrono::hours duration) const -> double {
    std::lock_guard lock(monitor_mutex_);

    auto it = cache_data_.find(cache_name);
    if (it == cache_data_.end()) {
        return 0.0;
    }

    auto cutoff_time = std::chrono::steady_clock::now() - duration;
    int hits = 0, total = 0;

    for (const auto& record : it->second) {
        if (record.timestamp >= cutoff_time) {
            if (record.hit) hits++;
            total++;
        }
    }

    return (total > 0) ? (static_cast<double>(hits) / total) : 0.0;
}

auto PerformanceMonitor::getAverageOperationTime(const std::string& operation_name, std::chrono::hours duration) const -> std::chrono::microseconds {
    std::lock_guard lock(monitor_mutex_);

    auto it = timing_data_.find(operation_name);
    if (it == timing_data_.end()) {
        return std::chrono::microseconds(0);
    }

    auto cutoff_time = std::chrono::steady_clock::now() - duration;
    std::vector<std::chrono::microseconds> recent_timings;

    for (const auto& record : it->second) {
        if (record.timestamp >= cutoff_time) {
            recent_timings.push_back(record.duration);
        }
    }

    if (recent_timings.empty()) {
        return std::chrono::microseconds(0);
    }

    auto total = std::accumulate(recent_timings.begin(), recent_timings.end(), std::chrono::microseconds(0));
    return total / recent_timings.size();
}

void PerformanceMonitor::clearData() {
    std::lock_guard lock(monitor_mutex_);
    timing_data_.clear();
    memory_data_.clear();
    cache_data_.clear();
    LOG_F(INFO, "Performance monitoring data cleared");
}

// PerformanceTimer implementation
PerformanceTimer::PerformanceTimer(const std::string& operation_name, PerformanceMonitor& monitor)
    : operation_name_(operation_name), monitor_(monitor), start_time_(std::chrono::steady_clock::now()) {
}

PerformanceTimer::~PerformanceTimer() {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
    monitor_.recordTiming(operation_name_, duration);
}

// Global instances
static std::unique_ptr<WiFiConfigManager> global_config_manager;
static std::unique_ptr<PerformanceMonitor> global_performance_monitor;
static std::mutex global_config_mutex;

auto getGlobalConfigManager() -> WiFiConfigManager& {
    std::lock_guard lock(global_config_mutex);
    if (!global_config_manager) {
        global_config_manager = std::make_unique<WiFiConfigManager>();
    }
    return *global_config_manager;
}

auto getGlobalPerformanceMonitor() -> PerformanceMonitor& {
    std::lock_guard lock(global_config_mutex);
    if (!global_performance_monitor) {
        global_performance_monitor = std::make_unique<PerformanceMonitor>();
    }
    return *global_performance_monitor;
}

auto initializeConfiguration(const std::string& config_file) -> bool {
    std::lock_guard lock(global_config_mutex);

    try {
        global_config_manager = std::make_unique<WiFiConfigManager>();
        global_performance_monitor = std::make_unique<PerformanceMonitor>();

        // Load configuration from environment
        global_config_manager->loadFromEnvironment();

        // Load from file if specified
        if (!config_file.empty()) {
            if (!global_config_manager->loadFromFile(config_file)) {
                LOG_F(WARNING, "Failed to load configuration from file: {}", config_file);
            }
        }

        // Auto-tune configuration
        global_config_manager->autoTune();

        LOG_F(INFO, "WiFi configuration system initialized successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to initialize WiFi configuration system: {}", e.what());
        return false;
    }
}

void shutdownConfiguration() {
    std::lock_guard lock(global_config_mutex);

    if (global_config_manager) {
        global_config_manager.reset();
    }
    if (global_performance_monitor) {
        global_performance_monitor.reset();
    }

    LOG_F(INFO, "WiFi configuration system shutdown completed");
}

} // namespace atom::system
