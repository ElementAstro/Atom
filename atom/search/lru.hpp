#ifndef ATOM_SEARCH_LRU_HPP
#define ATOM_SEARCH_LRU_HPP

#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>
#include <random>

#include <spdlog/spdlog.h>

struct PairStringHash {
    std::size_t operator()(
        const std::pair<std::string, std::string>& p) const noexcept;
};

namespace atom::search {

template <typename Key, typename Value, typename Hash>
class ThreadSafeLRUCache;

/**
 * @brief Enhanced LRU cache configuration.
 */
struct LRUCacheConfig {
    size_t max_size = 1000;                           ///< Maximum cache size
    size_t concurrency_level = 0;                    ///< Number of shards (0 = auto)
    std::chrono::seconds default_ttl{0};             ///< Default TTL (0 = no expiration)
    bool enable_statistics = true;                   ///< Enable performance statistics
    bool enable_compression = false;                 ///< Enable value compression
    size_t max_memory_mb = 0;                       ///< Memory limit (0 = unlimited)
    std::chrono::milliseconds cleanup_interval{60000};  ///< Cleanup interval for expired items
    bool enable_prefetching = false;                ///< Enable predictive prefetching
    double prefetch_threshold = 0.8;                ///< Load factor threshold for prefetching

    // Enhanced configuration options
    bool enable_health_monitoring = true;            ///< Enable health monitoring
    double memory_pressure_threshold = 0.8;          ///< Memory pressure threshold (0.0-1.0)
    size_t batch_size_hint = 100;                   ///< Hint for batch operations
    bool enable_adaptive_sizing = false;             ///< Enable adaptive shard sizing
    size_t compression_threshold = 1024;             ///< Compress values larger than this

    // Performance tuning
    bool enable_fast_size_tracking = true;           ///< Use cached size for fast size() calls
    bool enable_background_cleanup = true;           ///< Enable background cleanup thread
    size_t cleanup_batch_size = 50;                 ///< Items to clean per batch

    // Monitoring and alerting
    std::chrono::seconds health_check_interval{300}; ///< Health check interval
    double unhealthy_hit_ratio_threshold = 0.1;      ///< Alert if hit ratio drops below
    size_t max_consecutive_evictions = 1000;         ///< Alert if too many evictions

    // Advanced features
    bool enable_write_through = false;               ///< Enable write-through caching
    bool enable_write_behind = false;                ///< Enable write-behind caching
    std::chrono::milliseconds write_behind_delay{100}; ///< Write-behind delay
};

/**
 * @brief Enhanced cache metrics.
 */
struct LRUCacheMetrics {
    std::atomic<uint64_t> hit_count{0};
    std::atomic<uint64_t> miss_count{0};
    std::atomic<uint64_t> eviction_count{0};
    std::atomic<uint64_t> expiration_count{0};
    std::atomic<uint64_t> total_operations{0};
    std::atomic<size_t> memory_usage_bytes{0};
    std::atomic<uint64_t> prefetch_count{0};
    std::atomic<uint64_t> compression_saves_bytes{0};

    // Default constructor
    LRUCacheMetrics() = default;

    // Copy constructor
    LRUCacheMetrics(const LRUCacheMetrics& other)
        : hit_count(other.hit_count.load()),
          miss_count(other.miss_count.load()),
          eviction_count(other.eviction_count.load()),
          expiration_count(other.expiration_count.load()),
          total_operations(other.total_operations.load()),
          memory_usage_bytes(other.memory_usage_bytes.load()),
          prefetch_count(other.prefetch_count.load()),
          compression_saves_bytes(other.compression_saves_bytes.load()) {}

    // Move constructor
    LRUCacheMetrics(LRUCacheMetrics&& other) noexcept
        : hit_count(other.hit_count.load()),
          miss_count(other.miss_count.load()),
          eviction_count(other.eviction_count.load()),
          expiration_count(other.expiration_count.load()),
          total_operations(other.total_operations.load()),
          memory_usage_bytes(other.memory_usage_bytes.load()),
          prefetch_count(other.prefetch_count.load()),
          compression_saves_bytes(other.compression_saves_bytes.load()) {}

    // Copy assignment operator
    LRUCacheMetrics& operator=(const LRUCacheMetrics& other) {
        if (this != &other) {
            hit_count.store(other.hit_count.load());
            miss_count.store(other.miss_count.load());
            eviction_count.store(other.eviction_count.load());
            expiration_count.store(other.expiration_count.load());
            total_operations.store(other.total_operations.load());
            memory_usage_bytes.store(other.memory_usage_bytes.load());
            prefetch_count.store(other.prefetch_count.load());
            compression_saves_bytes.store(other.compression_saves_bytes.load());
        }
        return *this;
    }

    // Move assignment operator
    LRUCacheMetrics& operator=(LRUCacheMetrics&& other) noexcept {
        if (this != &other) {
            hit_count.store(other.hit_count.load());
            miss_count.store(other.miss_count.load());
            eviction_count.store(other.eviction_count.load());
            expiration_count.store(other.expiration_count.load());
            total_operations.store(other.total_operations.load());
            memory_usage_bytes.store(other.memory_usage_bytes.load());
            prefetch_count.store(other.prefetch_count.load());
            compression_saves_bytes.store(other.compression_saves_bytes.load());
        }
        return *this;
    }

    double get_hit_ratio() const noexcept {
        uint64_t total = hit_count.load() + miss_count.load();
        return total > 0 ? static_cast<double>(hit_count.load()) / total : 0.0;
    }

    double get_memory_usage_mb() const noexcept {
        return static_cast<double>(memory_usage_bytes.load()) / (1024.0 * 1024.0);
    }

    double get_compression_ratio() const noexcept {
        uint64_t saves = compression_saves_bytes.load();
        uint64_t usage = memory_usage_bytes.load();
        return usage > 0 ? static_cast<double>(saves) / (usage + saves) : 0.0;
    }
};

/**
 * @brief Custom exceptions for LRU Cache operations.
 */
class LRUCacheException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class LRUCacheIOException : public LRUCacheException {
public:
    using LRUCacheException::LRUCacheException;
};

/**
 * @brief A shard of the LRU cache. This is an internal implementation detail.
 */
template <typename Key, typename Value, typename Hash>
class LRUCacheShard {
public:
    using KeyValuePair = std::pair<Key, Value>;
    using ListIterator = typename std::list<KeyValuePair>::iterator;
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using ValuePtr = std::shared_ptr<Value>;

    struct CacheItem {
        ValuePtr value;
        TimePoint expiryTime;
        ListIterator iterator;
    };

private:
    friend class ThreadSafeLRUCache<Key, Value, Hash>;

    LRUCacheShard(size_t max_shard_size,
                  ThreadSafeLRUCache<Key, Value, Hash>* parent);

    ValuePtr getShared(const Key& key);
    void put(const Key& key, Value value,
             std::optional<std::chrono::seconds> ttl);
    void putBatch(const std::vector<KeyValuePair>& items,
                  std::optional<std::chrono::seconds> ttl);
    bool erase(const Key& key);
    bool remove(const Key& key) { return erase(key); }  // Alias for erase for compatibility
    void clear();
    size_t size() const;
    size_t maxSize() const;
    bool contains(const Key& key) const;
    size_t pruneExpired();
    void resize(size_t new_max_size);
    std::vector<Key> keys() const;
    std::vector<Value> values() const;
    void saveToStream(std::ofstream& ofs) const;

