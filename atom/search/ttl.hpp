#ifndef ATOM_SEARCH_TTL_CACHE_HPP
#define ATOM_SEARCH_TTL_CACHE_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>

// Boost support
#if defined(ATOM_USE_BOOST_THREAD) || defined(ATOM_USE_BOOST_LOCKFREE)
#include <boost/config.hpp>
#endif

#ifdef ATOM_USE_BOOST_THREAD
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/atomic.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#endif

namespace atom::search {

// Define aliases based on whether we're using Boost or STL
#if defined(ATOM_USE_BOOST_THREAD)
template <typename T>
using SharedMutex = boost::shared_mutex;

template <typename T>
using SharedLock = boost::shared_lock<T>;

template <typename T>
using UniqueLock = boost::unique_lock<T>;

using CondVarAny = boost::condition_variable_any;
using Thread = boost::thread;
#else
template <typename T>
using SharedMutex = std::shared_mutex;

template <typename T>
using SharedLock = std::shared_lock<T>;

template <typename T>
using UniqueLock = std::unique_lock<T>;

using CondVarAny = std::condition_variable_any;
using Thread = std::thread;
#endif

#if defined(ATOM_USE_BOOST_LOCKFREE)
template <typename T>
using Atomic = boost::atomic<T>;
#else
template <typename T>
using Atomic = std::atomic<T>;
#endif

/**
 * @brief Custom exception class for TTL Cache errors.
 */
class TTLCacheException : public std::runtime_error {
public:
    explicit TTLCacheException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Cache statistics for monitoring performance and usage.
 */
struct CacheStatistics {
    size_t hits{0};
    size_t misses{0};
    size_t evictions{0};
    size_t expirations{0};
    size_t current_size{0};
    size_t max_capacity{0};
    double hit_rate{0.0};
    std::chrono::milliseconds avg_access_time{0};
};

/**
 * @brief Configuration options for TTL Cache behavior.
 */
struct CacheConfig {
    bool enable_automatic_cleanup{true};
    bool enable_statistics{true};
    bool thread_safe{true};
    size_t cleanup_batch_size{100};
    double load_factor{0.75};
};

/**
 * @brief A Time-to-Live (TTL) Cache with LRU eviction policy and advanced
 * features.
 *
 * This class implements a thread-safe TTL cache with LRU eviction policy.
 * Items in the cache expire after a specified duration and are evicted when
 * the cache exceeds its maximum capacity. The cache supports batch operations,
 * statistics collection, and customizable behavior through configuration
 * options.
 *
 * @tparam Key The type of the cache keys (must be hashable).
 * @tparam Value The type of the cache values.
 * @tparam Hash The hash function type for keys (defaults to std::hash<Key>).
 * @tparam KeyEqual The key equality comparison type (defaults to
 * std::equal_to<Key>).
 */
template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class TTLCache {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::milliseconds;
    using ValuePtr = std::shared_ptr<Value>;
    using EvictionCallback =
        std::function<void(const Key&, const Value&, bool)>;
    using KeyContainer = std::vector<Key>;
    using ValueContainer = std::vector<std::optional<Value>>;

    /**
     * @brief Constructs a TTLCache with the specified parameters.
     *
     * @param ttl Duration after which items expire and are removed from cache.
     * @param max_capacity Maximum number of items the cache can hold.
     * @param cleanup_interval Optional interval for cleanup operations
     * (defaults to ttl/2).
     * @param config Optional configuration for cache behavior.
     * @param eviction_callback Optional callback for eviction events.
     * @throws TTLCacheException if ttl <= 0 or max_capacity == 0
     */
    explicit TTLCache(Duration ttl, size_t max_capacity,
                      std::optional<Duration> cleanup_interval = std::nullopt,
                      CacheConfig config = CacheConfig{},
                      EvictionCallback eviction_callback = nullptr);

    /**
     * @brief Destructor that properly shuts down the cache.
     */
    ~TTLCache() noexcept;

    TTLCache(const TTLCache&) = delete;
    TTLCache& operator=(const TTLCache&) = delete;

    /**
     * @brief Move constructor.
     */
    TTLCache(TTLCache&& other) noexcept;

    /**
     * @brief Move assignment operator.
     */
    TTLCache& operator=(TTLCache&& other) noexcept;

    /**
     * @brief Inserts or updates a key-value pair in the cache.
     *
     * @param key The key to insert or update.
     * @param value The value associated with the key.
     * @param custom_ttl Optional custom TTL for this specific item.
     * @throws std::bad_alloc if memory allocation fails
     * @throws TTLCacheException for other internal errors
     */
    void put(const Key& key, const Value& value,
             std::optional<Duration> custom_ttl = std::nullopt);

    /**
     * @brief Inserts or updates a key-value pair using move semantics.
     *
     * @param key The key to insert or update.
     * @param value The value to be moved into the cache.
     * @param custom_ttl Optional custom TTL for this specific item.
     * @throws std::bad_alloc if memory allocation fails
     * @throws TTLCacheException for other internal errors
     */
    void put(const Key& key, Value&& value,
             std::optional<Duration> custom_ttl = std::nullopt);

    /**
     * @brief Emplace constructs a value directly in the cache.
     *
     * @tparam Args Constructor argument types for Value.
     * @param key The key for the new entry.
     * @param custom_ttl Optional custom TTL for this specific item.
     * @param args Arguments to forward to Value constructor.
     * @throws std::bad_alloc if memory allocation fails
     * @throws TTLCacheException for other internal errors
     */
    template <typename... Args>
    void emplace(const Key& key, std::optional<Duration> custom_ttl,
                 Args&&... args);

    /**
     * @brief Batch insertion of multiple key-value pairs.
     *
     * @param items Vector of key-value pairs to insert.
     * @param custom_ttl Optional custom TTL for all items in the batch.
     * @throws std::bad_alloc if memory allocation fails
     * @throws TTLCacheException for other internal errors
     */
    void batch_put(const std::vector<std::pair<Key, Value>>& items,
                   std::optional<Duration> custom_ttl = std::nullopt);

    /**
     * @brief Retrieves the value associated with the given key.
     *
     * @param key The key whose associated value is to be retrieved.
     * @param update_access_time Whether to update the access time (default:
     * true).
     * @return An optional containing the value if found and not expired.
     */
    [[nodiscard]] std::optional<Value> get(const Key& key,
                                           bool update_access_time = true);

    /**
     * @brief Retrieves the value as a shared pointer to avoid copies.
     *
     * @param key The key whose associated value is to be retrieved.
     * @param update_access_time Whether to update the access time (default:
     * true).
     * @return A shared pointer to the value if found and not expired.
     */
    [[nodiscard]] ValuePtr get_shared(const Key& key,
                                      bool update_access_time = true);

    /**
     * @brief Batch retrieval of multiple values by keys.
     *
     * @param keys Vector of keys to retrieve.
     * @param update_access_time Whether to update access times (default: true).
     * @return Vector of optional values corresponding to the keys.
     */
    [[nodiscard]] ValueContainer batch_get(const KeyContainer& keys,
                                           bool update_access_time = true);

    /**
     * @brief Retrieves a value or computes it if not present.
     *
     * @tparam Factory Function type that produces a Value.
     * @param key The key to lookup or create.
     * @param factory Function to create the value if not present.
     * @param custom_ttl Optional custom TTL for the computed value.
     * @return The value from cache or newly computed value.
     */
    template <typename Factory>
    Value get_or_compute(const Key& key, Factory&& factory,
                         std::optional<Duration> custom_ttl = std::nullopt);

    /**
     * @brief Removes an item from the cache.
     *
     * @param key The key to remove.
     * @return true if the item was found and removed, false otherwise.
     */
    bool remove(const Key& key) noexcept;

    /**
     * @brief Removes multiple items from the cache.
     *
     * @param keys Vector of keys to remove.
     * @return Number of items actually removed.
     */
    size_t batch_remove(const KeyContainer& keys) noexcept;

    /**
     * @brief Checks if a key exists in the cache and has not expired.
     *
     * @param key The key to check.
     * @return true if the key exists and has not expired.
     */
    [[nodiscard]] bool contains(const Key& key) const noexcept;

    /**
     * @brief Updates the TTL for an existing key.
     *
     * @param key The key whose TTL should be updated.
     * @param new_ttl The new TTL duration.
     * @return true if the key was found and updated, false otherwise.
     */
    bool update_ttl(const Key& key, Duration new_ttl) noexcept;

    /**
     * @brief Gets the remaining TTL for a key.
     *
     * @param key The key to check.
     * @return The remaining TTL duration, or nullopt if key doesn't exist.
     */
    [[nodiscard]] std::optional<Duration> get_remaining_ttl(
        const Key& key) const noexcept;

    /**
     * @brief Performs cache cleanup by removing expired items.
     */
    void cleanup() noexcept;

    /**
     * @brief Manually triggers an immediate cleanup operation.
     */
    void force_cleanup() noexcept;

    /**
     * @brief Gets comprehensive cache statistics.
     *
     * @return Current cache statistics.
     */
    [[nodiscard]] CacheStatistics get_statistics() const noexcept;

    /**
     * @brief Resets hit/miss counters and other statistics.
     */
    void reset_statistics() noexcept;

    /**
     * @brief Gets the cache hit rate.
     *
     * @return The ratio of cache hits to total accesses.
     */
    [[nodiscard]] double hit_rate() const noexcept;

    /**
     * @brief Gets the current number of items in the cache.
     *
     * @return The number of items in the cache.
     */
    [[nodiscard]] size_t size() const noexcept;

    /**
     * @brief Checks if the cache is empty.
     *
     * @return true if the cache contains no items.
     */
    [[nodiscard]] bool empty() const noexcept;

    /**
     * @brief Gets the maximum capacity of the cache.
     *
     * @return The maximum capacity of the cache.
     */
    [[nodiscard]] constexpr size_t capacity() const noexcept {
        return max_capacity_;
    }

    /**
     * @brief Gets the default TTL duration of the cache.
     *
     * @return The default TTL duration.
     */
    [[nodiscard]] constexpr Duration ttl() const noexcept { return ttl_; }

    /**
     * @brief Gets all keys currently in the cache.
     *
     * @return Vector containing all keys (not expired).
     */
    [[nodiscard]] KeyContainer get_keys() const;

    /**
     * @brief Clears all items from the cache and resets statistics.
     */
    void clear() noexcept;

    /**
     * @brief Resizes the cache to a new maximum capacity.
     *
     * If the new capacity is smaller than the current size,
     * the least recently used items will be evicted.
     *
     * @param new_capacity The new maximum capacity.
     * @throws TTLCacheException if new_capacity == 0
     */
    void resize(size_t new_capacity);

    /**
     * @brief Reserves space in the internal hash map.
     *
     * @param count The number of elements to reserve space for.
     */
    void reserve(size_t count);

    /**
     * @brief Sets or updates the eviction callback.
     *
     * @param callback The new eviction callback function.
     */
    void set_eviction_callback(EvictionCallback callback) noexcept;

    /**
     * @brief Updates the cache configuration.
     *
     * @param new_config The new configuration settings.
     */
    void update_config(const CacheConfig& new_config) noexcept;

    /**
     * @brief Gets the current cache configuration.
     *
     * @return The current configuration settings.
     */
    [[nodiscard]] CacheConfig get_config() const noexcept;

private:
    struct CacheItem {
        Key key;
        ValuePtr value;
        TimePoint expiry_time;
        TimePoint access_time;

        CacheItem(const Key& k, const Value& v, const TimePoint& expiry,
                  const TimePoint& access);
        CacheItem(const Key& k, Value&& v, const TimePoint& expiry,
                  const TimePoint& access);
        template <typename... Args>
        CacheItem(const Key& k, const TimePoint& expiry,
                  const TimePoint& access, Args&&... args);
    };

    using CacheList = std::list<CacheItem>;
    using CacheMap =
        std::unordered_map<Key, typename CacheList::iterator, Hash, KeyEqual>;

    Duration ttl_;
    Duration cleanup_interval_;
    size_t max_capacity_;
    CacheConfig config_;
    EvictionCallback eviction_callback_;

    CacheList cache_list_;
    CacheMap cache_map_;

    mutable SharedMutex<std::shared_mutex> mutex_;

    Atomic<size_t> hit_count_{0};
    Atomic<size_t> miss_count_{0};
    Atomic<size_t> eviction_count_{0};
    Atomic<size_t> expiration_count_{0};

    Thread cleaner_thread_;
    Atomic<bool> stop_flag_{false};
    CondVarAny cleanup_cv_;

    void cleaner_task() noexcept;
    void evict_items(UniqueLock<std::shared_mutex>& lock,
                     size_t count = 1) noexcept;
    void move_to_front(typename CacheList::iterator item);
    void notify_eviction(const Key& key, const Value& value,
                         bool expired) noexcept;
    [[nodiscard]] inline bool is_expired(
        const TimePoint& expiry_time) const noexcept;
    void cleanup_expired_items(UniqueLock<std::shared_mutex>& lock) noexcept;
};

template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::TTLCache(
    Duration ttl, size_t max_capacity, std::optional<Duration> cleanup_interval,
    CacheConfig config, EvictionCallback eviction_callback)
    : ttl_(ttl),
      cleanup_interval_(cleanup_interval.value_or(ttl / 2)),
      max_capacity_(max_capacity),
      config_(std::move(config)),
      eviction_callback_(std::move(eviction_callback)) {
    if (ttl <= Duration::zero()) {
        throw TTLCacheException("TTL must be greater than zero");
    }
    if (max_capacity == 0) {
        throw TTLCacheException("Maximum capacity must be greater than zero");
    }

    if (config_.enable_automatic_cleanup) {
        try {
            cleaner_thread_ = Thread([this] { cleaner_task(); });
        } catch (const std::exception& e) {
            throw TTLCacheException("Failed to create cleaner thread: " +
                                    std::string(e.what()));
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::~TTLCache() noexcept {
    try {
        stop_flag_ = true;
        cleanup_cv_.notify_all();
        if (cleaner_thread_.joinable()) {
            cleaner_thread_.join();
        }
    } catch (...) {
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::TTLCache(TTLCache&& other) noexcept
    : ttl_(other.ttl_),
      cleanup_interval_(other.cleanup_interval_),
      max_capacity_(other.max_capacity_),
      config_(std::move(other.config_)),
      eviction_callback_(std::move(other.eviction_callback_)),
      hit_count_(other.hit_count_.load()),
      miss_count_(other.miss_count_.load()),
      eviction_count_(other.eviction_count_.load()),
      expiration_count_(other.expiration_count_.load()) {
    UniqueLock lock(other.mutex_);
    cache_list_ = std::move(other.cache_list_);
    cache_map_ = std::move(other.cache_map_);

    other.stop_flag_ = true;
    other.cleanup_cv_.notify_all();
    if (other.cleaner_thread_.joinable()) {
        other.cleaner_thread_.join();
    }

    if (config_.enable_automatic_cleanup) {
        stop_flag_ = false;
        cleaner_thread_ = Thread([this] { cleaner_task(); });
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>&
TTLCache<Key, Value, Hash, KeyEqual>::operator=(TTLCache&& other) noexcept {
    if (this != &other) {
        stop_flag_ = true;
        cleanup_cv_.notify_all();
        if (cleaner_thread_.joinable()) {
            cleaner_thread_.join();
        }

        UniqueLock lock1(mutex_, std::defer_lock);
        UniqueLock lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);

        ttl_ = other.ttl_;
        cleanup_interval_ = other.cleanup_interval_;
        max_capacity_ = other.max_capacity_;
        config_ = std::move(other.config_);
        eviction_callback_ = std::move(other.eviction_callback_);
        cache_list_ = std::move(other.cache_list_);
        cache_map_ = std::move(other.cache_map_);
        hit_count_ = other.hit_count_.load();
        miss_count_ = other.miss_count_.load();
        eviction_count_ = other.eviction_count_.load();
        expiration_count_ = other.expiration_count_.load();

        other.stop_flag_ = true;
        other.cleanup_cv_.notify_all();
        if (other.cleaner_thread_.joinable()) {
            other.cleaner_thread_.join();
        }

        if (config_.enable_automatic_cleanup) {
            stop_flag_ = false;
            cleaner_thread_ = Thread([this] { cleaner_task(); });
        }
    }
    return *this;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::put(
    const Key& key, const Value& value, std::optional<Duration> custom_ttl) {
    try {
        UniqueLock lock(mutex_);
        auto now = Clock::now();
        auto expiry = now + (custom_ttl ? *custom_ttl : ttl_);

        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            notify_eviction(it->second->key, *(it->second->value), false);
            cache_list_.erase(it->second);
            cache_map_.erase(it);
        } else if (cache_list_.size() >= max_capacity_) {
            evict_items(lock);
        }

        cache_list_.emplace_front(key, value, expiry, now);
        cache_map_[key] = cache_list_.begin();

    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& e) {
        throw TTLCacheException("Error putting item in cache: " +
                                std::string(e.what()));
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::put(
    const Key& key, Value&& value, std::optional<Duration> custom_ttl) {
    try {
        UniqueLock lock(mutex_);
        auto now = Clock::now();
        auto expiry = now + (custom_ttl ? *custom_ttl : ttl_);

        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            notify_eviction(it->second->key, *(it->second->value), false);
            cache_list_.erase(it->second);
            cache_map_.erase(it);
        } else if (cache_list_.size() >= max_capacity_) {
            evict_items(lock);
        }

        cache_list_.emplace_front(key, std::move(value), expiry, now);
        cache_map_[key] = cache_list_.begin();

    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& e) {
        throw TTLCacheException("Error putting item in cache: " +
                                std::string(e.what()));
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
template <typename... Args>
void TTLCache<Key, Value, Hash, KeyEqual>::emplace(
    const Key& key, std::optional<Duration> custom_ttl, Args&&... args) {
    try {
        UniqueLock lock(mutex_);
        auto now = Clock::now();
        auto expiry = now + (custom_ttl ? *custom_ttl : ttl_);

        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            notify_eviction(it->second->key, *(it->second->value), false);
            cache_list_.erase(it->second);
            cache_map_.erase(it);
        } else if (cache_list_.size() >= max_capacity_) {
            evict_items(lock);
        }

        cache_list_.emplace_front(key, expiry, now,
                                  std::forward<Args>(args)...);
        cache_map_[key] = cache_list_.begin();

    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& e) {
        throw TTLCacheException("Error emplacing item in cache: " +
                                std::string(e.what()));
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::batch_put(
    const std::vector<std::pair<Key, Value>>& items,
    std::optional<Duration> custom_ttl) {
    if (items.empty())
        return;

    try {
        UniqueLock lock(mutex_);
        auto now = Clock::now();
        auto ttl_to_use = custom_ttl ? *custom_ttl : ttl_;

        cache_map_.reserve(
            std::min(cache_map_.size() + items.size(), max_capacity_));

        for (const auto& [key, value] : items) {
            auto expiry = now + ttl_to_use;

            auto it = cache_map_.find(key);
            if (it != cache_map_.end()) {
                notify_eviction(it->second->key, *(it->second->value), false);
                cache_list_.erase(it->second);
                cache_map_.erase(it);
            } else if (cache_list_.size() >= max_capacity_) {
                evict_items(lock);
            }

            cache_list_.emplace_front(key, value, expiry, now);
            cache_map_[key] = cache_list_.begin();
        }
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& e) {
        throw TTLCacheException("Error batch putting items: " +
                                std::string(e.what()));
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::optional<Value> TTLCache<Key, Value, Hash, KeyEqual>::get(
    const Key& key, bool update_access_time) {
    try {
        if (config_.thread_safe) {
            SharedLock lock(mutex_);
            return get_impl(key, update_access_time, lock);
        } else {
            UniqueLock lock(mutex_);
            return get_impl(key, update_access_time, lock);
        }
    } catch (...) {
        if (config_.enable_statistics) {
            miss_count_++;
        }
        return std::nullopt;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
typename TTLCache<Key, Value, Hash, KeyEqual>::ValuePtr
TTLCache<Key, Value, Hash, KeyEqual>::get_shared(const Key& key,
                                                 bool update_access_time) {
    try {
        if (config_.thread_safe) {
            SharedLock lock(mutex_);
            return get_shared_impl(key, update_access_time, lock);
        } else {
            UniqueLock lock(mutex_);
            return get_shared_impl(key, update_access_time, lock);
        }
    } catch (...) {
        if (config_.enable_statistics) {
            miss_count_++;
        }
        return nullptr;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
typename TTLCache<Key, Value, Hash, KeyEqual>::ValueContainer
TTLCache<Key, Value, Hash, KeyEqual>::batch_get(const KeyContainer& keys,
                                                bool update_access_time) {
    if (keys.empty())
        return {};

    ValueContainer results;
    results.reserve(keys.size());

    try {
        SharedLock lock(mutex_);
        auto now = Clock::now();

        for (const auto& key : keys) {
            auto it = cache_map_.find(key);
            if (it != cache_map_.end() &&
                !is_expired(it->second->expiry_time)) {
                if (config_.enable_statistics)
                    hit_count_++;

                if (update_access_time) {
                    it->second->access_time = now;
                    move_to_front(it->second);
                }

                results.emplace_back(*(it->second->value));
            } else {
                if (config_.enable_statistics)
                    miss_count_++;
                results.emplace_back(std::nullopt);
            }
        }
    } catch (...) {
        while (results.size() < keys.size()) {
            if (config_.enable_statistics)
                miss_count_++;
            results.emplace_back(std::nullopt);
        }
    }

    return results;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
template <typename Factory>
Value TTLCache<Key, Value, Hash, KeyEqual>::get_or_compute(
    const Key& key, Factory&& factory, std::optional<Duration> custom_ttl) {
    auto cached_value = get_shared(key);
    if (cached_value) {
        return *cached_value;
    }

    Value computed_value = factory();
    put(key, computed_value, custom_ttl);
    return computed_value;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::remove(const Key& key) noexcept {
    try {
        UniqueLock lock(mutex_);
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            notify_eviction(it->second->key, *(it->second->value), false);
            cache_list_.erase(it->second);
            cache_map_.erase(it);
            return true;
        }
        return false;
    } catch (...) {
        return false;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
size_t TTLCache<Key, Value, Hash, KeyEqual>::batch_remove(
    const KeyContainer& keys) noexcept {
    if (keys.empty())
        return 0;

    size_t removed_count = 0;
    try {
        UniqueLock lock(mutex_);
        for (const auto& key : keys) {
            auto it = cache_map_.find(key);
            if (it != cache_map_.end()) {
                notify_eviction(it->second->key, *(it->second->value), false);
                cache_list_.erase(it->second);
                cache_map_.erase(it);
                ++removed_count;
            }
        }
    } catch (...) {
    }
    return removed_count;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::contains(
    const Key& key) const noexcept {
    try {
        SharedLock lock(mutex_);
        auto it = cache_map_.find(key);
        return (it != cache_map_.end() && !is_expired(it->second->expiry_time));
    } catch (...) {
        return false;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::update_ttl(
    const Key& key, Duration new_ttl) noexcept {
    try {
        UniqueLock lock(mutex_);
        auto it = cache_map_.find(key);
        if (it != cache_map_.end() && !is_expired(it->second->expiry_time)) {
            it->second->expiry_time = Clock::now() + new_ttl;
            return true;
        }
        return false;
    } catch (...) {
        return false;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::optional<typename TTLCache<Key, Value, Hash, KeyEqual>::Duration>
TTLCache<Key, Value, Hash, KeyEqual>::get_remaining_ttl(
    const Key& key) const noexcept {
    try {
        SharedLock lock(mutex_);
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            auto now = Clock::now();
            if (it->second->expiry_time > now) {
                return std::chrono::duration_cast<Duration>(
                    it->second->expiry_time - now);
            }
        }
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::cleanup() noexcept {
    try {
        UniqueLock lock(mutex_);
        cleanup_expired_items(lock);
    } catch (...) {
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::force_cleanup() noexcept {
    cleanup();
    cleanup_cv_.notify_one();
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
CacheStatistics TTLCache<Key, Value, Hash, KeyEqual>::get_statistics()
    const noexcept {
    CacheStatistics stats;
    try {
        SharedLock lock(mutex_);
        stats.hits = hit_count_.load();
        stats.misses = miss_count_.load();
        stats.evictions = eviction_count_.load();
        stats.expirations = expiration_count_.load();
        stats.current_size = cache_map_.size();
        stats.max_capacity = max_capacity_;

        size_t total = stats.hits + stats.misses;
        stats.hit_rate =
            total > 0 ? static_cast<double>(stats.hits) / total : 0.0;
    } catch (...) {
    }
    return stats;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::reset_statistics() noexcept {
    if (config_.enable_statistics) {
        hit_count_ = 0;
        miss_count_ = 0;
        eviction_count_ = 0;
        expiration_count_ = 0;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
double TTLCache<Key, Value, Hash, KeyEqual>::hit_rate() const noexcept {
    if (!config_.enable_statistics)
        return 0.0;

    size_t hits = hit_count_.load();
    size_t misses = miss_count_.load();
    size_t total = hits + misses;
    return total > 0 ? static_cast<double>(hits) / total : 0.0;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
size_t TTLCache<Key, Value, Hash, KeyEqual>::size() const noexcept {
    try {
        SharedLock lock(mutex_);
        return cache_map_.size();
    } catch (...) {
        return 0;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::empty() const noexcept {
    return size() == 0;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
typename TTLCache<Key, Value, Hash, KeyEqual>::KeyContainer
TTLCache<Key, Value, Hash, KeyEqual>::get_keys() const {
    KeyContainer keys;
    try {
        SharedLock lock(mutex_);
        auto now = Clock::now();
        keys.reserve(cache_map_.size());

        for (const auto& [key, iter] : cache_map_) {
            if (!is_expired(iter->expiry_time)) {
                keys.push_back(key);
            }
        }
    } catch (...) {
    }
    return keys;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::clear() noexcept {
    try {
        UniqueLock lock(mutex_);

        if (eviction_callback_) {
            for (const auto& item : cache_list_) {
                notify_eviction(item.key, *(item.value), false);
            }
        }

        cache_list_.clear();
        cache_map_.clear();

        if (config_.enable_statistics) {
            hit_count_ = 0;
            miss_count_ = 0;
            eviction_count_ = 0;
            expiration_count_ = 0;
        }
    } catch (...) {
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::resize(size_t new_capacity) {
    if (new_capacity == 0) {
        throw TTLCacheException("New capacity must be greater than zero");
    }

    try {
        UniqueLock lock(mutex_);
        max_capacity_ = new_capacity;

        if (cache_list_.size() > max_capacity_) {
            size_t excess = cache_list_.size() - max_capacity_;
            evict_items(lock, excess);
        }
    } catch (const TTLCacheException&) {
        throw;
    } catch (const std::exception& e) {
        throw TTLCacheException("Error resizing cache: " +
                                std::string(e.what()));
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::reserve(size_t count) {
    try {
        UniqueLock lock(mutex_);
        cache_map_.reserve(count);
    } catch (...) {
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::set_eviction_callback(
    EvictionCallback callback) noexcept {
    try {
        UniqueLock lock(mutex_);
        eviction_callback_ = std::move(callback);
    } catch (...) {
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::update_config(
    const CacheConfig& new_config) noexcept {
    try {
        UniqueLock lock(mutex_);
        config_ = new_config;
    } catch (...) {
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
CacheConfig TTLCache<Key, Value, Hash, KeyEqual>::get_config() const noexcept {
    try {
        SharedLock lock(mutex_);
        return config_;
    } catch (...) {
        return CacheConfig{};
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::CacheItem::CacheItem(
    const Key& k, const Value& v, const TimePoint& expiry,
    const TimePoint& access)
    : key(k),
      value(std::make_shared<Value>(v)),
      expiry_time(expiry),
      access_time(access) {}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::CacheItem::CacheItem(
    const Key& k, Value&& v, const TimePoint& expiry, const TimePoint& access)
    : key(k),
      value(std::make_shared<Value>(std::move(v))),
      expiry_time(expiry),
      access_time(access) {}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
template <typename... Args>
TTLCache<Key, Value, Hash, KeyEqual>::CacheItem::CacheItem(
    const Key& k, const TimePoint& expiry, const TimePoint& access,
    Args&&... args)
    : key(k),
      value(std::make_shared<Value>(std::forward<Args>(args)...)),
      expiry_time(expiry),
      access_time(access) {}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::cleaner_task() noexcept {
    while (!stop_flag_) {
        try {
            SharedLock lock(mutex_);
            cleanup_cv_.wait_for(lock, cleanup_interval_,
                                 [this] { return stop_flag_.load(); });

            if (stop_flag_)
                break;

            lock.unlock();
            cleanup();

        } catch (...) {
            std::this_thread::sleep_for(cleanup_interval_);
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::evict_items(
    UniqueLock<std::shared_mutex>& lock, size_t count) noexcept {
    try {
        auto now = Clock::now();
        size_t expired_removed = 0;

        auto it = cache_list_.rbegin();
        while (count > 0 && it != cache_list_.rend()) {
            if (is_expired(it->expiry_time)) {
                auto key = it->key;
                auto value = it->value;
                auto list_it = std::next(it).base();
                --it;

                notify_eviction(key, *value, true);
                cache_list_.erase(list_it);
                cache_map_.erase(key);
                --count;
                ++expired_removed;

                if (config_.enable_statistics) {
                    expiration_count_++;
                }
            } else {
                ++it;
            }
        }

        while (count > 0 && !cache_list_.empty()) {
            auto& last = cache_list_.back();
            notify_eviction(last.key, *(last.value), false);
            cache_map_.erase(last.key);
            cache_list_.pop_back();
            --count;

            if (config_.enable_statistics) {
                eviction_count_++;
            }
        }
    } catch (...) {
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::move_to_front(
    typename CacheList::iterator item) {
    if (item != cache_list_.begin()) {
        cache_list_.splice(cache_list_.begin(), cache_list_, item);
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::notify_eviction(
    const Key& key, const Value& value, bool expired) noexcept {
    try {
        if (eviction_callback_) {
            eviction_callback_(key, value, expired);
        }
    } catch (...) {
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
inline bool TTLCache<Key, Value, Hash, KeyEqual>::is_expired(
    const TimePoint& expiry_time) const noexcept {
    return expiry_time <= Clock::now();
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::cleanup_expired_items(
    UniqueLock<std::shared_mutex>& lock) noexcept {
    try {
        auto now = Clock::now();
        size_t batch_count = 0;

        auto it = cache_list_.begin();
        while (it != cache_list_.end() &&
               batch_count < config_.cleanup_batch_size) {
            if (is_expired(it->expiry_time)) {
                auto key = it->key;
                auto value = it->value;
                it = cache_list_.erase(it);
                cache_map_.erase(key);

                notify_eviction(key, *value, true);
                ++batch_count;

                if (config_.enable_statistics) {
                    expiration_count_++;
                }
            } else {
                ++it;
            }
        }
    } catch (...) {
    }
}

}  // namespace atom::search

#endif  // ATOM_SEARCH_TTL_CACHE_HPP