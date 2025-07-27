#include "config.hpp"
#include <fstream>
#include <sstream>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cstdlib>
#include <spdlog/spdlog.h>

namespace atom::system::locale {

namespace {
    // Helper function to parse environment variable as boolean
    bool parseEnvBool(const char* envVar, bool defaultValue = false) {
        const char* value = std::getenv(envVar);
        if (!value) return defaultValue;
        
        std::string str(value);
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str == "true" || str == "1" || str == "yes" || str == "on";
    }
    
    // Helper function to parse environment variable as integer
    int parseEnvInt(const char* envVar, int defaultValue = 0) {
        const char* value = std::getenv(envVar);
        if (!value) return defaultValue;
        
        try {
            return std::stoi(value);
        } catch (...) {
            return defaultValue;
        }
    }
    
    // Helper function to parse environment variable as string
    std::string parseEnvString(const char* envVar, const std::string& defaultValue = "") {
        const char* value = std::getenv(envVar);
        return value ? std::string(value) : defaultValue;
    }
}

// LocaleConfigManager implementation
auto LocaleConfigManager::getInstance() -> LocaleConfigManager& {
    static LocaleConfigManager instance;
    return instance;
}

auto LocaleConfigManager::loadFromFile(const std::string& filePath) -> bool {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            spdlog::warn("Could not open config file: {}", filePath);
            return false;
        }
        
        // Simple key=value parser (in a real implementation, you might use JSON or YAML)
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                
                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                updateConfig(key, value);
            }
        }
        
        config_.configFilePath = filePath;
        spdlog::info("Loaded configuration from: {}", filePath);
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Error loading config file {}: {}", filePath, e.what());
        return false;
    }
}

auto LocaleConfigManager::saveToFile(const std::string& filePath) -> bool {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            spdlog::error("Could not create config file: {}", filePath);
            return false;
        }
        
        file << "# Locale System Configuration\n";
        file << "# Generated automatically\n\n";
        
        file << "default_locale=" << config_.defaultLocale << "\n";
        file << "log_level=" << static_cast<int>(config_.logLevel) << "\n";
        file << "enable_validation=" << (config_.enableValidation ? "true" : "false") << "\n";
        file << "enable_compatibility_checks=" << (config_.enableCompatibilityChecks ? "true" : "false") << "\n";
        
        file << "\n# Cache Configuration\n";
        file << "cache_strategy=" << static_cast<int>(config_.cache.strategy) << "\n";
        file << "cache_timeout=" << config_.cache.timeout.count() << "\n";
        file << "cache_max_memory_entries=" << config_.cache.maxMemoryEntries << "\n";
        file << "cache_max_memory_size=" << config_.cache.maxMemorySize << "\n";
        file << "cache_enable_compression=" << (config_.cache.enableCompression ? "true" : "false") << "\n";
        
        file << "\n# Performance Configuration\n";
        file << "perf_enable_lazy_loading=" << (config_.performance.enableLazyLoading ? "true" : "false") << "\n";
        file << "perf_enable_preloading=" << (config_.performance.enablePreloading ? "true" : "false") << "\n";
        file << "perf_thread_pool_size=" << config_.performance.threadPoolSize << "\n";
        file << "perf_enable_async=" << (config_.performance.enableAsyncOperations ? "true" : "false") << "\n";
        file << "perf_operation_timeout=" << config_.performance.operationTimeout.count() << "\n";
        file << "perf_enable_metrics=" << (config_.performance.enableMetrics ? "true" : "false") << "\n";
        
        spdlog::info("Saved configuration to: {}", filePath);
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Error saving config file {}: {}", filePath, e.what());
        return false;
    }
}