    bool isExpired(const CacheItem& item) const;
    void trim();

    mutable std::shared_mutex mutex_;
    std::list<KeyValuePair> cache_items_list_;
    std::unordered_map<Key, CacheItem, Hash> cache_items_map_;
    size_t max_size_;
    ThreadSafeLRUCache<Key, Value, Hash>* parent_;
};

/**
 * @brief A thread-safe, sharded LRU (Least Recently Used) cache for
 * high-concurrency scenarios.
 *
 * This class implements a highly-optimized LRU cache by sharding the data
 * across multiple independent caches, each with its own lock. This design
 * minimizes lock contention and improves scalability on multi-core systems.
 *
 * @tparam Key Type of the cache keys.
 * @tparam Value Type of the cache values.
 * @tparam Hash Hash function for keys. Defaults to std::hash<Key>.
 */
template <typename Key, typename Value, typename Hash = std::hash<Key>>
class ThreadSafeLRUCache {
public:
    using KeyValuePair = std::pair<Key, Value>;
    using ValuePtr = std::shared_ptr<Value>;
    using BatchKeyType = std::vector<Key>;
    using BatchValueType = std::vector<ValuePtr>;

    struct CacheStatistics {
        size_t hitCount;
        size_t missCount;
        float hitRate;
        size_t size;
        size_t maxSize;
        float loadFactor;
    };

    /**
     * @brief Constructs a ThreadSafeLRUCache.
     * @param max_size The maximum number of items the cache can hold.
     * @param concurrency_level The number of shards to distribute data across.
     * Defaults to hardware concurrency.
     * @throws std::invalid_argument if max_size is zero.
     */
    explicit ThreadSafeLRUCache(size_t max_size, size_t concurrency_level = 0);

    /**
     * @brief Constructs a ThreadSafeLRUCache with configuration.
     * @param config Cache configuration options.
     * @throws std::invalid_argument if max_size is zero.
     */
    explicit ThreadSafeLRUCache(const LRUCacheConfig& config);

    ~ThreadSafeLRUCache() = default;

    /**
     * @brief Retrieves a value from the cache.
     * @param key The key of the item to retrieve.
     * @return An optional containing the value if found and not expired,
     * otherwise std::nullopt.
     */
    [[nodiscard]] std::optional<Value> get(const Key& key);

    /**
     * @brief Retrieves a value as a shared pointer from the cache.
     * @param key The key of the item to retrieve.
     * @return A shared pointer to the value if found and not expired, otherwise
     * nullptr.
     */
    [[nodiscard]] ValuePtr getShared(const Key& key);

    /**
     * @brief Batch retrieval of multiple values from the cache.
     * @param keys Vector of keys to retrieve.
     * @return Vector of shared pointers to values (nullptr for missing items).
     */
    [[nodiscard]] BatchValueType getBatch(const BatchKeyType& keys);

    /**
     * @brief Checks if a key exists in the cache.
     * @param key The key to check.
     * @return True if the key exists and is not expired, false otherwise.
     */
    [[nodiscard]] bool contains(const Key& key) const;

    /**
     * @brief Inserts or updates a value in the cache.
     * @param key The key of the item to insert or update.
     * @param value The value to associate with the key.
     * @param ttl Optional time-to-live duration for the cache item.
     */
    template<typename Duration = std::chrono::seconds>
    void put(const Key& key, Value value,
             std::optional<Duration> ttl = std::nullopt) {
        std::optional<std::chrono::seconds> ttl_seconds;
        if (ttl.has_value()) {
            ttl_seconds = std::chrono::duration_cast<std::chrono::seconds>(ttl.value());
        }
        put_impl(key, std::move(value), ttl_seconds);
    }

private:
    void put_impl(const Key& key, Value value,
                  std::optional<std::chrono::seconds> ttl);

public:

    /**
     * @brief Inserts or updates a batch of values in the cache.
     * @param items Vector of key-value pairs to insert.
     * @param ttl Optional time-to-live duration for all cache items.
     */
    template<typename Duration = std::chrono::seconds>
    void putBatch(const std::vector<KeyValuePair>& items,
                  std::optional<Duration> ttl = std::nullopt) {
        std::optional<std::chrono::seconds> ttl_seconds;
        if (ttl.has_value()) {
            ttl_seconds = std::chrono::duration_cast<std::chrono::seconds>(ttl.value());
        }
        putBatch_impl(items, ttl_seconds);
    }

private:
    void putBatch_impl(const std::vector<KeyValuePair>& items,
                       std::optional<std::chrono::seconds> ttl);

public:

    /**
     * @brief Erases an item from the cache.
     * @param key The key of the item to remove.
     * @return True if the item was found and removed, false otherwise.
     */
    bool erase(const Key& key);

    /**
     * @brief Alias for erase method for compatibility.
     * @param key The key of the item to remove.
     * @return True if the item was found and removed, false otherwise.
     */
    bool remove(const Key& key) { return erase(key); }

    /**
     * @brief Clears all items from the cache.
     */
    void clear();

    /**
     * @brief Removes and returns the least recently used item.
     * @return An optional containing the LRU key-value pair if cache is not empty,
     * otherwise std::nullopt.
     */
    [[nodiscard]] std::optional<KeyValuePair> popLru();

    /**
     * @brief Alias for popLru method for compatibility.
     * @return An optional containing the LRU key-value pair if cache is not empty,
     * otherwise std::nullopt.
     */
    [[nodiscard]] std::optional<KeyValuePair> pop_lru() { return popLru(); }

    /**
     * @brief Retrieves all keys in the cache.
     * @return A vector containing all keys currently in the cache.
     */
    [[nodiscard]] std::vector<Key> keys() const;

    /**
     * @brief Retrieves all values in the cache.
     * @return A vector containing all values currently in the cache.
     */
    [[nodiscard]] std::vector<Value> values() const;

    /**
     * @brief Resizes the cache to a new maximum size.
     * @param new_max_size The new maximum size of the cache.
     * @throws std::invalid_argument if new_max_size is zero.
     */
    void resize(size_t new_max_size);

    /**
     * @brief Gets the current size of the cache.
     * @return The number of items currently in the cache.
     */
    [[nodiscard]] size_t size() const;

    /**
     * @brief Gets the maximum size of the cache.
     * @return The maximum number of items the cache can hold.
     */
    [[nodiscard]] size_t maxSize() const noexcept;

    /**
     * @brief Gets the current load factor of the cache.
     * @return The load factor of the cache.
     */
    [[nodiscard]] float loadFactor() const;

    /**
     * @brief Checks if the cache is empty.
     * @return True if the cache is empty, false otherwise.
     */
    [[nodiscard]] bool empty() const;

    /**
     * @brief Sets a callback function to be called on item insertion.
     * @param callback The callback function.
     */
    void setInsertCallback(
        std::function<void(const Key&, const Value&)> callback);

    /**
     * @brief Sets a callback function to be called on item erasure.
     * @param callback The callback function.
     */
    void setEraseCallback(std::function<void(const Key&)> callback);

