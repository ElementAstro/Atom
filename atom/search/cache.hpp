/**
 * @file cache.hpp
 * @brief A high-performance, thread-safe, sharded resource cache for Atom Search.
 * @date 2025-07-16
 */

#ifndef ATOM_SEARCH_CACHE_HPP
#define ATOM_SEARCH_CACHE_HPP

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <concepts>
#include <fstream>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <shared_mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "atom/containers/high_performance.hpp"
#include "atom/type/json.hpp"

namespace atom::search {

using json = nlohmann::json;
using atom::containers::HashMap;
using atom::containers::String;
using atom::containers::Vector;

/**
 * @brief Cache eviction policies.
 */
enum class EvictionPolicy {
    LRU,    ///< Least Recently Used
    LFU,    ///< Least Frequently Used
    FIFO,   ///< First In, First Out
    RANDOM  ///< Random eviction
};

/**
 * @brief Cache configuration options.
 */
struct CacheConfig {
    size_t max_size = 1000;                           ///< Maximum cache size
    std::chrono::seconds cleanup_interval{5};         ///< Cleanup interval
    EvictionPolicy eviction_policy = EvictionPolicy::LRU; ///< Eviction policy
    bool enable_compression = false;                  ///< Enable data compression
    size_t max_memory_mb = 0;                        ///< Max memory usage (0 = unlimited)
    bool enable_persistence = false;                 ///< Enable automatic persistence
    std::string persistence_file;                    ///< Persistence file path
    std::chrono::seconds persistence_interval{60};   ///< Persistence interval

    // Enhanced configuration options
    bool enable_metrics = true;                      ///< Enable detailed metrics collection
    bool enable_performance_tracking = false;        ///< Enable performance timing
    double memory_pressure_threshold = 0.8;          ///< Memory pressure threshold (0.0-1.0)
    size_t batch_size_hint = 100;                   ///< Hint for batch operations
    bool enable_adaptive_sizing = false;             ///< Enable adaptive shard sizing
    size_t compression_threshold = 1024;             ///< Compress values larger than this
    size_t num_shards = 16;                         ///< Number of shards (power of 2)

    // Monitoring and alerting
    bool enable_health_monitoring = true;            ///< Enable health monitoring
    std::chrono::seconds health_check_interval{300}; ///< Health check interval
    double unhealthy_hit_ratio_threshold = 0.1;      ///< Alert if hit ratio drops below
    size_t max_consecutive_evictions = 1000;         ///< Alert if too many evictions

    // Performance tuning
    bool use_optimized_eviction = true;              ///< Use optimized eviction algorithms
    bool enable_prefetching = false;                 ///< Enable predictive prefetching
    size_t prefetch_window_size = 10;                ///< Number of items to prefetch
    bool enable_deferred_cleanup = true;             ///< Enable deferred cleanup for performance
};

/**
 * @brief Cache performance metrics.
 */
struct CacheMetrics {
    std::atomic<uint64_t> hit_count{0};
    std::atomic<uint64_t> miss_count{0};
    std::atomic<uint64_t> eviction_count{0};
    std::atomic<uint64_t> expiration_count{0};
    std::atomic<uint64_t> total_operations{0};
    std::atomic<size_t> memory_usage_bytes{0};

    // Custom copy constructor
    CacheMetrics(const CacheMetrics& other)
        : hit_count(other.hit_count.load()),
          miss_count(other.miss_count.load()),
          eviction_count(other.eviction_count.load()),
          expiration_count(other.expiration_count.load()),
          total_operations(other.total_operations.load()),
          memory_usage_bytes(other.memory_usage_bytes.load()) {}

    // Custom assignment operator
    CacheMetrics& operator=(const CacheMetrics& other) {
        if (this != &other) {
            hit_count.store(other.hit_count.load());
            miss_count.store(other.miss_count.load());
            eviction_count.store(other.eviction_count.load());
            expiration_count.store(other.expiration_count.load());
            total_operations.store(other.total_operations.load());
            memory_usage_bytes.store(other.memory_usage_bytes.load());
        }
        return *this;
    }

    // Default constructor
    CacheMetrics() = default;

    // Enhanced metrics
    std::atomic<uint64_t> insert_count{0};
    std::atomic<uint64_t> remove_count{0};
    std::atomic<uint64_t> update_count{0};

    // Performance metrics
    std::atomic<uint64_t> total_access_time_ns{0};
    std::atomic<uint64_t> total_insert_time_ns{0};
    std::atomic<uint64_t> max_access_time_ns{0};
    std::atomic<uint64_t> max_insert_time_ns{0};

    // Memory pressure metrics
    std::atomic<uint64_t> memory_pressure_evictions{0};
    std::atomic<uint64_t> size_pressure_evictions{0};

    double get_hit_ratio() const noexcept {
        uint64_t total = hit_count.load() + miss_count.load();
        return total > 0 ? static_cast<double>(hit_count.load()) / total : 0.0;
    }

    double get_memory_usage_mb() const noexcept {
        return static_cast<double>(memory_usage_bytes.load()) / (1024.0 * 1024.0);
    }

    double get_average_access_time_ns() const noexcept {
        uint64_t total_accesses = hit_count.load() + miss_count.load();
        return total_accesses > 0 ?
            static_cast<double>(total_access_time_ns.load()) / total_accesses : 0.0;
    }

    double get_average_insert_time_ns() const noexcept {
        uint64_t inserts = insert_count.load();
        return inserts > 0 ?
            static_cast<double>(total_insert_time_ns.load()) / inserts : 0.0;
    }

    void reset() noexcept {
        hit_count = 0;
        miss_count = 0;
        eviction_count = 0;
        expiration_count = 0;
        total_operations = 0;
        insert_count = 0;
        remove_count = 0;
        update_count = 0;
        total_access_time_ns = 0;
        total_insert_time_ns = 0;
        max_access_time_ns = 0;
        max_insert_time_ns = 0;
        memory_pressure_evictions = 0;
        size_pressure_evictions = 0;
    }
};

/**
 * @brief Concept for types that can be stored in the ResourceCache.
 * @details Ensures that the type is both copy-constructible and copy-assignable.
 */
template <typename T>
concept Cacheable = std::copy_constructible<T> && std::is_copy_assignable_v<T>;

/**
 * @brief A high-performance, thread-safe, sharded cache for storing and managing
 * resources with expiration times.
 *
 * This class provides a highly concurrent caching mechanism with an LRU eviction
 * policy. It achieves scalability by partitioning the cache into multiple shards,
 * each with its own lock, minimizing contention on multi-core systems. It
 * features automatic expiration cleanup and supports both synchronous and
 * asynchronous operations.
 *
 * @tparam T The type of the resources to be cached. Must satisfy the Cacheable
 * concept.
 */
template <Cacheable T>
class ResourceCache {
public:
    using Callback = std::function<void(const String& key)>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::seconds;

    /**
     * @brief Constructs a ResourceCache.
     *
     * @param max_size The maximum number of items the cache can hold across all
     * shards.
     * @param cleanup_interval The interval at which the cleanup thread checks
     * for expired items.
     */
    explicit ResourceCache(size_t max_size,
                           Duration cleanup_interval = Duration(5));

    /**
     * @brief Constructs a ResourceCache with configuration.
     *
     * @param config Cache configuration options.
     */
    explicit ResourceCache(const CacheConfig& config);

    /**
     * @brief Destructs the ResourceCache, stopping the background cleanup
     * thread.
     */
    ~ResourceCache();