void LocaleConfigManager::loadFromEnvironment() {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    // Load basic configuration
    config_.defaultLocale = parseEnvString("LOCALE_DEFAULT", config_.defaultLocale);
    config_.enableValidation = parseEnvBool("LOCALE_ENABLE_VALIDATION", config_.enableValidation);
    config_.enableCompatibilityChecks = parseEnvBool("LOCALE_ENABLE_COMPATIBILITY", config_.enableCompatibilityChecks);
    
    // Load cache configuration
    int cacheStrategy = parseEnvInt("LOCALE_CACHE_STRATEGY", static_cast<int>(config_.cache.strategy));
    if (cacheStrategy >= 0 && cacheStrategy <= 3) {
        config_.cache.strategy = static_cast<CacheStrategy>(cacheStrategy);
    }
    
    int cacheTimeout = parseEnvInt("LOCALE_CACHE_TIMEOUT", config_.cache.timeout.count());
    if (cacheTimeout > 0) {
        config_.cache.timeout = std::chrono::seconds(cacheTimeout);
    }
    
    config_.cache.maxMemoryEntries = parseEnvInt("LOCALE_CACHE_MAX_ENTRIES", config_.cache.maxMemoryEntries);
    config_.cache.enableCompression = parseEnvBool("LOCALE_CACHE_COMPRESSION", config_.cache.enableCompression);
    config_.cache.persistentCachePath = parseEnvString("LOCALE_CACHE_PATH", config_.cache.persistentCachePath);
    
    // Load performance configuration
    config_.performance.enableLazyLoading = parseEnvBool("LOCALE_LAZY_LOADING", config_.performance.enableLazyLoading);
    config_.performance.enablePreloading = parseEnvBool("LOCALE_PRELOADING", config_.performance.enablePreloading);
    config_.performance.threadPoolSize = parseEnvInt("LOCALE_THREAD_POOL_SIZE", config_.performance.threadPoolSize);
    config_.performance.enableAsyncOperations = parseEnvBool("LOCALE_ASYNC_OPS", config_.performance.enableAsyncOperations);
    config_.performance.enableMetrics = parseEnvBool("LOCALE_METRICS", config_.performance.enableMetrics);
    
    int operationTimeout = parseEnvInt("LOCALE_OPERATION_TIMEOUT", config_.performance.operationTimeout.count());
    if (operationTimeout > 0) {
        config_.performance.operationTimeout = std::chrono::milliseconds(operationTimeout);
    }
    
    // Load log level
    int logLevel = parseEnvInt("LOCALE_LOG_LEVEL", static_cast<int>(config_.logLevel));
    if (logLevel >= 0 && logLevel <= 5) {
        config_.logLevel = static_cast<LogLevel>(logLevel);
    }
    
    spdlog::info("Loaded configuration from environment variables");
}