    /**
     * @brief Sets a callback function to be called when the cache is cleared.
     * @param callback The callback function.
     */
    void setClearCallback(std::function<void()> callback);

    /**
     * @brief Gets the hit rate of the cache.
     * @return The hit rate as a float between 0.0 and 1.0.
     */
    [[nodiscard]] float hitRate() const noexcept;

    /**
     * @brief Gets comprehensive statistics about the cache.
     * @return A CacheStatistics struct.
     */
    [[nodiscard]] CacheStatistics getStatistics() const;

    /**
     * @brief Alias for getStatistics method for compatibility.
     * @return A CacheStatistics struct.
     */
    [[nodiscard]] CacheStatistics get_statistics() const { return getStatistics(); }

    /**
     * @brief Resets cache statistics (hits and misses).
     */
    void resetStatistics() noexcept;

    /**
     * @brief Saves the cache contents to a file.
     * @param filename The name of the file to save to.
     * @throws LRUCacheIOException If file operations fail.
     */
    void saveToFile(const std::string& filename) const;

    /**
     * @brief Loads cache contents from a file.
     * @param filename The name of the file to load from.
     * @throws LRUCacheIOException If file operations fail.
     */
    void loadFromFile(const std::string& filename);

    /**
     * @brief Prunes expired items from the cache.
     * @return Number of items pruned.
     */
    size_t pruneExpired();

    /**
     * @brief Asynchronously retrieves a value from the cache.
     * @param key The key of the item to retrieve.
     * @return A future containing an optional with the value if found,
     * otherwise std::nullopt.
     */
    [[nodiscard]] std::future<std::optional<Value>> asyncGet(const Key& key);

    /**
     * @brief Alias for asyncGet method for compatibility.
     * @param key The key of the item to retrieve.
     * @return A future containing an optional with the value if found,
     * otherwise std::nullopt.
     */
    [[nodiscard]] std::future<std::optional<Value>> async_get(const Key& key) { return asyncGet(key); }

    /**
     * @brief Asynchronously inserts or updates a value in the cache.
     * @param key The key of the item to insert or update.
     * @param value The value to associate with the key.
     * @param ttl Optional time-to-live duration for the cache item.
     * @return A future that completes when the operation is done.
     */
    std::future<void> asyncPut(
        const Key& key, Value value,
        std::optional<std::chrono::seconds> ttl = std::nullopt);

    /**
     * @brief Sets the default TTL for cache items.
     * @param ttl The default time-to-live duration.
     */
    void setDefaultTTL(std::chrono::seconds ttl);

    /**
     * @brief Gets the default TTL for cache items.
     * @return The default time-to-live duration.
     */
    [[nodiscard]] std::optional<std::chrono::seconds> getDefaultTTL()
        const noexcept;

    /**
     * @brief Gets enhanced cache metrics.
     * @return Comprehensive cache metrics.
     */
    [[nodiscard]] const LRUCacheMetrics& getMetrics() const noexcept;

    /**
     * @brief Resets all cache metrics.
     */
    void resetMetrics() noexcept;

    /**
     * @brief Gets current cache configuration.
     * @return Current cache configuration.
     */
    [[nodiscard]] const LRUCacheConfig& getConfig() const noexcept;

    /**
     * @brief Updates cache configuration.
     * @param config New configuration.
     */
    void updateConfig(const LRUCacheConfig& config);

    /**
     * @brief Gets memory usage in bytes.
     * @return Current memory usage.
     */
    [[nodiscard]] size_t getMemoryUsage() const noexcept;

    /**
     * @brief Checks if cache is healthy (within memory limits, etc.).
     * @return True if cache is healthy.
     */
    [[nodiscard]] bool isHealthy() const noexcept;

    /**
     * @brief Optimizes cache performance.
     */
    void optimize();

    /**
     * @brief Prefetches data based on access patterns.
     * @param predictor Function that predicts keys to prefetch.
     * @param loader Function that loads values for predicted keys.
     */
    void prefetch(const std::function<std::vector<Key>()>& predictor,
                  const std::function<std::optional<Value>(const Key&)>& loader);

    /**
     * @brief Gets detailed shard statistics.
     * @return Vector of per-shard statistics.
     */
    [[nodiscard]] std::vector<std::unordered_map<std::string, size_t>> getShardStats() const;

    /**
     * @brief Compresses a value if compression is enabled.
     * @param value The value to compress.
     * @return Compressed value or original if compression disabled.
     */
    Value compressValue(const Value& value) const;

    /**
     * @brief Decompresses a value if compression is enabled.
     * @param value The value to decompress.
     * @return Decompressed value or original if compression disabled.
     */
    Value decompressValue(const Value& value) const;

    /**
     * @brief Estimates memory usage of a value.
     * @param value The value to estimate.
     * @return Estimated memory usage in bytes.
     */
    size_t estimateValueSize(const Value& value) const;

    /**
     * @brief Gets comprehensive cache health report.
     *
     * @return Health report with recommendations.
     */
    std::unordered_map<std::string, std::string> getHealthReport() const;

    /**
     * @brief Gets cache efficiency metrics.
     *
     * @return Efficiency metrics and performance data.
     */
    std::unordered_map<std::string, double> getEfficiencyMetrics() const;

    /**
     * @brief Performs cache warmup with provided data.
     *
     * @param loader Function that returns key-value pairs to warm the cache.
     * @param ttl Optional TTL for warmed items.
     */
    void warmCache(const std::function<std::vector<KeyValuePair>()>& loader,
                   std::optional<std::chrono::seconds> ttl = std::nullopt);

    /**
     * @brief Enables or disables adaptive sizing.
     *
     * @param enabled Whether to enable adaptive sizing.
     */
    void setAdaptiveSizing(bool enabled);

    /**
     * @brief Gets load balancing statistics across shards.
     *
     * @return Load balancing metrics.
     */
    std::unordered_map<std::string, double> getLoadBalanceMetrics() const;

private:
    friend class LRUCacheShard<Key, Value, Hash>;

    LRUCacheShard<Key, Value, Hash>& get_shard(const Key& key) const;
    void startCleanupThread();
    void stopCleanupThread();
    void cleanupWorker();

    size_t max_size_;
    const size_t concurrency_level_;
    std::vector<std::unique_ptr<LRUCacheShard<Key, Value, Hash>>> shards_;
    Hash key_hasher_;

    // Enhanced configuration and metrics
    LRUCacheConfig config_;
    mutable LRUCacheMetrics metrics_;

    // Legacy statistics for backward compatibility
    std::atomic<size_t> hit_count_{0};
    std::atomic<size_t> miss_count_{0};

    // Callbacks
    std::function<void(const Key&, const Value&)> on_insert_;
    std::function<void(const Key&)> on_erase_;
    std::function<void()> on_clear_;
    std::optional<std::chrono::seconds> default_ttl_;

    // Background cleanup
    std::unique_ptr<std::thread> cleanup_thread_;
    std::atomic<bool> stop_cleanup_{false};

    // Performance optimizations
    mutable std::atomic<size_t> cached_size_{0};  ///< Cached total size for fast access
    mutable std::atomic<bool> size_dirty_{true};  ///< Flag indicating size cache needs update
};

}  // namespace atom::search