    ResourceCache(const ResourceCache&) = delete;
    ResourceCache& operator=(const ResourceCache&) = delete;
    ResourceCache(ResourceCache&&) = delete;
    ResourceCache& operator=(ResourceCache&&) = delete;

    /**
     * @brief Inserts a resource into the cache with an expiration time.
     *
     * @param key The key associated with the resource.
     * @param value The resource to be cached.
     * @param expiration_time The duration after which the resource expires.
     */
    void insert(const String& key, const T& value, Duration expiration_time);

    /**
     * @brief Checks if the cache contains a resource with the specified key.
     *
     * @param key The key to check.
     * @return True if the cache contains a non-expired resource, false
     * otherwise.
     */
    [[nodiscard]] auto contains(const String& key) const -> bool;

    /**
     * @brief Retrieves a resource from the cache.
     *
     * @param key The key associated with the resource.
     * @return An optional containing the resource if found and not expired,
     * otherwise std::nullopt.
     */
    [[nodiscard]] auto get(const String& key) -> std::optional<T>;

    /**
     * @brief Removes a resource from the cache.
     *
     * @param key The key associated with the resource to be removed.
     */
    void remove(const String& key);

    /**
     * @brief Asynchronously retrieves a resource from the cache.
     *
     * @param key The key associated with the resource.
     * @return A future containing an optional with the resource if found,
     * otherwise std::nullopt.
     */
    [[nodiscard]] auto async_get(const String& key) -> std::future<std::optional<T>>;

    /**
     * @brief Asynchronously inserts a resource into the cache.
     *
     * @param key The key associated with the resource.
     * @param value The resource to be cached.
     * @param expiration_time The time after which the resource expires.
     * @return A future that completes when the insertion is done.
     */
    auto async_insert(const String& key, const T& value, Duration expiration_time)
        -> std::future<void>;

    /**
     * @brief Clears all resources from the cache.
     */
    void clear();

    /**
     * @brief Gets the approximate number of resources in the cache.
     *
     * @return The number of resources currently in the cache.
     */
    [[nodiscard]] auto size() const -> size_t;

    /**
     * @brief Checks if the cache is empty.
     *
     * @return True if the cache is empty, false otherwise.
     */
    [[nodiscard]] auto empty() const -> bool;

    /**
     * @brief Sets the maximum size of the cache.
     * @details This will re-distribute the capacity among shards and may cause
     * evictions.
     * @param new_max_size The new maximum size of the cache.
     */
    void set_max_size(size_t new_max_size);

    /**
     * @brief Reads resources from a JSON file and inserts them into the cache.
     *
     * @param file_path The path to the JSON file.
     * @param from_json A function to deserialize a resource from a JSON object.
     * @param expiration_time The expiration time to apply to all loaded items.
     */
    void read_from_json_file(const String& file_path,
                             const std::function<T(const json&)>& from_json,
                             Duration expiration_time);

    /**
     * @brief Writes the resources in the cache to a JSON file.
     *
     * @param file_path The path to the JSON file.
     * @param to_json A function to serialize a resource to a JSON object.
     */
    void write_to_json_file(const String& file_path,
                            const std::function<json(const T&)>& to_json) const;

    /**
     * @brief Inserts multiple resources into the cache.
     *
     * @param items The vector of key-value pairs to insert.
     * @param expiration_time The time after which the resources expire.
     */
    void insert_batch(const Vector<std::pair<String, T>>& items,
                      Duration expiration_time);

    /**
     * @brief Removes multiple resources from the cache.
     *
     * @param keys The vector of keys associated with the resources to remove.
     */
    void remove_batch(const Vector<String>& keys);

    /**
     * @brief Registers a callback to be called on insertion.
     *
     * @param callback The callback function.
     */
    void on_insert(Callback callback);

    /**
     * @brief Registers a callback to be called on removal.
     *
     * @param callback The callback function.
     */
    void on_remove(Callback callback);

    /**
     * @brief Checks if a cache entry is expired.
     *
     * @param key The key to check for expiration.
     * @return True if the entry is expired or doesn't exist, false otherwise.
     */
    [[nodiscard]] bool is_expired(const String& key) const;

    /**
     * @brief Asynchronously loads a value using a loader function.
     *
     * @param key The key to associate with the loaded value.
     * @param loader Function that returns the value to cache.
     * @param expiration_time Optional expiration time for the loaded value.
     * @return A future that completes when the value is loaded and cached.
     */
    template<typename Loader>
    auto async_load(const String& key, Loader&& loader,
                    Duration expiration_time = Duration(3600)) -> std::future<void>;

    /**
     * @brief Sets the expiration time for an existing cache entry.
     *
     * @param key The key of the entry to update.
     * @param expiration_time The new expiration time.
     */
    void set_expiration_time(const String& key, Duration expiration_time);

    /**
     * @brief Writes cache contents to a file using custom serialization.
     *
     * @param file_path The path to write to.
     * @param serializer Function to serialize values to strings.
     */
    void write_to_file(const String& file_path,
                       const std::function<String(const T&)>& serializer) const;

    /**
     * @brief Reads cache contents from a file using custom deserialization.
     *
     * @param file_path The path to read from.
     * @param deserializer Function to deserialize strings to values.
     * @param expiration_time Expiration time for loaded entries.
     */
    void read_from_file(const String& file_path,
                        const std::function<T(const String&)>& deserializer,
                        Duration expiration_time);

    /**
     * @brief Retrieves cache performance statistics.
     *
     * @return A pair containing hit count and miss count.
     */
    [[nodiscard]] auto get_statistics() const -> std::pair<size_t, size_t>;

    /**
     * @brief Removes expired entries from the cache.
     */
    void removeExpired();

    // Compatibility aliases for camelCase method names
    [[nodiscard]] auto getStatistics() const -> std::pair<size_t, size_t> { return get_statistics(); }
    void setMaxSize(size_t max_size) { set_max_size(max_size); }
    void setExpirationTime(const String& key, Duration expiration_time) { set_expiration_time(key, expiration_time); }
    void onInsert(const std::function<void(const String&)>& callback) { on_insert(callback); }
    void onRemove(const std::function<void(const String&)>& callback) { on_remove(callback); }
    void insertBatch(const std::vector<std::pair<String, T>>& items, Duration expiration_time) { insert_batch(items, expiration_time); }
    void removeBatch(const std::vector<String>& keys) { remove_batch(keys); }
    void writeToFile(const String& file_path, const std::function<String(const T&)>& serializer) const { write_to_file(file_path, serializer); }
    void readFromFile(const String& file_path, const std::function<T(const String&)>& deserializer, Duration expiration_time = Duration(3600)) { read_from_file(file_path, deserializer, expiration_time); }
    void writeToJsonFile(const String& file_path, const std::function<json(const T&)>& serializer) const { write_to_json_file(file_path, serializer); }
    void readFromJsonFile(const String& file_path, const std::function<T(const json&)>& deserializer, Duration expiration_time = Duration(3600)) { read_from_json_file(file_path, deserializer, expiration_time); }
    void evictOldest() { /* Implementation needed */ }
    auto asyncInsert(const String& key, T value, Duration expiration_time = Duration(3600)) { return async_insert(key, std::move(value), expiration_time); }
    auto asyncGet(const String& key) { return async_get(key); }
    template<typename Loader>
    auto asyncLoad(const String& key, Loader&& loader, Duration expiration_time = Duration(3600)) { return async_load(key, std::forward<Loader>(loader), expiration_time); }
    bool isExpired(const String& key) const { return is_expired(key); }