auto LocaleConfigManager::getConfig() -> const LocaleConfig& {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

void LocaleConfigManager::setConfig(const LocaleConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
}

void LocaleConfigManager::updateConfig(const std::string& key, const std::string& value) {
    if (key == "default_locale") {
        config_.defaultLocale = value;
    } else if (key == "log_level") {
        try {
            int level = std::stoi(value);
            if (level >= 0 && level <= 5) {
                config_.logLevel = static_cast<LogLevel>(level);
            }
        } catch (...) {}
    } else if (key == "enable_validation") {
        config_.enableValidation = (value == "true" || value == "1");
    } else if (key == "enable_compatibility_checks") {
        config_.enableCompatibilityChecks = (value == "true" || value == "1");
    } else if (key == "cache_strategy") {
        try {
            int strategy = std::stoi(value);
            if (strategy >= 0 && strategy <= 3) {
                config_.cache.strategy = static_cast<CacheStrategy>(strategy);
            }
        } catch (...) {}
    } else if (key == "cache_timeout") {
        try {
            int timeout = std::stoi(value);
            if (timeout > 0) {
                config_.cache.timeout = std::chrono::seconds(timeout);
            }
        } catch (...) {}
    } else if (key == "cache_max_memory_entries") {
        try {
            config_.cache.maxMemoryEntries = std::stoul(value);
        } catch (...) {}
    } else if (key == "cache_max_memory_size") {
        try {
            config_.cache.maxMemorySize = std::stoul(value);
        } catch (...) {}
    } else if (key == "cache_enable_compression") {
        config_.cache.enableCompression = (value == "true" || value == "1");
    } else if (key == "perf_enable_lazy_loading") {
        config_.performance.enableLazyLoading = (value == "true" || value == "1");
    } else if (key == "perf_enable_preloading") {
        config_.performance.enablePreloading = (value == "true" || value == "1");
    } else if (key == "perf_thread_pool_size") {
        try {
            config_.performance.threadPoolSize = std::stoul(value);
        } catch (...) {}
    } else if (key == "perf_enable_async") {
        config_.performance.enableAsyncOperations = (value == "true" || value == "1");
    } else if (key == "perf_operation_timeout") {
        try {
            int timeout = std::stoi(value);
            if (timeout > 0) {
                config_.performance.operationTimeout = std::chrono::milliseconds(timeout);
            }
        } catch (...) {}
    } else if (key == "perf_enable_metrics") {
        config_.performance.enableMetrics = (value == "true" || value == "1");
    }
}

void LocaleConfigManager::resetToDefaults() {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = LocaleConfig{};
}

auto LocaleConfigManager::validateConfig() -> bool {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    // Basic validation
    if (config_.defaultLocale.empty()) {
        spdlog::error("Default locale cannot be empty");
        return false;
    }
    
    if (config_.cache.maxMemoryEntries == 0) {
        spdlog::warn("Cache max memory entries is 0, caching will be ineffective");
    }
    
    if (config_.performance.threadPoolSize == 0) {
        spdlog::warn("Thread pool size is 0, async operations will be disabled");
        config_.performance.enableAsyncOperations = false;
    }
    
    return true;
}

auto LocaleConfigManager::toJson() -> std::string {
    // Simplified JSON serialization (in a real implementation, use a proper JSON library)
    std::ostringstream json;
    json << "{\n";
    json << "  \"defaultLocale\": \"" << config_.defaultLocale << "\",\n";
    json << "  \"enableValidation\": " << (config_.enableValidation ? "true" : "false") << ",\n";
    json << "  \"enableCompatibilityChecks\": " << (config_.enableCompatibilityChecks ? "true" : "false") << ",\n";
    json << "  \"logLevel\": " << static_cast<int>(config_.logLevel) << ",\n";
    json << "  \"cache\": {\n";
    json << "    \"strategy\": " << static_cast<int>(config_.cache.strategy) << ",\n";
    json << "    \"timeout\": " << config_.cache.timeout.count() << ",\n";
    json << "    \"maxMemoryEntries\": " << config_.cache.maxMemoryEntries << ",\n";
    json << "    \"enableCompression\": " << (config_.cache.enableCompression ? "true" : "false") << "\n";
    json << "  },\n";
    json << "  \"performance\": {\n";
    json << "    \"enableLazyLoading\": " << (config_.performance.enableLazyLoading ? "true" : "false") << ",\n";
    json << "    \"enablePreloading\": " << (config_.performance.enablePreloading ? "true" : "false") << ",\n";
    json << "    \"threadPoolSize\": " << config_.performance.threadPoolSize << ",\n";
    json << "    \"enableAsyncOperations\": " << (config_.performance.enableAsyncOperations ? "true" : "false") << ",\n";
    json << "    \"enableMetrics\": " << (config_.performance.enableMetrics ? "true" : "false") << "\n";
    json << "  }\n";
    json << "}";
    return json.str();
}

auto LocaleConfigManager::fromJson([[maybe_unused]] const std::string& json) -> bool {
    // Simplified JSON parsing (in a real implementation, use a proper JSON library)
    // This is just a placeholder implementation
    spdlog::warn("JSON configuration loading not fully implemented");
    return false;
}

// LocaleCache implementation
class LocaleCache::Impl {
public:
    struct CacheEntry {
        LocaleInfo info;
        std::chrono::system_clock::time_point timestamp;
        size_t accessCount{0};
        std::chrono::system_clock::time_point lastAccess;
    };

    CacheConfig config_;
    std::unordered_map<std::string, CacheEntry> memoryCache_;
    mutable std::mutex cacheMutex_;
    std::atomic<size_t> hits_{0};
    std::atomic<size_t> misses_{0};
    std::atomic<size_t> totalSize_{0};

    explicit Impl(const CacheConfig& config) : config_(config) {}

    void store(const std::string& key, const LocaleInfo& info) {
        std::lock_guard<std::mutex> lock(cacheMutex_);

        if (config_.strategy == CacheStrategy::None) return;

        auto now = std::chrono::system_clock::now();
        CacheEntry entry{info, now, 1, now};

        // Check memory limits
        if (memoryCache_.size() >= config_.maxMemoryEntries) {
            evictOldest();
        }

        memoryCache_[key] = std::move(entry);
        updateTotalSize();
    }

    auto retrieve(const std::string& key) -> std::optional<LocaleInfo> {
        std::lock_guard<std::mutex> lock(cacheMutex_);

        if (config_.strategy == CacheStrategy::None) {
            misses_++;
            return std::nullopt;
        }

        auto it = memoryCache_.find(key);
        if (it != memoryCache_.end()) {
            auto now = std::chrono::system_clock::now();

            // Check if entry has expired
            if ((now - it->second.timestamp) > config_.timeout) {
                memoryCache_.erase(it);
                misses_++;
                updateTotalSize();
                return std::nullopt;
            }

            // Update access information
            it->second.accessCount++;
            it->second.lastAccess = now;
            hits_++;

            return it->second.info;
        }

        misses_++;
        return std::nullopt;
    }

    auto exists(const std::string& key) -> bool {
        std::lock_guard<std::mutex> lock(cacheMutex_);

        auto it = memoryCache_.find(key);
        if (it != memoryCache_.end()) {
            auto now = std::chrono::system_clock::now();
            if ((now - it->second.timestamp) <= config_.timeout) {
                return true;
            } else {
                memoryCache_.erase(it);
                updateTotalSize();
            }
        }

        return false;
    }

    void remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        memoryCache_.erase(key);
        updateTotalSize();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        memoryCache_.clear();
        totalSize_ = 0;
        hits_ = 0;
        misses_ = 0;
    }

    auto getStatistics() -> std::unordered_map<std::string, size_t> {
        std::lock_guard<std::mutex> lock(cacheMutex_);

        return {
            {"entries", memoryCache_.size()},
            {"hits", hits_.load()},
            {"misses", misses_.load()},
            {"total_size", totalSize_.load()},
            {"max_entries", config_.maxMemoryEntries},
            {"max_size", config_.maxMemorySize}
        };
    }

    void maintenance() {
        std::lock_guard<std::mutex> lock(cacheMutex_);

        auto now = std::chrono::system_clock::now();
        auto it = memoryCache_.begin();

        while (it != memoryCache_.end()) {
            if ((now - it->second.timestamp) > config_.timeout) {
                it = memoryCache_.erase(it);
            } else {
                ++it;
            }
        }

        updateTotalSize();
    }

private:
    void evictOldest() {
        if (memoryCache_.empty()) return;

        auto oldest = memoryCache_.begin();
        for (auto it = memoryCache_.begin(); it != memoryCache_.end(); ++it) {
            if (it->second.lastAccess < oldest->second.lastAccess) {
                oldest = it;
            }
        }

        memoryCache_.erase(oldest);
    }

    void updateTotalSize() {
        // Simplified size calculation
        totalSize_ = memoryCache_.size() * sizeof(CacheEntry);
    }
};