// Implementation for PairStringHash
inline std::size_t PairStringHash::operator()(
    const std::pair<std::string, std::string>& p) const noexcept {
    std::size_t h1 = std::hash<std::string>{}(p.first);
    std::size_t h2 = std::hash<std::string>{}(p.second);
    // Combine the two hashes
    return h1 ^ (h2 << 1);
}

namespace atom::search {

// LRUCacheShard Implementation

template <typename Key, typename Value, typename Hash>
atom::search::LRUCacheShard<Key, Value, Hash>::LRUCacheShard(
    size_t max_shard_size, atom::search::ThreadSafeLRUCache<Key, Value, Hash>* parent)
    : max_size_(max_shard_size), parent_(parent) {}

template <typename Key, typename Value, typename Hash>
typename atom::search::LRUCacheShard<Key, Value, Hash>::ValuePtr
atom::search::LRUCacheShard<Key, Value, Hash>::getShared(const Key& key) {
    std::unique_lock lock(mutex_);
    auto it = cache_items_map_.find(key);

    if (it == cache_items_map_.end() || isExpired(it->second)) {
        parent_->miss_count_++;
        if (it != cache_items_map_.end()) {
            if (parent_->on_erase_)
                parent_->on_erase_(key);
            cache_items_list_.erase(it->second.iterator);
            cache_items_map_.erase(it);
        }
        return nullptr;
    }

    parent_->hit_count_++;
    cache_items_list_.splice(cache_items_list_.begin(), cache_items_list_,
                             it->second.iterator);
    return it->second.value;
}

template <typename Key, typename Value, typename Hash>
void atom::search::LRUCacheShard<Key, Value, Hash>::put(
    const Key& key, Value value, std::optional<std::chrono::seconds> ttl) {
    std::unique_lock lock(mutex_);
    auto effective_ttl = ttl.has_value() ? ttl : parent_->default_ttl_;
    auto expiry_time = effective_ttl.has_value()
                           ? (Clock::now() + *effective_ttl)
                           : TimePoint::max();
    auto value_ptr = std::make_shared<Value>(std::move(value));

    auto it = cache_items_map_.find(key);
    if (it != cache_items_map_.end()) {
        cache_items_list_.splice(cache_items_list_.begin(), cache_items_list_,
                                 it->second.iterator);
        it->second.value = value_ptr;
        it->second.expiryTime = expiry_time;
    } else {
        cache_items_list_.emplace_front(key, *value_ptr);
        cache_items_map_[key] = {value_ptr, expiry_time,
                                 cache_items_list_.begin()};
        trim();
    }
    if (parent_->on_insert_)
        parent_->on_insert_(key, *value_ptr);
}

template <typename Key, typename Value, typename Hash>
void atom::search::LRUCacheShard<Key, Value, Hash>::putBatch(
    const std::vector<KeyValuePair>& items,
    std::optional<std::chrono::seconds> ttl) {
    std::unique_lock lock(mutex_);
    auto effective_ttl = ttl.has_value() ? ttl : parent_->default_ttl_;
    auto expiry_time = effective_ttl.has_value()
                           ? (Clock::now() + *effective_ttl)
                           : TimePoint::max();

    for (const auto& [key, value] : items) {
        auto value_ptr = std::make_shared<Value>(value);
        auto it = cache_items_map_.find(key);
        if (it != cache_items_map_.end()) {
            cache_items_list_.splice(cache_items_list_.begin(),
                                     cache_items_list_, it->second.iterator);
            it->second.value = value_ptr;
            it->second.expiryTime = expiry_time;
        } else {
            cache_items_list_.emplace_front(key, value);
            cache_items_map_[key] = {value_ptr, expiry_time,
                                     cache_items_list_.begin()};
        }
        if (parent_->on_insert_)
            parent_->on_insert_(key, value);
    }
    trim();
}

template <typename Key, typename Value, typename Hash>
bool atom::search::LRUCacheShard<Key, Value, Hash>::erase(const Key& key) {
    std::unique_lock lock(mutex_);
    auto it = cache_items_map_.find(key);
    if (it == cache_items_map_.end()) {
        return false;
    }
    if (parent_->on_erase_)
        parent_->on_erase_(key);
    cache_items_list_.erase(it->second.iterator);
    cache_items_map_.erase(it);
    return true;
}

template <typename Key, typename Value, typename Hash>
void atom::search::LRUCacheShard<Key, Value, Hash>::clear() {
    std::unique_lock lock(mutex_);
    cache_items_map_.clear();
    cache_items_list_.clear();
}

template <typename Key, typename Value, typename Hash>
size_t atom::search::LRUCacheShard<Key, Value, Hash>::size() const {
    std::shared_lock lock(mutex_);
    return cache_items_map_.size();
}

template <typename Key, typename Value, typename Hash>
size_t atom::search::LRUCacheShard<Key, Value, Hash>::maxSize() const {
    return max_size_;
}

template <typename Key, typename Value, typename Hash>
bool atom::search::LRUCacheShard<Key, Value, Hash>::contains(const Key& key) const {
    std::shared_lock lock(mutex_);
    auto it = cache_items_map_.find(key);
    return it != cache_items_map_.end() && !isExpired(it->second);
}

template <typename Key, typename Value, typename Hash>
size_t atom::search::LRUCacheShard<Key, Value, Hash>::pruneExpired() {
    std::unique_lock lock(mutex_);
    size_t pruned_count = 0;
    auto it = cache_items_list_.begin();
    while (it != cache_items_list_.end()) {
        auto map_it = cache_items_map_.find(it->first);
        if (map_it != cache_items_map_.end() && isExpired(map_it->second)) {
            if (parent_->on_erase_)
                parent_->on_erase_(it->first);
            cache_items_map_.erase(map_it);
            it = cache_items_list_.erase(it);
            pruned_count++;
        } else {
            ++it;
        }
    }
    return pruned_count;
}

template <typename Key, typename Value, typename Hash>
void atom::search::LRUCacheShard<Key, Value, Hash>::resize(size_t new_max_size) {
    std::unique_lock lock(mutex_);
    max_size_ = new_max_size;
    trim();
}

template <typename Key, typename Value, typename Hash>
std::vector<Key> atom::search::LRUCacheShard<Key, Value, Hash>::keys() const {
    std::shared_lock lock(mutex_);
    std::vector<Key> all_keys;
    all_keys.reserve(cache_items_list_.size());
    for (const auto& pair : cache_items_list_) {
        all_keys.push_back(pair.first);
    }
    return all_keys;
}

template <typename Key, typename Value, typename Hash>
std::vector<Value> atom::search::LRUCacheShard<Key, Value, Hash>::values() const {
    std::shared_lock lock(mutex_);
    std::vector<Value> all_values;
    all_values.reserve(cache_items_list_.size());
    for (const auto& pair : cache_items_list_) {
        all_values.push_back(pair.second);
    }
    return all_values;
}

template <typename Key, typename Value, typename Hash>
void atom::search::LRUCacheShard<Key, Value, Hash>::saveToStream(std::ofstream& ofs) const {
    std::shared_lock lock(mutex_);
    for (const auto& pair : cache_items_list_) {
        auto it = cache_items_map_.find(pair.first);
        if (it == cache_items_map_.end() || isExpired(it->second))
            continue;

        auto now = Clock::now();
        int64_t remainingTtl = -1;
        if (it->second.expiryTime != TimePoint::max()) {
            auto ttlDuration = std::chrono::duration_cast<std::chrono::seconds>(
                it->second.expiryTime - now);
            remainingTtl = ttlDuration.count();
            if (remainingTtl <= 0)
                continue;
        }

        ofs.write(reinterpret_cast<const char*>(&pair.first),
                  sizeof(pair.first));
        ofs.write(reinterpret_cast<const char*>(&remainingTtl),
                  sizeof(remainingTtl));

        if constexpr (std::is_trivially_copyable_v<Value>) {
            ofs.write(reinterpret_cast<const char*>(&pair.second),
                      sizeof(pair.second));
        } else if constexpr (std::is_same_v<Value, std::string>) {
            size_t valueSize = pair.second.size();
            ofs.write(reinterpret_cast<const char*>(&valueSize),
                      sizeof(valueSize));
            ofs.write(pair.second.c_str(), valueSize);
        } else {
            // For non-trivial types, a proper serialization would be needed.
            // This is a placeholder and might not compile for complex types.
            static_assert(std::is_trivially_copyable_v<Value> ||
                              std::is_same_v<Value, std::string>,
                          "Value type must be trivially copyable or "
                          "std::string for file operations.");
        }
    }
}

template <typename Key, typename Value, typename Hash>
bool atom::search::LRUCacheShard<Key, Value, Hash>::isExpired(const CacheItem& item) const {
    return item.expiryTime != TimePoint::max() &&
           Clock::now() > item.expiryTime;
}

template <typename Key, typename Value, typename Hash>
void atom::search::LRUCacheShard<Key, Value, Hash>::trim() {
    while (cache_items_map_.size() > max_size_) {
        if (cache_items_list_.empty())
            return;
        const auto& key = cache_items_list_.back().first;
        if (parent_->on_erase_)
            parent_->on_erase_(key);
        cache_items_map_.erase(key);
        cache_items_list_.pop_back();
    }
}

// ThreadSafeLRUCache Implementation

template <typename Key, typename Value, typename Hash>
atom::search::ThreadSafeLRUCache<Key, Value, Hash>::ThreadSafeLRUCache(
    size_t max_size, size_t concurrency_level)
    : max_size_(max_size),
      concurrency_level_(concurrency_level > 0
                             ? concurrency_level
                             : std::thread::hardware_concurrency()) {
    if (max_size == 0) {
        throw std::invalid_argument(
            "Cache max size must be greater than zero.");
    }
    if (concurrency_level_ == 0) {
        const_cast<size_t&>(concurrency_level_) = 1;
    }

    // Initialize default config
    config_.max_size = max_size;
    config_.concurrency_level = concurrency_level_;

    shards_.reserve(concurrency_level_);
    size_t max_shard_size =
        (max_size + concurrency_level_ - 1) / concurrency_level_;
    for (size_t i = 0; i < concurrency_level_; ++i) {
        shards_.emplace_back(std::unique_ptr<LRUCacheShard<Key, Value, Hash>>(
            new LRUCacheShard<Key, Value, Hash>(max_shard_size, this)));
    }

    if (config_.cleanup_interval.count() > 0) {
        startCleanupThread();
    }
}

template <typename Key, typename Value, typename Hash>
atom::search::ThreadSafeLRUCache<Key, Value, Hash>::ThreadSafeLRUCache(const LRUCacheConfig& config)
    : max_size_(config.max_size),
      concurrency_level_(config.concurrency_level > 0
                             ? config.concurrency_level
                             : std::thread::hardware_concurrency()),
      config_(config) {
    if (config.max_size == 0) {
        throw std::invalid_argument(
            "Cache max size must be greater than zero.");
    }
    if (concurrency_level_ == 0) {
        const_cast<size_t&>(concurrency_level_) = 1;
    }

    // Set default TTL if specified
    if (config.default_ttl.count() > 0) {
        default_ttl_ = config.default_ttl;
    }

    shards_.reserve(concurrency_level_);
    size_t max_shard_size =
        (config.max_size + concurrency_level_ - 1) / concurrency_level_;
    for (size_t i = 0; i < concurrency_level_; ++i) {
        shards_.emplace_back(std::unique_ptr<LRUCacheShard<Key, Value, Hash>>(
            new LRUCacheShard<Key, Value, Hash>(max_shard_size, this)));
    }

    if (config.cleanup_interval.count() > 0) {
        startCleanupThread();
    }
}

template <typename Key, typename Value, typename Hash>
std::optional<Value> atom::search::ThreadSafeLRUCache<Key, Value, Hash>::get(const Key& key) {
    auto sharedPtr = getShared(key);
    if (sharedPtr) {
        return *sharedPtr;
    }
    return std::nullopt;
}

template <typename Key, typename Value, typename Hash>
typename atom::search::ThreadSafeLRUCache<Key, Value, Hash>::ValuePtr
atom::search::ThreadSafeLRUCache<Key, Value, Hash>::getShared(const Key& key) {
    return get_shard(key).getShared(key);
}

template <typename Key, typename Value, typename Hash>
typename atom::search::ThreadSafeLRUCache<Key, Value, Hash>::BatchValueType
atom::search::ThreadSafeLRUCache<Key, Value, Hash>::getBatch(const BatchKeyType& keys) {
    BatchValueType results;
    results.reserve(keys.size());
    for (const auto& key : keys) {
        results.push_back(getShared(key));
    }
    return results;
}

template <typename Key, typename Value, typename Hash>
bool ThreadSafeLRUCache<Key, Value, Hash>::contains(const Key& key) const {
    return get_shard(key).contains(key);
}

template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::put_impl(
    const Key& key, Value value, std::optional<std::chrono::seconds> ttl) {
    get_shard(key).put(key, std::move(value), ttl);
}

template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::putBatch_impl(
    const std::vector<KeyValuePair>& items,
    std::optional<std::chrono::seconds> ttl) {
    if (items.empty()) return;

    // Group items by shard for better performance
    std::vector<std::vector<KeyValuePair>> shard_groups(concurrency_level_);
    for (const auto& item : items) {
        size_t shard_index = key_hasher_(item.first) % concurrency_level_;
        shard_groups[shard_index].push_back(item);
    }

    // Process each shard group
    for (size_t i = 0; i < concurrency_level_; ++i) {
        if (!shard_groups[i].empty()) {
            shards_[i]->putBatch(shard_groups[i], ttl);
        }
    }

    // Mark size as dirty since we added items
    size_dirty_.store(true, std::memory_order_release);

    // Update metrics
    if (config_.enable_statistics) {
        metrics_.total_operations.fetch_add(items.size(), std::memory_order_relaxed);
    }
}

template <typename Key, typename Value, typename Hash>
bool ThreadSafeLRUCache<Key, Value, Hash>::erase(const Key& key) {
    return get_shard(key).erase(key);
}



template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::clear() {
    for (auto& shard : shards_) {
        shard->clear();
    }
    if (on_clear_)
        on_clear_();
}

template <typename Key, typename Value, typename Hash>
std::optional<typename ThreadSafeLRUCache<Key, Value, Hash>::KeyValuePair>
ThreadSafeLRUCache<Key, Value, Hash>::popLru() {
    // Find the shard with the least recently used item (at the back of the list)
    std::optional<KeyValuePair> lru_item;

    for (auto& shard : shards_) {
        std::unique_lock lock(shard->mutex_);
        if (!shard->cache_items_list_.empty()) {
            // The back of the list contains the least recently used item
            auto& back_item = shard->cache_items_list_.back();
            auto it = shard->cache_items_map_.find(back_item.first);
            if (it != shard->cache_items_map_.end()) {
                lru_item = back_item;
                // Remove the item from this shard
                shard->cache_items_map_.erase(it);
                shard->cache_items_list_.pop_back();
                break;  // Found and removed the LRU item
            }
        }
    }

    return lru_item;
}

template <typename Key, typename Value, typename Hash>
std::vector<Key> ThreadSafeLRUCache<Key, Value, Hash>::keys() const {
    std::vector<Key> all_keys;
    for (const auto& shard : shards_) {
        auto shard_keys = shard->keys();
        all_keys.insert(all_keys.end(), shard_keys.begin(), shard_keys.end());
    }
    return all_keys;
}

template <typename Key, typename Value, typename Hash>
std::vector<Value> ThreadSafeLRUCache<Key, Value, Hash>::values() const {
    std::vector<Value> all_values;
    for (const auto& shard : shards_) {
        auto shard_values = shard->values();
        all_values.insert(all_values.end(), shard_values.begin(),
                          shard_values.end());
    }
    return all_values;
}

template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::resize(size_t new_max_size) {
    if (new_max_size == 0) {
        throw std::invalid_argument(
            "Cache max size must be greater than zero.");
    }
    max_size_ = new_max_size;
    size_t new_max_shard_size =
        (new_max_size + concurrency_level_ - 1) / concurrency_level_;
    for (auto& shard : shards_) {
        shard->resize(new_max_shard_size);
    }
}

template <typename Key, typename Value, typename Hash>
size_t ThreadSafeLRUCache<Key, Value, Hash>::size() const {
    // Use cached size if available and not dirty
    if (!size_dirty_.load(std::memory_order_acquire)) {
        return cached_size_.load(std::memory_order_relaxed);
    }

    // Recalculate size
    size_t total_size = 0;
    for (const auto& shard : shards_) {
        total_size += shard->size();
    }

    // Update cache
    cached_size_.store(total_size, std::memory_order_relaxed);
    size_dirty_.store(false, std::memory_order_release);

    return total_size;
}

template <typename Key, typename Value, typename Hash>
size_t ThreadSafeLRUCache<Key, Value, Hash>::maxSize() const noexcept {
    return max_size_;
}

template <typename Key, typename Value, typename Hash>
float ThreadSafeLRUCache<Key, Value, Hash>::loadFactor() const {
    return static_cast<float>(size()) / static_cast<float>(max_size_);
}

template <typename Key, typename Value, typename Hash>
bool ThreadSafeLRUCache<Key, Value, Hash>::empty() const {
    return size() == 0;
}

template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::setInsertCallback(
    std::function<void(const Key&, const Value&)> callback) {
    on_insert_ = std::move(callback);
}

template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::setEraseCallback(
    std::function<void(const Key&)> callback) {
    on_erase_ = std::move(callback);
}

template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::setClearCallback(
    std::function<void()> callback) {
    on_clear_ = std::move(callback);
}

template <typename Key, typename Value, typename Hash>
float ThreadSafeLRUCache<Key, Value, Hash>::hitRate() const noexcept {
    size_t hits = hit_count_.load(std::memory_order_relaxed);
    size_t misses = miss_count_.load(std::memory_order_relaxed);
    size_t total = hits + misses;
    return total == 0 ? 0.0f
                      : static_cast<float>(hits) / static_cast<float>(total);
}

template <typename Key, typename Value, typename Hash>
typename ThreadSafeLRUCache<Key, Value, Hash>::CacheStatistics
ThreadSafeLRUCache<Key, Value, Hash>::getStatistics() const {
    size_t current_size = size();
    return {hit_count_.load(std::memory_order_relaxed),
            miss_count_.load(std::memory_order_relaxed),
            hitRate(),
            current_size,
            max_size_,
            static_cast<float>(current_size) / static_cast<float>(max_size_)};
}

template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::resetStatistics() noexcept {
    hit_count_.store(0, std::memory_order_relaxed);
    miss_count_.store(0, std::memory_order_relaxed);
}

template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::saveToFile(
    const std::string& filename) const {
    std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        throw LRUCacheIOException("Failed to open file for writing: " +
                                  filename);
    }
    size_t current_size = size();
    ofs.write(reinterpret_cast<const char*>(&current_size),
              sizeof(current_size));
    ofs.write(reinterpret_cast<const char*>(&max_size_), sizeof(max_size_));

