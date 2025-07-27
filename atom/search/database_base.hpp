/**
 * @file database_base.hpp
 * @brief Enhanced base classes and utilities for database connectors
 * @date 2025-07-24
 */

#ifndef ATOM_SEARCH_DATABASE_BASE_HPP
#define ATOM_SEARCH_DATABASE_BASE_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

namespace atom::database {

/**
 * @brief Database performance and health statistics
 */
struct DatabaseStatistics {
    std::atomic<uint64_t> total_queries{0};
    std::atomic<uint64_t> successful_queries{0};
    std::atomic<uint64_t> failed_queries{0};
    std::atomic<uint64_t> total_query_time_ns{0};
    std::atomic<uint64_t> max_query_time_ns{0};
    std::atomic<uint64_t> connections_created{0};
    std::atomic<uint64_t> connections_destroyed{0};
    std::atomic<uint64_t> connection_pool_hits{0};
    std::atomic<uint64_t> connection_pool_misses{0};
    std::atomic<uint64_t> connection_timeouts{0};
    std::atomic<uint64_t> retry_attempts{0};
    
    double get_success_rate() const noexcept {
        uint64_t total = total_queries.load();
        return total > 0 ? static_cast<double>(successful_queries.load()) / total : 0.0;
    }
    
    double get_average_query_time_ms() const noexcept {
        uint64_t total = total_queries.load();
        return total > 0 ? 
            static_cast<double>(total_query_time_ns.load()) / (total * 1000000.0) : 0.0;
    }
    
    double get_connection_pool_hit_rate() const noexcept {
        uint64_t total = connection_pool_hits.load() + connection_pool_misses.load();
        return total > 0 ? static_cast<double>(connection_pool_hits.load()) / total : 0.0;
    }
};

/**
 * @brief Enhanced database configuration
 */
struct DatabaseConfig {
    // Connection pool settings
    size_t pool_size = 0;  // 0 = hardware_concurrency
    size_t min_pool_size = 1;
    size_t max_pool_size = 100;
    std::chrono::seconds connection_timeout{30};
    std::chrono::seconds idle_timeout{300};  // 5 minutes
    std::chrono::seconds max_connection_age{3600};  // 1 hour
    
    // Performance settings
    bool enable_statistics = true;
    bool enable_query_caching = false;
    size_t query_cache_size = 1000;
    std::chrono::seconds query_cache_ttl{300};  // 5 minutes
    bool enable_prepared_statements = true;
    size_t prepared_statement_cache_size = 100;
    
    // Reliability settings
    bool enable_health_checks = true;
    std::chrono::seconds health_check_interval{60};
    size_t max_retry_attempts = 3;
    std::chrono::milliseconds retry_delay{100};
    bool enable_connection_validation = true;
    
    // Monitoring settings
    bool enable_performance_monitoring = true;
    std::chrono::seconds monitoring_interval{30};
    bool enable_slow_query_logging = true;
    std::chrono::milliseconds slow_query_threshold{1000};
};

/**
 * @brief Connection health status
 */
enum class ConnectionHealth {
    HEALTHY,
    DEGRADED,
    UNHEALTHY,
    UNKNOWN
};

/**
 * @brief Base class for database connection wrappers
 */
template<typename ConnectionType>
class DatabaseConnection {
public:
    DatabaseConnection(ConnectionType* conn, std::chrono::steady_clock::time_point created)
        : connection_(conn), created_time_(created), last_used_(created) {}
    
    virtual ~DatabaseConnection() = default;
    
    ConnectionType* get() const { return connection_; }
    
    void update_last_used() {
        last_used_ = std::chrono::steady_clock::now();
        use_count_++;
    }
    
    bool is_expired(std::chrono::seconds max_age) const {
        auto now = std::chrono::steady_clock::now();
        return (now - created_time_) > max_age;
    }
    