LocaleCache::LocaleCache(const CacheConfig& config) : pImpl(std::make_unique<Impl>(config)) {}
LocaleCache::~LocaleCache() = default;

void LocaleCache::store(const std::string& key, const LocaleInfo& info) {
    pImpl->store(key, info);
}

auto LocaleCache::retrieve(const std::string& key) -> std::optional<LocaleInfo> {
    return pImpl->retrieve(key);
}

auto LocaleCache::exists(const std::string& key) -> bool {
    return pImpl->exists(key);
}

void LocaleCache::remove(const std::string& key) {
    pImpl->remove(key);
}

void LocaleCache::clear() {
    pImpl->clear();
}

auto LocaleCache::getStatistics() -> std::unordered_map<std::string, size_t> {
    return pImpl->getStatistics();
}

void LocaleCache::maintenance() {
    pImpl->maintenance();
}

void LocaleCache::setConfig(const CacheConfig& config) {
    std::lock_guard<std::mutex> lock(pImpl->cacheMutex_);
    pImpl->config_ = config;
}

// PerformanceMonitor implementation
class PerformanceMonitor::Impl {
public:
    std::unordered_map<std::string, Metrics> metrics_;
    mutable std::mutex metricsMutex_;
    std::atomic<bool> enabled_{false};

    void recordOperation(const std::string& operation, double executionTime, bool success) {
        if (!enabled_) return;

        std::lock_guard<std::mutex> lock(metricsMutex_);
        auto& metric = metrics_[operation];

        metric.totalOperations++;
        if (success) {
            metric.successfulOperations++;
        } else {
            metric.failedOperations++;
        }

        // Update execution time statistics
        if (metric.totalOperations == 1) {
            metric.minExecutionTime = executionTime;
            metric.maxExecutionTime = executionTime;
            metric.averageExecutionTime = executionTime;
        } else {
            metric.minExecutionTime = std::min(metric.minExecutionTime, executionTime);
            metric.maxExecutionTime = std::max(metric.maxExecutionTime, executionTime);

            // Update running average
            double totalTime = metric.averageExecutionTime * (metric.totalOperations - 1) + executionTime;
            metric.averageExecutionTime = totalTime / metric.totalOperations;
        }
    }