    for (const auto& shard : shards_) {
        shard->saveToStream(ofs);
    }
    if (!ofs) {
        throw LRUCacheIOException("Failed writing to file: " + filename);
    }
}

template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::loadFromFile(
    const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) {
        throw LRUCacheIOException("Failed to open file for reading: " +
                                  filename);
    }
    clear();

    size_t stored_size, stored_max_size;
    ifs.read(reinterpret_cast<char*>(&stored_size), sizeof(stored_size));
    ifs.read(reinterpret_cast<char*>(&stored_max_size),
             sizeof(stored_max_size));
    if (!ifs)
        throw LRUCacheIOException("Failed to read cache metadata from file");

    resize(stored_max_size);

    for (size_t i = 0; i < stored_size && ifs; ++i) {
        Key key;
        int64_t ttlSeconds;
        Value value;

        ifs.read(reinterpret_cast<char*>(&key), sizeof(key));
        ifs.read(reinterpret_cast<char*>(&ttlSeconds), sizeof(ttlSeconds));

        if constexpr (std::is_trivially_copyable_v<Value>) {
            ifs.read(reinterpret_cast<char*>(&value), sizeof(value));
        } else if constexpr (std::is_same_v<Value, std::string>) {
            size_t valueSize;
            ifs.read(reinterpret_cast<char*>(&valueSize), sizeof(valueSize));
            value.resize(valueSize);
            ifs.read(&value[0], static_cast<std::streamsize>(valueSize));
        } else {
            static_assert(std::is_trivially_copyable_v<Value> ||
                              std::is_same_v<Value, std::string>,
                          "Value type must be trivially copyable or "
                          "std::string for file operations.");
        }

        if (!ifs)
            break;

        std::optional<std::chrono::seconds> ttl =
            (ttlSeconds >= 0) ? std::optional<std::chrono::seconds>(
                                    std::chrono::seconds(ttlSeconds))
                              : std::nullopt;

        put(key, std::move(value), ttl);
    }
}