    bool is_idle(std::chrono::seconds idle_timeout) const {
        auto now = std::chrono::steady_clock::now();
        return (now - last_used_) > idle_timeout;
    }
    
    virtual bool is_healthy() const = 0;
    virtual ConnectionHealth check_health() = 0;
    virtual void reset_connection() = 0;
    
    size_t get_use_count() const { return use_count_; }
    std::chrono::steady_clock::time_point get_created_time() const { return created_time_; }
    std::chrono::steady_clock::time_point get_last_used() const { return last_used_; }

protected:
    ConnectionType* connection_;
    std::chrono::steady_clock::time_point created_time_;
    std::chrono::steady_clock::time_point last_used_;
    std::atomic<size_t> use_count_{0};
};

/**
 * @brief Enhanced connection pool base class
 */
template<typename ConnectionType, typename ConnectionWrapper>
class EnhancedConnectionPool {
public:
    using ConnectionPtr = std::unique_ptr<ConnectionWrapper>;
    using AcquiredConnection = std::unique_ptr<ConnectionWrapper, 
                                              std::function<void(ConnectionWrapper*)>>;
    
    EnhancedConnectionPool(const DatabaseConfig& config) 
        : config_(config), stop_maintenance_(false) {
        initialize_pool();
        if (config_.enable_health_checks) {
            start_maintenance_thread();
        }
    }
    
    virtual ~EnhancedConnectionPool() {
        stop_maintenance_ = true;
        if (maintenance_thread_.joinable()) {
            maintenance_thread_.join();
        }
        cleanup_pool();
    }
    
    AcquiredConnection acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait for available connection or create new one
        cv_.wait(lock, [this] { 
            return !available_connections_.empty() || 
                   total_connections_ < config_.max_pool_size; 
        });
        
        ConnectionWrapper* conn = nullptr;
        
        if (!available_connections_.empty()) {
            conn = available_connections_.front().release();
            available_connections_.pop_front();
            statistics_.connection_pool_hits++;
        } else if (total_connections_ < config_.max_pool_size) {
            conn = create_connection().release();
            total_connections_++;
            statistics_.connection_pool_misses++;
        }
        
        if (conn) {
            conn->update_last_used();
            active_connections_++;
            
            return AcquiredConnection(conn, [this](ConnectionWrapper* c) {
                release(c);
            });
        }
        
        throw std::runtime_error("Failed to acquire database connection");
    }
    
    void release(ConnectionWrapper* conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (conn->is_healthy() && !conn->is_expired(config_.max_connection_age)) {
            available_connections_.emplace_back(conn);
        } else {
            delete conn;
            total_connections_--;
            statistics_.connections_destroyed++;
        }
        
        active_connections_--;
        cv_.notify_one();
    }
    
    const DatabaseStatistics& get_statistics() const {
        return statistics_;
    }
    
    std::unordered_map<std::string, size_t> get_pool_stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {
            {"total_connections", total_connections_},
            {"active_connections", active_connections_},
            {"available_connections", available_connections_.size()},
            {"max_pool_size", config_.max_pool_size},
            {"min_pool_size", config_.min_pool_size}
        };
    }
    
    bool is_healthy() const {
        auto stats = get_pool_stats();
        return stats.at("total_connections") >= config_.min_pool_size &&
               statistics_.get_success_rate() > 0.9;
    }