    void recordCacheHit(const std::string& operation) {
        if (!enabled_) return;

        std::lock_guard<std::mutex> lock(metricsMutex_);
        metrics_[operation].cacheHits++;
    }

    void recordCacheMiss(const std::string& operation) {
        if (!enabled_) return;

        std::lock_guard<std::mutex> lock(metricsMutex_);
        metrics_[operation].cacheMisses++;
    }

    auto getMetrics(const std::string& operation) -> Metrics {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        auto it = metrics_.find(operation);
        return (it != metrics_.end()) ? it->second : Metrics{};
    }

    auto getAllMetrics() -> std::unordered_map<std::string, Metrics> {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        return metrics_;
    }

    void resetMetrics(const std::string& operation) {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        auto it = metrics_.find(operation);
        if (it != metrics_.end()) {
            it->second = Metrics{};
            it->second.lastReset = std::chrono::system_clock::now();
        }
    }

    void resetAllMetrics() {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        auto now = std::chrono::system_clock::now();
        for (auto& [operation, metric] : metrics_) {
            metric = Metrics{};
            metric.lastReset = now;
        }
    }
};

auto PerformanceMonitor::getInstance() -> PerformanceMonitor& {
    static PerformanceMonitor instance;
    if (!instance.pImpl) {
        instance.pImpl = std::make_unique<Impl>();
    }
    return instance;
}

void PerformanceMonitor::recordOperation(const std::string& operation, double executionTime, bool success) {
    if (pImpl) {
        pImpl->recordOperation(operation, executionTime, success);
    }
}

void PerformanceMonitor::recordCacheHit(const std::string& operation) {
    if (pImpl) {
        pImpl->recordCacheHit(operation);
    }
}

void PerformanceMonitor::recordCacheMiss(const std::string& operation) {
    if (pImpl) {
        pImpl->recordCacheMiss(operation);
    }
}

auto PerformanceMonitor::getMetrics(const std::string& operation) -> Metrics {
    return pImpl ? pImpl->getMetrics(operation) : Metrics{};
}

auto PerformanceMonitor::getAllMetrics() -> std::unordered_map<std::string, Metrics> {
    return pImpl ? pImpl->getAllMetrics() : std::unordered_map<std::string, Metrics>{};
}

void PerformanceMonitor::resetMetrics(const std::string& operation) {
    if (pImpl) {
        pImpl->resetMetrics(operation);
    }
}

void PerformanceMonitor::resetAllMetrics() {
    if (pImpl) {
        pImpl->resetAllMetrics();
    }
}

void PerformanceMonitor::setEnabled(bool enabled) {
    if (pImpl) {
        pImpl->enabled_ = enabled;
    }
}

auto PerformanceMonitor::isEnabled() const -> bool {
    return pImpl ? pImpl->enabled_.load() : false;
}

// PerformanceTimer implementation
PerformanceTimer::PerformanceTimer(const std::string& operation)
    : operation_(operation), startTime_(std::chrono::high_resolution_clock::now()) {
}

PerformanceTimer::~PerformanceTimer() {
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime_);
    double executionTimeMs = duration.count() / 1000.0;

    PerformanceMonitor::getInstance().recordOperation(operation_, executionTimeMs, !failed_);
}

void PerformanceTimer::markFailed() {
    failed_ = true;
}

} // namespace atom::system::locale