template <typename Key, typename Value, typename Hash>
size_t ThreadSafeLRUCache<Key, Value, Hash>::pruneExpired() {
    size_t total_pruned = 0;
    for (auto& shard : shards_) {
        total_pruned += shard->pruneExpired();
    }
    return total_pruned;
}

template <typename Key, typename Value, typename Hash>
std::future<std::optional<Value>>
ThreadSafeLRUCache<Key, Value, Hash>::asyncGet(const Key& key) {
    return std::async(std::launch::async, [this, key]() { return get(key); });
}

template <typename Key, typename Value, typename Hash>
std::future<void> ThreadSafeLRUCache<Key, Value, Hash>::asyncPut(
    const Key& key, Value value, std::optional<std::chrono::seconds> ttl) {
    return std::async(std::launch::async,
                      [this, key, value = std::move(value), ttl]() mutable {
                          put(key, std::move(value), ttl);
                      });
}

template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::setDefaultTTL(
    std::chrono::seconds ttl) {
    default_ttl_ = ttl;
}

template <typename Key, typename Value, typename Hash>
std::optional<std::chrono::seconds>
ThreadSafeLRUCache<Key, Value, Hash>::getDefaultTTL() const noexcept {
    return default_ttl_;
}

template <typename Key, typename Value, typename Hash>
LRUCacheShard<Key, Value, Hash>&
ThreadSafeLRUCache<Key, Value, Hash>::get_shard(const Key& key) const {
    return *shards_[key_hasher_(key) % concurrency_level_];
}