protected:
    virtual ConnectionPtr create_connection() = 0;
    virtual bool validate_connection(ConnectionWrapper* conn) = 0;
    
    void initialize_pool() {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t initial_size = std::max(config_.min_pool_size, 
                                      config_.pool_size > 0 ? config_.pool_size : 
                                      std::thread::hardware_concurrency());
        
        for (size_t i = 0; i < initial_size; ++i) {
            try {
                available_connections_.push_back(create_connection());
                total_connections_++;
                statistics_.connections_created++;
            } catch (const std::exception& e) {
                spdlog::error("Failed to create initial connection: {}", e.what());
            }
        }
    }
    
    void cleanup_pool() {
        std::lock_guard<std::mutex> lock(mutex_);
        available_connections_.clear();
        total_connections_ = 0;
    }
    
    void start_maintenance_thread() {
        maintenance_thread_ = std::thread([this] {
            while (!stop_maintenance_) {
                std::this_thread::sleep_for(config_.health_check_interval);
                if (!stop_maintenance_) {
                    perform_maintenance();
                }
            }
        });
    }
    
    void perform_maintenance() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Remove expired and unhealthy connections
        auto it = available_connections_.begin();
        while (it != available_connections_.end()) {
            if ((*it)->is_expired(config_.max_connection_age) || 
                !(*it)->is_healthy()) {
                it = available_connections_.erase(it);
                total_connections_--;
                statistics_.connections_destroyed++;
            } else {
                ++it;
            }
        }
        
        // Ensure minimum pool size
        while (total_connections_ < config_.min_pool_size) {
            try {
                available_connections_.push_back(create_connection());
                total_connections_++;
                statistics_.connections_created++;
            } catch (const std::exception& e) {
                spdlog::error("Failed to create maintenance connection: {}", e.what());
                break;
            }
        }
    }

protected:
    DatabaseConfig config_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<ConnectionPtr> available_connections_;
    size_t total_connections_ = 0;
    size_t active_connections_ = 0;
    mutable DatabaseStatistics statistics_;
    
    std::thread maintenance_thread_;
    std::atomic<bool> stop_maintenance_;
};

/**
 * @brief Query result cache for improved performance
 */
template<typename ResultType>
class QueryCache {
public:
    QueryCache(size_t max_size, std::chrono::seconds ttl)
        : max_size_(max_size), ttl_(ttl) {}
    
    std::optional<ResultType> get(const std::string& query) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(query);
        if (it != cache_.end()) {
            auto now = std::chrono::steady_clock::now();
            if ((now - it->second.timestamp) <= ttl_) {
                it->second.access_count++;
                it->second.last_access = now;
                return it->second.result;
            } else {
                cache_.erase(it);
            }
        }
        
        return std::nullopt;
    }
    
    void put(const std::string& query, const ResultType& result) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (cache_.size() >= max_size_) {
            evict_lru();
        }
        
        auto now = std::chrono::steady_clock::now();
        cache_[query] = {result, now, now, 1};
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

private:
    struct CacheEntry {
        ResultType result;
        std::chrono::steady_clock::time_point timestamp;
        std::chrono::steady_clock::time_point last_access;
        size_t access_count;
    };
    
    void evict_lru() {
        if (cache_.empty()) return;
        
        auto oldest = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (it->second.last_access < oldest->second.last_access) {
                oldest = it;
            }
        }
        cache_.erase(oldest);
    }
    
    size_t max_size_;
    std::chrono::seconds ttl_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, CacheEntry> cache_;
};

/**
 * @brief Retry mechanism for database operations
 */
class RetryManager {
public:
    RetryManager(size_t max_attempts, std::chrono::milliseconds delay)
        : max_attempts_(max_attempts), base_delay_(delay) {}
    
    template<typename Func>
    auto execute_with_retry(Func&& func) -> decltype(func()) {
        size_t attempt = 0;
        std::exception_ptr last_exception;
        
        while (attempt < max_attempts_) {
            try {
                return func();
            } catch (...) {
                last_exception = std::current_exception();
                attempt++;
                
                if (attempt < max_attempts_) {
                    auto delay = base_delay_ * (1 << (attempt - 1));  // Exponential backoff
                    std::this_thread::sleep_for(delay);
                }
            }
        }
        
        std::rethrow_exception(last_exception);
    }

private:
    size_t max_attempts_;
    std::chrono::milliseconds base_delay_;
};

}  // namespace atom::database

#endif  // ATOM_SEARCH_DATABASE_BASE_HPP