    /**
     * @brief Retrieves comprehensive cache metrics.
     *
     * @return Cache metrics structure with detailed statistics.
     */
    [[nodiscard]] const CacheMetrics& get_metrics() const noexcept;

    /**
     * @brief Resets all cache metrics.
     */
    void reset_metrics() noexcept;

    /**
     * @brief Gets current cache configuration.
     *
     * @return Current cache configuration.
     */
    [[nodiscard]] const CacheConfig& get_config() const noexcept;

    /**
     * @brief Updates cache configuration.
     *
     * @param config New configuration.
     */
    void update_config(const CacheConfig& config);

    /**
     * @brief Preloads cache with data from a source.
     *
     * @param loader Function that provides key-value pairs to preload.
     * @param expiration_time Expiration time for preloaded items.
     */
    void warm_cache(const std::function<Vector<std::pair<String, T>>()>& loader,
                    Duration expiration_time);

    /**
     * @brief Gets cache health information.
     *
     * @return Map of health metrics.
     */
    [[nodiscard]] HashMap<String, double> get_health_metrics() const;

    /**
     * @brief Optimizes cache performance by reorganizing data.
     */
    void optimize();

    /**
     * @brief Gets memory usage statistics.
     *
     * @return Memory usage in bytes.
     */
    [[nodiscard]] size_t get_memory_usage() const noexcept;

    /**
     * @brief Checks if cache is healthy (within memory limits, etc.).
     *
     * @return True if cache is healthy, false otherwise.
     */
    [[nodiscard]] bool is_healthy() const noexcept;

    /**
     * @brief Manually triggers persistence if enabled.
     */
    void persist_now();

    /**
     * @brief Gets detailed shard statistics.
     *
     * @return Vector of per-shard statistics.
     */
    [[nodiscard]] Vector<HashMap<String, size_t>> get_shard_stats() const;

    /**
     * @brief Gets comprehensive cache health report.
     *
     * @return Health report with recommendations.
     */
    [[nodiscard]] HashMap<String, String> get_health_report() const;

    /**
     * @brief Prefetches related items based on access patterns.
     *
     * @param key The key to base prefetching on.
     * @param count Number of items to prefetch.
     */
    void prefetch_related(const String& key, size_t count = 5);

    /**
     * @brief Gets cache efficiency metrics.
     *
     * @return Efficiency metrics and recommendations.
     */
    [[nodiscard]] HashMap<String, double> get_efficiency_metrics() const;

    /**
     * @brief Enables or disables performance tracking.
     *
     * @param enabled Whether to enable performance tracking.
     */
    void set_performance_tracking(bool enabled);

private:
    struct CacheEntry {
        T value;
        TimePoint creation_time;
        TimePoint expiration_time;
        std::atomic<uint64_t> access_count{0};  ///< For LFU policy
        TimePoint last_access_time;             ///< For LRU optimization
        size_t estimated_size{0};               ///< For memory tracking

        CacheEntry() = default;
        CacheEntry(const T& v, TimePoint ct, Duration et, size_t size = 0)
            : value(v), creation_time(ct), expiration_time(ct + et),
              last_access_time(ct), estimated_size(size) {}

        // Copy constructor
        CacheEntry(const CacheEntry& other)
            : value(other.value), creation_time(other.creation_time),
              expiration_time(other.expiration_time),
              access_count(other.access_count.load()),
              last_access_time(other.last_access_time),
              estimated_size(other.estimated_size) {}

        // Move constructor
        CacheEntry(CacheEntry&& other) noexcept
            : value(std::move(other.value)), creation_time(other.creation_time),
              expiration_time(other.expiration_time),
              access_count(other.access_count.load()),
              last_access_time(other.last_access_time),
              estimated_size(other.estimated_size) {}

        // Copy assignment operator
        CacheEntry& operator=(const CacheEntry& other) {
            if (this != &other) {
                value = other.value;
                creation_time = other.creation_time;
                expiration_time = other.expiration_time;
                access_count.store(other.access_count.load());
                last_access_time = other.last_access_time;
                estimated_size = other.estimated_size;
            }
            return *this;
        }

        // Move assignment operator
        CacheEntry& operator=(CacheEntry&& other) noexcept {
            if (this != &other) {
                value = std::move(other.value);
                creation_time = other.creation_time;
                expiration_time = other.expiration_time;
                access_count.store(other.access_count.load());
                last_access_time = other.last_access_time;
                estimated_size = other.estimated_size;
            }
            return *this;
        }
    };

    struct Shard {
        HashMap<String, typename std::list<String>::iterator> map;
        std::list<String> lru_list;
        HashMap<String, CacheEntry> entries;
        mutable std::shared_mutex mutex;
        size_t max_size;
        std::atomic<size_t> memory_usage{0};
        std::atomic<size_t> entry_count{0};  ///< Fast entry count per shard

        // For different eviction policies
        HashMap<String, uint64_t> access_frequency;  ///< For LFU
        std::queue<String> fifo_queue;               ///< For FIFO

        // Performance optimizations
        mutable std::priority_queue<std::pair<uint64_t, String>,
                                   std::vector<std::pair<uint64_t, String>>,
                                   std::greater<>> lfu_heap;  ///< Min-heap for LFU
        mutable std::mt19937 rng;  ///< Thread-safe random generator

        // Memory management
        std::atomic<bool> needs_cleanup{false};  ///< Flag for deferred cleanup

        explicit Shard(size_t capacity) : max_size(capacity), rng(std::random_device{}()) {}
    };

    void evict(Shard& shard);
    void evict_lru(Shard& shard);
    void evict_lfu(Shard& shard);
    void evict_lfu_optimized(Shard& shard);  ///< Optimized LFU with heap
    void evict_fifo(Shard& shard);
    void evict_random(Shard& shard);
    void cleanup_expired_entries();
    void cleanup_shard_deferred(Shard& shard);  ///< Deferred cleanup for performance
    void persistence_worker();
    auto get_shard(const String& key) const -> Shard&;
    size_t estimate_size(const T& value) const;
    void update_memory_usage(Shard& shard, const String& key, bool adding);
    void enforce_memory_limits();  ///< Enforce memory constraints
    void optimize_shard(Shard& shard);  ///< Per-shard optimization

    std::vector<std::unique_ptr<Shard>> shards_;
    const size_t shard_mask_;
    std::atomic<size_t> max_size_;
    std::atomic<size_t> current_size_{0};

    std::jthread cleanup_thread_;
    std::jthread persistence_thread_;
    std::atomic<bool> stop_cleanup_{false};
    std::atomic<bool> stop_persistence_{false};
    Duration cleanup_interval_;

    Callback insert_callback_;
    Callback remove_callback_;
    mutable std::mutex callback_mutex_;

    // Enhanced configuration and metrics
    CacheConfig config_;
    mutable CacheMetrics metrics_;