// Implementation of missing enhanced methods
template <typename Key, typename Value, typename Hash>
const LRUCacheMetrics& ThreadSafeLRUCache<Key, Value, Hash>::getMetrics() const noexcept {
    // Update metrics from legacy counters for backward compatibility
    metrics_.hit_count.store(hit_count_.load(), std::memory_order_relaxed);
    metrics_.miss_count.store(miss_count_.load(), std::memory_order_relaxed);
    return metrics_;
}

template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::resetMetrics() noexcept {
    metrics_.hit_count = 0;
    metrics_.miss_count = 0;
    metrics_.eviction_count = 0;
    metrics_.expiration_count = 0;
    metrics_.total_operations = 0;
    metrics_.memory_usage_bytes = 0;
    metrics_.prefetch_count = 0;
    metrics_.compression_saves_bytes = 0;

    // Reset legacy counters too
    hit_count_ = 0;
    miss_count_ = 0;
}

template <typename Key, typename Value, typename Hash>
const LRUCacheConfig& ThreadSafeLRUCache<Key, Value, Hash>::getConfig() const noexcept {
    return config_;
}

template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::updateConfig(const LRUCacheConfig& config) {
    config_ = config;

    // Update max size if changed
    if (config.max_size != max_size_) {
        resize(config.max_size);
    }

    // Update default TTL
    if (config.default_ttl.count() > 0) {
        default_ttl_ = config.default_ttl;
    } else {
        default_ttl_.reset();
    }

    // Restart cleanup thread if interval changed
    if (cleanup_thread_ && config.cleanup_interval != config_.cleanup_interval) {
        stopCleanupThread();
        if (config.cleanup_interval.count() > 0) {
            startCleanupThread();
        }
    } else if (!cleanup_thread_ && config.cleanup_interval.count() > 0) {
        startCleanupThread();
    }
}

template <typename Key, typename Value, typename Hash>
size_t ThreadSafeLRUCache<Key, Value, Hash>::getMemoryUsage() const noexcept {
    if (config_.enable_statistics) {
        return metrics_.memory_usage_bytes.load();
    }

    // Estimate memory usage
    size_t total_memory = 0;
    size_t current_size = size();

    // Rough estimation: each entry has key + value + overhead
    total_memory += current_size * (sizeof(Key) + sizeof(Value) + 64); // 64 bytes overhead
    total_memory += shards_.size() * sizeof(LRUCacheShard<Key, Value, Hash>);

    return total_memory;
}

template <typename Key, typename Value, typename Hash>
bool ThreadSafeLRUCache<Key, Value, Hash>::isHealthy() const noexcept {
    // Check memory limits
    if (config_.max_memory_mb > 0) {
        double memory_mb = static_cast<double>(getMemoryUsage()) / (1024.0 * 1024.0);
        if (memory_mb > config_.max_memory_mb) {
            return false;
        }
    }

    // Check hit ratio if we have enough operations
    if (config_.enable_statistics) {
        uint64_t total_ops = metrics_.hit_count.load() + metrics_.miss_count.load();
        if (total_ops > 100) {  // Only check if we have enough data
            double hit_ratio = metrics_.get_hit_ratio();
            if (hit_ratio < 0.1) {  // Less than 10% hit ratio is concerning
                return false;
            }
        }
    }

    return true;
}

template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::optimize() {
    spdlog::info("Starting LRU cache optimization...");

    size_t total_pruned = pruneExpired();
    size_dirty_.store(true, std::memory_order_release);  // Force size recalculation

    spdlog::info("LRU cache optimization completed. Pruned {} expired items", total_pruned);
}

template <typename Key, typename Value, typename Hash>
void ThreadSafeLRUCache<Key, Value, Hash>::prefetch(
    const std::function<std::vector<Key>()>& predictor,
    const std::function<std::optional<Value>(const Key&)>& loader) {

    if (!config_.enable_prefetching) return;

    try {
        auto keys_to_prefetch = predictor();
        size_t prefetched = 0;

        for (const auto& key : keys_to_prefetch) {
            if (!contains(key)) {
                auto value = loader(key);
                if (value.has_value()) {
                    put(key, std::move(*value), default_ttl_);
                    prefetched++;
                }
            }
        }

        if (config_.enable_statistics) {
            metrics_.prefetch_count.fetch_add(prefetched, std::memory_order_relaxed);
        }

        spdlog::debug("Prefetched {} items", prefetched);
    } catch (const std::exception& e) {
        spdlog::error("Prefetch failed: {}", e.what());
    }
}

template <typename Key, typename Value, typename Hash>
std::vector<std::unordered_map<std::string, size_t>>
ThreadSafeLRUCache<Key, Value, Hash>::getShardStats() const {
    std::vector<std::unordered_map<std::string, size_t>> stats;
    stats.reserve(shards_.size());

    for (size_t i = 0; i < shards_.size(); ++i) {
        std::unordered_map<std::string, size_t> shard_stats;
        shard_stats["shard_id"] = i;
        shard_stats["size"] = shards_[i]->size();
        shard_stats["max_size"] = shards_[i]->maxSize();
        shard_stats["load_factor"] = static_cast<size_t>(
            (static_cast<double>(shards_[i]->size()) / shards_[i]->maxSize()) * 100);
        stats.push_back(std::move(shard_stats));
    }

    return stats;
}

template <typename Key, typename Value, typename Hash>
Value ThreadSafeLRUCache<Key, Value, Hash>::compressValue(const Value& value) const {
    // Placeholder for compression - would need actual compression library
    if (config_.enable_compression) {
        // TODO: Implement actual compression
        spdlog::debug("Compression not yet implemented");
    }
    return value;
}

template <typename Key, typename Value, typename Hash>
Value ThreadSafeLRUCache<Key, Value, Hash>::decompressValue(const Value& value) const {
    // Placeholder for decompression - would need actual compression library
    if (config_.enable_compression) {
        // TODO: Implement actual decompression
        spdlog::debug("Decompression not yet implemented");
    }
    return value;
}

template <typename Key, typename Value, typename Hash>
size_t ThreadSafeLRUCache<Key, Value, Hash>::estimateValueSize(const Value& value) const {
    if constexpr (std::is_arithmetic_v<Value>) {
        return sizeof(Value);
    } else if constexpr (std::is_same_v<Value, std::string>) {
        return sizeof(std::string) + value.capacity();
    } else {
        // For other types, try to detect if they have a size() method
        // This is a simplified approach without C++20 concepts
        return sizeof(Value);
    }
        // Default estimation for complex types
        return sizeof(Value) + 64;  // Base size + estimated overhead
    }
}

// Implementation of enhanced methods
template <typename Key, typename Value, typename Hash>
std::unordered_map<std::string, std::string>
atom::search::ThreadSafeLRUCache<Key, Value, Hash>::getHealthReport() const {
    std::unordered_map<std::string, std::string> report;

    auto metrics = getMetrics();
    double hit_ratio = metrics.get_hit_ratio();
    double memory_usage_mb = static_cast<double>(getMemoryUsage()) / (1024.0 * 1024.0);

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

    // Size health
    size_t current_size = size();
    double load_factor = static_cast<double>(current_size) / max_size_;
    if (load_factor > 0.9) {
        report["size_warning"] = "Cache is nearly full (" + std::to_string(load_factor * 100) + "% capacity)";
    }

    // Recommendations
    if (hit_ratio < 0.5) {
        report["recommendation"] = "Consider increasing cache size or adjusting TTL settings";
    } else if (memory_usage_mb > 0 && config_.max_memory_mb == 0) {
        report["recommendation"] = "Consider setting memory limits for better resource management";
    }

    return report;
}

template <typename Key, typename Value, typename Hash>
std::unordered_map<std::string, double>
atom::search::ThreadSafeLRUCache<Key, Value, Hash>::getEfficiencyMetrics() const {
    std::unordered_map<std::string, double> metrics;

    auto cache_metrics = getMetrics();

    // Basic efficiency metrics
    metrics["hit_ratio"] = cache_metrics.get_hit_ratio();
    metrics["memory_efficiency"] = size() > 0 ?
        static_cast<double>(getMemoryUsage()) / size() : 0.0;

    // Load factor
    metrics["load_factor"] = static_cast<double>(size()) / max_size_;

    // Shard distribution efficiency
    auto load_balance = getLoadBalanceMetrics();
    metrics["shard_balance_coefficient"] = load_balance["balance_coefficient"];
    metrics["shard_variance"] = load_balance["variance"];

    // Performance metrics
    uint64_t total_ops = cache_metrics.hit_count.load() + cache_metrics.miss_count.load();
    if (total_ops > 0) {
        metrics["operations_per_second"] = static_cast<double>(total_ops) /
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    return metrics;
}

template <typename Key, typename Value, typename Hash>
void atom::search::ThreadSafeLRUCache<Key, Value, Hash>::warmCache(
    const std::function<std::vector<KeyValuePair>()>& loader,
    std::optional<std::chrono::seconds> ttl) {

    try {
        auto items = loader();
        spdlog::info("Warming cache with {} items", items.size());

        putBatch(items, ttl);

        spdlog::info("Cache warming completed successfully");
    } catch (const std::exception& e) {
        spdlog::error("Cache warming failed: {}", e.what());
    }
}

template <typename Key, typename Value, typename Hash>
void atom::search::ThreadSafeLRUCache<Key, Value, Hash>::setAdaptiveSizing(bool enabled) {
    config_.enable_adaptive_sizing = enabled;

    if (enabled) {
        spdlog::info("Adaptive sizing enabled for LRU cache");
        // TODO: Implement adaptive sizing logic
    } else {
        spdlog::info("Adaptive sizing disabled for LRU cache");
    }
}

template <typename Key, typename Value, typename Hash>
std::unordered_map<std::string, double>
atom::search::ThreadSafeLRUCache<Key, Value, Hash>::getLoadBalanceMetrics() const {
    std::unordered_map<std::string, double> metrics;

    std::vector<size_t> shard_sizes;
    shard_sizes.reserve(shards_.size());

    size_t total_entries = 0;
    for (const auto& shard : shards_) {
        size_t shard_size = shard->size();
        shard_sizes.push_back(shard_size);
        total_entries += shard_size;
    }

    if (total_entries > 0) {
        double expected_per_shard = static_cast<double>(total_entries) / shards_.size();
        double variance = 0.0;

        for (size_t shard_size : shard_sizes) {
            double diff = shard_size - expected_per_shard;
            variance += diff * diff;
        }

        variance /= shards_.size();
        metrics["variance"] = variance;
        metrics["expected_per_shard"] = expected_per_shard;
        metrics["balance_coefficient"] = expected_per_shard > 0 ?
            1.0 / (1.0 + std::sqrt(variance) / expected_per_shard) : 1.0;

        // Calculate min/max load
        auto [min_it, max_it] = std::minmax_element(shard_sizes.begin(), shard_sizes.end());
        metrics["min_shard_size"] = static_cast<double>(*min_it);
        metrics["max_shard_size"] = static_cast<double>(*max_it);
        metrics["load_imbalance_ratio"] = expected_per_shard > 0 ?
            (*max_it - *min_it) / expected_per_shard : 0.0;
    } else {
        metrics["variance"] = 0.0;
        metrics["expected_per_shard"] = 0.0;
        metrics["balance_coefficient"] = 1.0;
        metrics["min_shard_size"] = 0.0;
        metrics["max_shard_size"] = 0.0;
        metrics["load_imbalance_ratio"] = 0.0;
    }

    return metrics;
}

// Cleanup thread implementation
template <typename Key, typename Value, typename Hash>
void atom::search::ThreadSafeLRUCache<Key, Value, Hash>::startCleanupThread() {
    if (cleanup_thread_) {
        stopCleanupThread();
    }

    stop_cleanup_.store(false);
    cleanup_thread_ = std::make_unique<std::thread>([this] { cleanupWorker(); });
    spdlog::info("LRU cache cleanup thread started");
}

template <typename Key, typename Value, typename Hash>
void atom::search::ThreadSafeLRUCache<Key, Value, Hash>::stopCleanupThread() {
    if (cleanup_thread_) {
        stop_cleanup_.store(true);
        cleanup_thread_.reset();
        spdlog::info("LRU cache cleanup thread stopped");
    }
}

template <typename Key, typename Value, typename Hash>
void atom::search::ThreadSafeLRUCache<Key, Value, Hash>::cleanupWorker() {
    while (!stop_cleanup_.load()) {
        std::this_thread::sleep_for(config_.cleanup_interval);

        if (stop_cleanup_.load()) break;

        try {
            size_t pruned = pruneExpired();
            if (pruned > 0) {
                size_dirty_.store(true, std::memory_order_release);
                if (config_.enable_statistics) {
                    metrics_.expiration_count.fetch_add(pruned, std::memory_order_relaxed);
                }
                spdlog::debug("Cleanup thread pruned {} expired items", pruned);
            }
        } catch (const std::exception& e) {
            spdlog::error("Cleanup thread error: {}", e.what());
        }
    }
}

#endif  // ATOM_SEARCH_LRU_HPP