    // Persistence support
    mutable std::mutex persistence_mutex_;
    std::atomic<bool> persistence_needed_{false};
};

template <Cacheable T>
ResourceCache<T>::ResourceCache(size_t max_size, Duration cleanup_interval)
    : shard_mask_([&] {
        size_t shard_count = std::thread::hardware_concurrency();
        if (shard_count == 0) shard_count = 4;
        size_t power = 1;
        while (power < shard_count) power <<= 1;
        return power - 1;
    }()),
      max_size_(max_size),
      cleanup_interval_(cleanup_interval),
      config_({.max_size = max_size, .cleanup_interval = cleanup_interval}) {
    size_t shard_count = shard_mask_ + 1;
    shards_.reserve(shard_count);
    // Ensure total capacity across all shards doesn't exceed max_size
    size_t per_shard_capacity = max_size / shard_count;
    if (per_shard_capacity == 0) per_shard_capacity = 1; // Each shard needs at least 1 capacity
    for (size_t i = 0; i < shard_count; ++i) {
        shards_.emplace_back(std::make_unique<Shard>(per_shard_capacity));
    }
    cleanup_thread_ = std::jthread([this] { cleanup_expired_entries(); });
}

template <Cacheable T>
ResourceCache<T>::~ResourceCache() {
    stop_cleanup_.store(true);
}

template <Cacheable T>
auto ResourceCache<T>::get_shard(const String& key) const -> Shard& {
    return *shards_[std::hash<String>{}(key) & shard_mask_];
}

template <Cacheable T>
void ResourceCache<T>::insert(const String& key, const T& value,
                              Duration expiration_time) {
    try {
        auto& shard = get_shard(key);
        std::unique_lock lock(shard.mutex);

        // Check if key already exists
        auto it = shard.map.find(key);
        if (it != shard.map.end()) {
            update_memory_usage(shard, key, false);  // Remove old entry
            shard.lru_list.erase(it->second);
            shard.map.erase(it);
            shard.entries.erase(key);
            shard.entry_count--;
            current_size_--;
        }

        // Evict if necessary - check global size first, then per-shard
        if (current_size_.load() >= max_size_) {
            // Global cache is full, need to evict
            if (!shard.entries.empty()) {
                evict(shard);
            } else {
                // Current shard is empty, try to evict from other shards
                for (auto& other_shard_ptr : shards_) {
                    if (other_shard_ptr.get() != &shard) {
                        std::unique_lock<std::shared_mutex> other_lock(other_shard_ptr->mutex, std::try_to_lock);
                        if (other_lock.owns_lock() && !other_shard_ptr->entries.empty()) {
                            evict(*other_shard_ptr);
                            break;
                        }
                    }
                }
            }
        }

        // Create new entry with size estimation
        size_t estimated_size = estimate_size(value);
        auto now = Clock::now();
        CacheEntry entry{value, now, expiration_time, estimated_size};

        // Insert new entry
        shard.lru_list.push_front(key);
        shard.map[key] = shard.lru_list.begin();
        shard.entries[key] = std::move(entry);
        shard.entry_count++;
        current_size_++;

        // Update memory tracking
        update_memory_usage(shard, key, true);

        // Add to FIFO queue if using FIFO policy
        if (config_.eviction_policy == EvictionPolicy::FIFO) {
            shard.fifo_queue.push(key);
        }

        // Check memory limits
        if (config_.max_memory_mb > 0) {
            lock.unlock();  // Release shard lock before global operation
            enforce_memory_limits();
        }

        // Trigger callback outside of lock
        if (insert_callback_) {
            lock.unlock();
            std::lock_guard cb_lock(callback_mutex_);
            if (insert_callback_) insert_callback_(key);
        }
    } catch (const std::exception& e) {
        spdlog::error("Insert failed for key {}: {}", key.c_str(), e.what());
    }
}

template <Cacheable T>
auto ResourceCache<T>::contains(const String& key) const -> bool {
    try {
        auto& shard = get_shard(key);
        std::shared_lock lock(shard.mutex);
        auto it = shard.entries.find(key);
        if (it == shard.entries.end()) {
            return false;
        }
        return Clock::now() < it->second.expiration_time;
    } catch (const std::exception& e) {
        spdlog::error("Contains check failed for key {}: {}", key.c_str(),
                      e.what());
        return false;
    }
}

template <Cacheable T>
auto ResourceCache<T>::get(const String& key) -> std::optional<T> {
    try {
        auto& shard = get_shard(key);
        std::unique_lock lock(shard.mutex);

        auto map_it = shard.map.find(key);
        if (map_it == shard.map.end()) {
            metrics_.miss_count++;
            metrics_.total_operations++;
            return std::nullopt;
        }

        auto& entry = shard.entries.at(key);
        if (Clock::now() >= entry.expiration_time) {
            metrics_.miss_count++;
            metrics_.expiration_count++;
            metrics_.total_operations++;
            // Entry is expired, remove it
            shard.lru_list.erase(map_it->second);
            shard.map.erase(map_it);
            shard.entries.erase(key);
            current_size_--;
            if (remove_callback_) {
                std::lock_guard cb_lock(callback_mutex_);
                if (remove_callback_) remove_callback_(key);
            }
            return std::nullopt;
        }

        // Update access information
        entry.access_count++;
        entry.last_access_time = Clock::now();

        // Move to front of LRU list
        shard.lru_list.splice(shard.lru_list.begin(), shard.lru_list,
                              map_it->second);

        metrics_.hit_count++;
        metrics_.total_operations++;
        return entry.value;
    } catch (const std::exception& e) {
        spdlog::error("Get failed for key {}: {}", key.c_str(), e.what());
        metrics_.miss_count++;
        metrics_.total_operations++;
        return std::nullopt;
    }
}

template <Cacheable T>
void ResourceCache<T>::remove(const String& key) {
    try {
        auto& shard = get_shard(key);
        std::unique_lock lock(shard.mutex);
        auto it = shard.map.find(key);
        if (it != shard.map.end()) {
            shard.lru_list.erase(it->second);
            shard.map.erase(it);
            shard.entries.erase(key);
            current_size_--;
            if (remove_callback_) {
                std::lock_guard cb_lock(callback_mutex_);
                if (remove_callback_) remove_callback_(key);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Remove failed for key {}: {}", key.c_str(), e.what());
    }
}

template <Cacheable T>
auto ResourceCache<T>::async_get(const String& key)
    -> std::future<std::optional<T>> {
    return std::async(std::launch::async, [this, key]() { return get(key); });
}

template <Cacheable T>
auto ResourceCache<T>::async_insert(const String& key, const T& value,
                                    Duration expiration_time) -> std::future<void> {
    return std::async(std::launch::async, [this, key, value, expiration_time]() {
        insert(key, value, expiration_time);
    });
}

template <Cacheable T>
void ResourceCache<T>::clear() {
    for (auto& shard_ptr : shards_) {
        std::unique_lock lock(shard_ptr->mutex);
        shard_ptr->map.clear();
        shard_ptr->lru_list.clear();
        shard_ptr->entries.clear();
    }
    current_size_ = 0;
}

template <Cacheable T>
auto ResourceCache<T>::size() const -> size_t {
    return current_size_.load();
}

template <Cacheable T>
auto ResourceCache<T>::empty() const -> bool {
    return size() == 0;
}



template <Cacheable T>
void ResourceCache<T>::cleanup_expired_entries() {
    while (!stop_cleanup_.load()) {
        // Sleep in smaller intervals to be more responsive to stop signal
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(cleanup_interval_);
        while (remaining > std::chrono::milliseconds(100) && !stop_cleanup_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            remaining -= std::chrono::milliseconds(100);
        }
        if (!stop_cleanup_.load() && remaining > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(remaining);
        }
        if (stop_cleanup_.load()) break;

        // Mark shards that need cleanup instead of blocking
        for (auto& shard_ptr : shards_) {
            shard_ptr->needs_cleanup = true;
        }

        // Perform deferred cleanup on each shard
        for (auto& shard_ptr : shards_) {
            cleanup_shard_deferred(*shard_ptr);
        }

        // Perform global optimizations periodically
        static int cleanup_cycles = 0;
        if (++cleanup_cycles % 10 == 0) {  // Every 10 cycles
            enforce_memory_limits();

            // Optimize shards
            for (auto& shard_ptr : shards_) {
                optimize_shard(*shard_ptr);
            }

            spdlog::info("Periodic cache optimization completed");
        }
    }
}

template <Cacheable T>
void ResourceCache<T>::set_max_size(size_t new_max_size) {
    max_size_ = new_max_size;

    // Simple approach: just evict items until we're under the global limit
    // Don't worry about per-shard limits for now
    while (current_size_.load() > new_max_size) {
        bool evicted_any = false;

        // Try to evict from each shard until we're under the limit
        for (auto& shard_ptr : shards_) {
            if (current_size_.load() <= new_max_size) {
                break; // We've reached the target size
            }

            std::unique_lock lock(shard_ptr->mutex);
            if (!shard_ptr->entries.empty()) {
                evict(*shard_ptr);
                evicted_any = true;
            }
        }

        if (!evicted_any) {
            break; // No more items to evict, avoid infinite loop
        }
    }

    // Update per-shard limits after global eviction
    size_t per_shard_capacity = new_max_size / shards_.size();
    if (per_shard_capacity == 0) per_shard_capacity = 1; // Each shard needs at least 1 capacity
    for (auto& shard_ptr : shards_) {
        std::unique_lock lock(shard_ptr->mutex);
        shard_ptr->max_size = per_shard_capacity;
    }
}

template <Cacheable T>
void ResourceCache<T>::read_from_json_file(
    const String& file_path, const std::function<T(const json&)>& from_json,
    Duration expiration_time) {
    std::ifstream input_file(file_path.c_str());
    if (!input_file.is_open()) {
        spdlog::error("Failed to open JSON file for reading: {}",
                      file_path.c_str());
        return;
    }

    try {
        json data;
        input_file >> data;
        if (data.is_object()) {
            for (auto it = data.begin(); it != data.end(); ++it) {
                insert(String(it.key()), from_json(it.value()), expiration_time);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error processing JSON file {}: {}", file_path.c_str(),
                      e.what());
    }
}

template <Cacheable T>
void ResourceCache<T>::write_to_json_file(
    const String& file_path,
    const std::function<json(const T&)>& to_json) const {
    json data = json::object();
    for (const auto& shard_ptr : shards_) {
        std::shared_lock lock(shard_ptr->mutex);
        for (const auto& pair : shard_ptr->entries) {
            data[std::string(pair.first.c_str())] = to_json(pair.second.value);
        }
    }

    std::ofstream output_file(file_path.c_str());
    if (!output_file.is_open()) {
        spdlog::error("Failed to open JSON file for writing: {}",
                      file_path.c_str());
        return;
    }
    output_file << data.dump(4);
}

template <Cacheable T>
void ResourceCache<T>::insert_batch(const Vector<std::pair<String, T>>& items,
                                    Duration expiration_time) {
    if (items.empty()) return;

    // Group items by shard for better performance
    HashMap<size_t, Vector<std::pair<String, T>>> shard_groups;
    for (const auto& [key, value] : items) {
        size_t shard_index = std::hash<String>{}(key) & shard_mask_;
        shard_groups[shard_index].emplace_back(key, value);
    }

    // Process each shard group
    for (const auto& [shard_index, shard_items] : shard_groups) {
        auto& shard = *shards_[shard_index];
        std::unique_lock lock(shard.mutex);

        for (const auto& [key, value] : shard_items) {
            // Check if key already exists
            auto it = shard.map.find(key);
            if (it != shard.map.end()) {
                update_memory_usage(shard, key, false);
                shard.lru_list.erase(it->second);
                shard.map.erase(it);
                shard.entries.erase(key);
                shard.entry_count--;
                current_size_--;
            }

            // Evict if necessary
            if (shard.entry_count.load() >= shard.max_size) {
                evict(shard);
            }

            // Create and insert new entry
            size_t estimated_size = estimate_size(value);
            auto now = Clock::now();
            CacheEntry entry{value, now, expiration_time, estimated_size};

            shard.lru_list.push_front(key);
            shard.map[key] = shard.lru_list.begin();
            shard.entries[key] = std::move(entry);
            shard.entry_count++;
            current_size_++;

            update_memory_usage(shard, key, true);

            if (config_.eviction_policy == EvictionPolicy::FIFO) {
                shard.fifo_queue.push(key);
            }
        }
    }

    // Check memory limits after batch
    if (config_.max_memory_mb > 0) {
        enforce_memory_limits();
    }

    spdlog::info("Batch inserted {} items", items.size());
}

template <Cacheable T>
void ResourceCache<T>::remove_batch(const Vector<String>& keys) {
    if (keys.empty()) return;

    // Group keys by shard for better performance
    HashMap<size_t, Vector<String>> shard_groups;
    for (const auto& key : keys) {
        size_t shard_index = std::hash<String>{}(key) & shard_mask_;
        shard_groups[shard_index].push_back(key);
    }

    size_t removed_count = 0;

    // Process each shard group
    for (const auto& [shard_index, shard_keys] : shard_groups) {
        auto& shard = *shards_[shard_index];
        std::unique_lock lock(shard.mutex);

        for (const auto& key : shard_keys) {
            auto it = shard.map.find(key);
            if (it != shard.map.end()) {
                update_memory_usage(shard, key, false);
                shard.lru_list.erase(it->second);
                shard.map.erase(it);
                shard.entries.erase(key);
                shard.entry_count--;
                current_size_--;
                removed_count++;
            }
        }
    }

    spdlog::info("Batch removed {} items", removed_count);
}

template <Cacheable T>
void ResourceCache<T>::on_insert(Callback callback) {
    std::lock_guard lock(callback_mutex_);
    insert_callback_ = std::move(callback);
}

template <Cacheable T>
void ResourceCache<T>::on_remove(Callback callback) {
    std::lock_guard lock(callback_mutex_);
    remove_callback_ = std::move(callback);
}

template <Cacheable T>
auto ResourceCache<T>::get_statistics() const -> std::pair<size_t, size_t> {
    return {metrics_.hit_count.load(), metrics_.miss_count.load()};
}

// Enhanced constructor implementation
template <Cacheable T>
ResourceCache<T>::ResourceCache(const CacheConfig& config)
    : shard_mask_([&] {
        size_t shard_count = std::thread::hardware_concurrency();
        if (shard_count == 0) shard_count = 4;
        size_t power = 1;
        while (power < shard_count) power <<= 1;
        return power - 1;
    }()),
      max_size_(config.max_size),
      cleanup_interval_(config.cleanup_interval),
      config_(config) {
    size_t shard_count = shard_mask_ + 1;
    shards_.reserve(shard_count);
    // Ensure total capacity across all shards doesn't exceed max_size
    size_t per_shard_capacity = config.max_size / shard_count;
    if (per_shard_capacity == 0) per_shard_capacity = 1; // Each shard needs at least 1 capacity
    for (size_t i = 0; i < shard_count; ++i) {
        shards_.emplace_back(std::make_unique<Shard>(per_shard_capacity));
    }
    cleanup_thread_ = std::jthread([this] { cleanup_expired_entries(); });

    if (config_.enable_persistence && !config_.persistence_file.empty()) {
        persistence_thread_ = std::jthread([this] { persistence_worker(); });
    }
}

// New method implementations
template <Cacheable T>
const CacheMetrics& ResourceCache<T>::get_metrics() const noexcept {
    return metrics_;
}

template <Cacheable T>
void ResourceCache<T>::reset_metrics() noexcept {
    metrics_.hit_count = 0;
    metrics_.miss_count = 0;
    metrics_.eviction_count = 0;
    metrics_.expiration_count = 0;
    metrics_.total_operations = 0;
    metrics_.memory_usage_bytes = 0;
}

template <Cacheable T>
const CacheConfig& ResourceCache<T>::get_config() const noexcept {
    return config_;
}

template <Cacheable T>
void ResourceCache<T>::update_config(const CacheConfig& config) {
    config_ = config;
    set_max_size(config.max_size);
    cleanup_interval_ = config.cleanup_interval;
}

template <Cacheable T>
void ResourceCache<T>::warm_cache(const std::function<Vector<std::pair<String, T>>()>& loader,
                                  Duration expiration_time) {
    try {
        auto items = loader();
        insert_batch(items, expiration_time);
        spdlog::info("Cache warmed with {} items", items.size());
    } catch (const std::exception& e) {
        spdlog::error("Cache warming failed: {}", e.what());
    }
}

template <Cacheable T>
HashMap<String, double> ResourceCache<T>::get_health_metrics() const {
    HashMap<String, double> health;
    health["hit_ratio"] = metrics_.get_hit_ratio();
    health["memory_usage_mb"] = metrics_.get_memory_usage_mb();
    health["load_factor"] = static_cast<double>(current_size_.load()) / max_size_.load();
    health["avg_shard_load"] = static_cast<double>(current_size_.load()) / shards_.size();

    // Check memory health
    if (config_.max_memory_mb > 0) {
        health["memory_health"] = 1.0 - (metrics_.get_memory_usage_mb() / config_.max_memory_mb);
    } else {
        health["memory_health"] = 1.0;
    }

    return health;
}

template <Cacheable T>
void ResourceCache<T>::optimize() {
    spdlog::info("Starting cache optimization...");

    size_t cleaned_entries = 0;
    for (auto& shard_ptr : shards_) {
        std::unique_lock lock(shard_ptr->mutex);

        // Clean up expired entries
        Vector<String> expired_keys;
        for (const auto& [key, entry] : shard_ptr->entries) {
            if (Clock::now() >= entry.expiration_time) {
                expired_keys.push_back(key);
            }
        }

        for (const auto& key : expired_keys) {
            auto it = shard_ptr->map.find(key);
            if (it != shard_ptr->map.end()) {
                shard_ptr->lru_list.erase(it->second);
                shard_ptr->map.erase(it);
                shard_ptr->entries.erase(key);
                current_size_--;
                cleaned_entries++;
            }
        }
    }

    spdlog::info("Cache optimization completed. Cleaned {} expired entries", cleaned_entries);
}

template <Cacheable T>
size_t ResourceCache<T>::get_memory_usage() const noexcept {
    return metrics_.memory_usage_bytes.load();
}

template <Cacheable T>
bool ResourceCache<T>::is_healthy() const noexcept {
    if (config_.max_memory_mb > 0 && metrics_.get_memory_usage_mb() > config_.max_memory_mb) {
        return false;
    }
    return true;
}

template <Cacheable T>
void ResourceCache<T>::persist_now() {
    if (!config_.enable_persistence || config_.persistence_file.empty()) {
        return;
    }

    std::lock_guard lock(persistence_mutex_);
    persistence_needed_ = true;
}

template <Cacheable T>
Vector<HashMap<String, size_t>> ResourceCache<T>::get_shard_stats() const {
    Vector<HashMap<String, size_t>> stats;
    stats.reserve(shards_.size());

    for (const auto& shard_ptr : shards_) {
        std::shared_lock lock(shard_ptr->mutex);
        HashMap<String, size_t> shard_stats;
        shard_stats["entries"] = shard_ptr->entries.size();
        shard_stats["max_size"] = shard_ptr->max_size;
        shard_stats["memory_usage"] = shard_ptr->memory_usage.load();
        stats.push_back(std::move(shard_stats));
    }

    return stats;
}

// Enhanced eviction methods
template <Cacheable T>
void ResourceCache<T>::evict(Shard& shard) {
    switch (config_.eviction_policy) {
        case EvictionPolicy::LRU:
            evict_lru(shard);
            break;
        case EvictionPolicy::LFU:
            evict_lfu_optimized(shard);  // Use optimized version
            break;
        case EvictionPolicy::FIFO:
            evict_fifo(shard);
            break;
        case EvictionPolicy::RANDOM:
            evict_random(shard);
            break;
    }
    metrics_.eviction_count++;
}

template <Cacheable T>
void ResourceCache<T>::evict_lru(Shard& shard) {
    if (shard.lru_list.empty()) return;

    String key_to_evict = shard.lru_list.back();
    update_memory_usage(shard, key_to_evict, false);
    shard.lru_list.pop_back();
    shard.map.erase(key_to_evict);
    shard.entries.erase(key_to_evict);
    shard.entry_count--;
    current_size_--;

    if (remove_callback_) {
        std::lock_guard cb_lock(callback_mutex_);
        if (remove_callback_) remove_callback_(key_to_evict);
    }
}

template <Cacheable T>
void ResourceCache<T>::evict_lfu(Shard& shard) {
    if (shard.entries.empty()) return;

    // Find entry with lowest access count
    String key_to_evict;
    uint64_t min_access_count = UINT64_MAX;

    for (const auto& [key, entry] : shard.entries) {
        if (entry.access_count.load() < min_access_count) {
            min_access_count = entry.access_count.load();
            key_to_evict = key;
        }
    }

    auto it = shard.map.find(key_to_evict);
    if (it != shard.map.end()) {
        shard.lru_list.erase(it->second);
        shard.map.erase(it);
        shard.entries.erase(key_to_evict);
        current_size_--;

        if (remove_callback_) {
            std::lock_guard cb_lock(callback_mutex_);
            if (remove_callback_) remove_callback_(key_to_evict);
        }
    }
}

template <Cacheable T>
void ResourceCache<T>::evict_fifo(Shard& shard) {
    if (shard.fifo_queue.empty()) return;

    String key_to_evict = shard.fifo_queue.front();
    shard.fifo_queue.pop();

    auto it = shard.map.find(key_to_evict);
    if (it != shard.map.end()) {
        update_memory_usage(shard, key_to_evict, false);
        shard.lru_list.erase(it->second);
        shard.map.erase(it);
        shard.entries.erase(key_to_evict);
        shard.entry_count--;
        current_size_--;

        if (remove_callback_) {
            std::lock_guard cb_lock(callback_mutex_);
            if (remove_callback_) remove_callback_(key_to_evict);
        }
    }
}

template <Cacheable T>
void ResourceCache<T>::evict_random(Shard& shard) {
    if (shard.entries.empty()) return;

    // Thread-safe random eviction
    std::uniform_int_distribution<size_t> dist(0, shard.entries.size() - 1);
    size_t random_index = dist(shard.rng);

    auto it = shard.entries.begin();
    std::advance(it, random_index);
    String key_to_evict = it->first;

    auto map_it = shard.map.find(key_to_evict);
    if (map_it != shard.map.end()) {
        update_memory_usage(shard, key_to_evict, false);
        shard.lru_list.erase(map_it->second);
        shard.map.erase(map_it);
        shard.entries.erase(key_to_evict);
        shard.entry_count--;
        current_size_--;

        if (remove_callback_) {
            std::lock_guard cb_lock(callback_mutex_);
            if (remove_callback_) remove_callback_(key_to_evict);
        }
    }
}

template <Cacheable T>
void ResourceCache<T>::persistence_worker() {
    while (!stop_persistence_.load()) {
        std::this_thread::sleep_for(config_.persistence_interval);
        if (stop_persistence_.load()) break;

        if (persistence_needed_.load()) {
            try {
                write_to_json_file(String(config_.persistence_file),
                                 [](const T& /* value */) -> json {
                                     // Default serialization - users should override
                                     return json{};
                                 });
                persistence_needed_ = false;
                spdlog::info("Cache persisted to {}", config_.persistence_file);
            } catch (const std::exception& e) {
                spdlog::error("Cache persistence failed: {}", e.what());
            }
        }
    }
}

// Implementation of missing methods
template <Cacheable T>
size_t ResourceCache<T>::estimate_size(const T& value) const {
    if constexpr (std::is_arithmetic_v<T>) {
        return sizeof(T);
    } else if constexpr (requires { value.size(); }) {
        // For containers with size() method
        return sizeof(T) + value.size() * sizeof(typename T::value_type);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return sizeof(std::string) + value.capacity();
    } else if constexpr (std::is_same_v<T, String>) {
        return sizeof(String) + std::string(value).capacity();
    } else {
        // Default estimation for complex types
        return sizeof(T) + 64;  // Base size + estimated overhead
    }
}

template <Cacheable T>
void ResourceCache<T>::update_memory_usage(Shard& shard, const String& key, bool adding) {
    size_t key_size = sizeof(String) + std::string(key).capacity();
    size_t entry_size = 0;

    if (adding) {
        auto it = shard.entries.find(key);
        if (it != shard.entries.end()) {
            entry_size = sizeof(CacheEntry) + it->second.estimated_size;
        }
        shard.memory_usage.fetch_add(key_size + entry_size);
        metrics_.memory_usage_bytes.fetch_add(key_size + entry_size);
    } else {
        auto it = shard.entries.find(key);
        if (it != shard.entries.end()) {
            entry_size = sizeof(CacheEntry) + it->second.estimated_size;
            shard.memory_usage.fetch_sub(key_size + entry_size);
            metrics_.memory_usage_bytes.fetch_sub(key_size + entry_size);
        }
    }
}

template <Cacheable T>
void ResourceCache<T>::enforce_memory_limits() {
    if (config_.max_memory_mb == 0) return;

    size_t max_bytes = config_.max_memory_mb * 1024 * 1024;
    size_t current_bytes = metrics_.memory_usage_bytes.load();

    if (current_bytes > max_bytes) {
        // Evict from shards until under limit
        size_t bytes_to_free = current_bytes - max_bytes;
        size_t freed = 0;

        for (auto& shard_ptr : shards_) {
            if (freed >= bytes_to_free) break;

            std::unique_lock lock(shard_ptr->mutex);
            while (freed < bytes_to_free && !shard_ptr->entries.empty()) {
                size_t before = shard_ptr->memory_usage.load();
                evict(*shard_ptr);
                size_t after = shard_ptr->memory_usage.load();
                freed += (before - after);
            }
        }

        spdlog::info("Memory limit enforcement freed {} bytes", freed);
    }
}

template <Cacheable T>
void ResourceCache<T>::evict_lfu_optimized(Shard& shard) {
    if (shard.entries.empty()) return;

    // Rebuild heap if needed or if it's empty
    if (shard.lfu_heap.empty() || shard.lfu_heap.size() != shard.entries.size()) {
        // Clear and rebuild heap
        while (!shard.lfu_heap.empty()) shard.lfu_heap.pop();

        for (const auto& [key, entry] : shard.entries) {
            shard.lfu_heap.emplace(entry.access_count.load(), key);
        }
    }

    // Find valid entry with minimum access count
    String key_to_evict;
    while (!shard.lfu_heap.empty()) {
        auto [access_count, key] = shard.lfu_heap.top();
        shard.lfu_heap.pop();

        // Verify entry still exists and access count is current
        auto it = shard.entries.find(key);
        if (it != shard.entries.end() && it->second.access_count.load() == access_count) {
            key_to_evict = key;
            break;
        }
    }

    if (!key_to_evict.empty()) {
        auto it = shard.map.find(key_to_evict);
        if (it != shard.map.end()) {
            update_memory_usage(shard, key_to_evict, false);
            shard.lru_list.erase(it->second);
            shard.map.erase(it);
            shard.entries.erase(key_to_evict);
            shard.entry_count--;
            current_size_--;

            if (remove_callback_) {
                std::lock_guard cb_lock(callback_mutex_);
                if (remove_callback_) remove_callback_(key_to_evict);
            }
        }
    }
}

template <Cacheable T>
void ResourceCache<T>::cleanup_shard_deferred(Shard& shard) {
    if (!shard.needs_cleanup.load()) return;

    std::unique_lock lock(shard.mutex, std::try_to_lock);
    if (!lock.owns_lock()) return;  // Skip if shard is busy

    Vector<String> expired_keys;
    auto now = Clock::now();

    for (const auto& [key, entry] : shard.entries) {
        if (now >= entry.expiration_time) {
            expired_keys.push_back(key);
        }
    }

    for (const auto& key : expired_keys) {
        auto it = shard.map.find(key);
        if (it != shard.map.end()) {
            update_memory_usage(shard, key, false);
            shard.lru_list.erase(it->second);
            shard.map.erase(it);
            shard.entries.erase(key);
            shard.entry_count--;
            current_size_--;
            metrics_.expiration_count++;

            if (remove_callback_) {
                std::lock_guard cb_lock(callback_mutex_);
                if (remove_callback_) remove_callback_(key);
            }
        }
    }

    shard.needs_cleanup = false;
}

template <Cacheable T>
void ResourceCache<T>::optimize_shard(Shard& shard) {
    std::unique_lock lock(shard.mutex);

    // Clean expired entries
    cleanup_shard_deferred(shard);

    // Rebuild LFU heap if using LFU policy
    if (config_.eviction_policy == EvictionPolicy::LFU) {
        while (!shard.lfu_heap.empty()) shard.lfu_heap.pop();
        for (const auto& [key, entry] : shard.entries) {
            shard.lfu_heap.emplace(entry.access_count.load(), key);
        }
    }

    // Update FIFO queue consistency
    if (config_.eviction_policy == EvictionPolicy::FIFO) {
        // Rebuild FIFO queue to match current entries
        std::queue<String> new_queue;
        for (const auto& key : shard.lru_list) {
            if (shard.entries.count(key)) {
                new_queue.push(key);
            }
        }
        shard.fifo_queue = std::move(new_queue);
    }
}

// Implementation of enhanced methods
template <Cacheable T>
HashMap<String, String> ResourceCache<T>::get_health_report() const {
    HashMap<String, String> report;

    auto metrics = get_metrics();
    double hit_ratio = metrics.get_hit_ratio();
    double memory_usage_mb = metrics.get_memory_usage_mb();

    // Overall health assessment
    if (hit_ratio < config_.unhealthy_hit_ratio_threshold) {
        report["status"] = "UNHEALTHY";
        report["hit_ratio_warning"] = "Hit ratio (" + std::to_string(hit_ratio) +
                                     ") is below threshold (" + std::to_string(config_.unhealthy_hit_ratio_threshold) + ")";
    } else if (hit_ratio > 0.8) {
        report["status"] = "HEALTHY";
    } else {
        report["status"] = "WARNING";
        report["hit_ratio_info"] = "Hit ratio could be improved";
    }

    // Memory health
    if (config_.max_memory_mb > 0) {
        double memory_usage_ratio = memory_usage_mb / config_.max_memory_mb;
        if (memory_usage_ratio > config_.memory_pressure_threshold) {
            report["memory_warning"] = "Memory usage (" + std::to_string(memory_usage_mb) +
                                      "MB) is above threshold (" + std::to_string(config_.memory_pressure_threshold * 100) + "%)";
        }
    }

    // Eviction health
    uint64_t evictions = metrics.eviction_count.load();
    uint64_t total_ops = metrics.total_operations.load();
    if (total_ops > 0 && (static_cast<double>(evictions) / total_ops) > 0.1) {
        report["eviction_warning"] = "High eviction rate detected";
    }

    // Performance health
    if (config_.enable_performance_tracking) {
        double avg_access_time = metrics.get_average_access_time_ns();
        if (avg_access_time > 1000000) {  // 1ms
            report["performance_warning"] = "Average access time is high: " +
                                          std::to_string(avg_access_time / 1000000.0) + "ms";
        }
    }

    // Recommendations
    if (hit_ratio < 0.5) {
        report["recommendation"] = "Consider increasing cache size or adjusting eviction policy";
    } else if (memory_usage_mb > 0 && config_.max_memory_mb == 0) {
        report["recommendation"] = "Consider setting memory limits for better resource management";
    }

    return report;
}

template <Cacheable T>
void ResourceCache<T>::prefetch_related(const String& key, size_t count) {
    if (!config_.enable_prefetching || count == 0) return;

    // Simple prefetching strategy: prefetch keys with similar prefixes
    auto& shard = get_shard(key);
    std::shared_lock lock(shard.mutex);

    std::vector<String> candidates;
    std::string key_str = std::string(key);

    // Find keys with similar prefixes
    for (const auto& [existing_key, entry] : shard.entries) {
        std::string existing_key_str = std::string(existing_key);
        if (existing_key_str != key_str &&
            existing_key_str.find(key_str.substr(0, std::min(key_str.length(), size_t(3)))) == 0) {
            candidates.push_back(existing_key);
        }
        if (candidates.size() >= count) break;
    }

    // Prefetch candidates (mark as recently accessed)
    for (const auto& candidate : candidates) {
        auto it = shard.map.find(candidate);
        if (it != shard.map.end()) {
            // Move to front of LRU list
            shard.lru_list.erase(it->second);
            shard.lru_list.push_front(candidate);
            shard.map[candidate] = shard.lru_list.begin();

            // Update access count for LFU
            auto entry_it = shard.entries.find(candidate);
            if (entry_it != shard.entries.end()) {
                entry_it->second.access_count++;
            }
        }
    }

    spdlog::debug("Prefetched {} related items for key {}", candidates.size(), key.c_str());
}

template <Cacheable T>
HashMap<String, double> ResourceCache<T>::get_efficiency_metrics() const {
    HashMap<String, double> metrics;

    auto cache_metrics = get_metrics();

    // Basic efficiency metrics
    metrics["hit_ratio"] = cache_metrics.get_hit_ratio();
    metrics["memory_efficiency"] = current_size_.load() > 0 ?
        static_cast<double>(cache_metrics.memory_usage_bytes.load()) / current_size_.load() : 0.0;

    // Eviction efficiency
    uint64_t total_ops = cache_metrics.total_operations.load();
    metrics["eviction_rate"] = total_ops > 0 ?
        static_cast<double>(cache_metrics.eviction_count.load()) / total_ops : 0.0;

    // Expiration efficiency
    metrics["expiration_rate"] = total_ops > 0 ?
        static_cast<double>(cache_metrics.expiration_count.load()) / total_ops : 0.0;

    // Performance metrics
    if (config_.enable_performance_tracking) {
        metrics["avg_access_time_ms"] = cache_metrics.get_average_access_time_ns() / 1000000.0;
        metrics["avg_insert_time_ms"] = cache_metrics.get_average_insert_time_ns() / 1000000.0;
        metrics["max_access_time_ms"] = cache_metrics.max_access_time_ns.load() / 1000000.0;
        metrics["max_insert_time_ms"] = cache_metrics.max_insert_time_ns.load() / 1000000.0;
    }

    // Shard distribution efficiency
    size_t total_entries = 0;
    for (const auto& shard_ptr : shards_) {
        std::shared_lock lock(shard_ptr->mutex);
        total_entries += shard_ptr->entry_count.load();
    }

    if (total_entries > 0) {
        double expected_per_shard = static_cast<double>(total_entries) / shards_.size();
        double variance = 0.0;

        for (const auto& shard_ptr : shards_) {
            std::shared_lock lock(shard_ptr->mutex);
            double diff = shard_ptr->entry_count.load() - expected_per_shard;
            variance += diff * diff;
        }

        variance /= shards_.size();
        metrics["shard_balance_coefficient"] = 1.0 / (1.0 + std::sqrt(variance) / expected_per_shard);
    }

    return metrics;
}

template <Cacheable T>
void ResourceCache<T>::set_performance_tracking(bool enabled) {
    config_.enable_performance_tracking = enabled;
    if (!enabled) {
        // Reset performance metrics
        metrics_.total_access_time_ns = 0;
        metrics_.total_insert_time_ns = 0;
        metrics_.max_access_time_ns = 0;
        metrics_.max_insert_time_ns = 0;
    }
    spdlog::info("Performance tracking {}", enabled ? "enabled" : "disabled");
}

// Implementation of missing methods

template <Cacheable T>
bool ResourceCache<T>::is_expired(const String& key) const {
    auto& shard = get_shard(key);
    std::shared_lock lock(shard.mutex);

    auto it = shard.entries.find(key);
    if (it == shard.entries.end()) {
        return true;  // Entry doesn't exist, consider it expired
    }

    return Clock::now() >= it->second.expiration_time;
}

template <Cacheable T>
template<typename Loader>
auto ResourceCache<T>::async_load(const String& key, Loader&& loader,
                                  Duration expiration_time) -> std::future<void> {
    return std::async(std::launch::async, [this, key, loader = std::forward<Loader>(loader), expiration_time]() {
        auto value = loader();
        insert(key, value, expiration_time);
    });
}

template <Cacheable T>
void ResourceCache<T>::set_expiration_time(const String& key, Duration expiration_time) {
    auto& shard = get_shard(key);
    std::unique_lock lock(shard.mutex);

    auto it = shard.entries.find(key);
    if (it != shard.entries.end()) {
        it->second.expiration_time = Clock::now() + expiration_time;
    }
}

template <Cacheable T>
void ResourceCache<T>::write_to_file(const String& file_path,
                                     const std::function<String(const T&)>& serializer) const {
    std::ofstream file(file_path.c_str());
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + std::string(file_path.c_str()));
    }

    for (const auto& shard_ptr : shards_) {
        std::shared_lock lock(shard_ptr->mutex);
        for (const auto& [key, entry] : shard_ptr->entries) {
            if (Clock::now() <= entry.expiration_time) {
                file << key.c_str() << "\n" << serializer(entry.value) << "\n";
            }
        }
    }
}

template <Cacheable T>
void ResourceCache<T>::read_from_file(const String& file_path,
                                      const std::function<T(const String&)>& deserializer,
                                      Duration expiration_time) {
    std::ifstream file(file_path.c_str());
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for reading: " + std::string(file_path.c_str()));
    }

    std::string key_str, value_str;
    while (std::getline(file, key_str) && std::getline(file, value_str)) {
        String key(key_str.c_str());
        String value_string(value_str.c_str());
        T value = deserializer(value_string);
        insert(key, value, expiration_time);
    }
}

template <Cacheable T>
void ResourceCache<T>::removeExpired() {
    cleanup_expired_entries();
}

}  // namespace atom::search

#endif  // ATOM_SEARCH_CACHE_HPP
