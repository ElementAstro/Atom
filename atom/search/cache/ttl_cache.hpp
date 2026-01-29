/*
 * ttl_cache.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file ttl_cache.hpp
 * @brief TTLCache class - a Time-to-Live cache with LRU eviction.
 * @details Provides a thread-safe TTL cache with automatic expiration cleanup,
 *          batch operations, and customizable behavior through configuration.
 */

#ifndef ATOM_SEARCH_CACHE_TTL_CACHE_HPP
#define ATOM_SEARCH_CACHE_TTL_CACHE_HPP

#include <algorithm>
#include <list>
#include <unordered_map>

#include "exceptions.hpp"
#include "types.hpp"

namespace atom::search::cache {

/**
 * @brief TTL-specific cache configuration.
 */
struct TTLCacheConfig {
    bool enable_automatic_cleanup{true};
    bool enable_statistics{true};
    bool thread_safe{true};
    size_t cleanup_batch_size{100};
    double load_factor{0.75};
};

/**
 * @brief A Time-to-Live (TTL) Cache with LRU eviction policy.
 *
 * This class implements a thread-safe TTL cache with LRU eviction policy.
 * Items in the cache expire after a specified duration and are evicted when
 * the cache exceeds its maximum capacity.
 *
 * @tparam Key The type of the cache keys (must be hashable).
 * @tparam Value The type of the cache values.
 * @tparam Hash The hash function type for keys.
 * @tparam KeyEqual The key equality comparison type.
 */
template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class TTLCache {
public:
    using ValuePtr = std::shared_ptr<Value>;
    using EvictionCallback =
        std::function<void(const Key&, const Value&, bool)>;
    using KeyContainer = std::vector<Key>;
    using ValueContainer = std::vector<std::optional<Value>>;

    /**
     * @brief Constructs a TTLCache with the specified parameters.
     * @param ttl Duration after which items expire.
     * @param max_capacity Maximum number of items the cache can hold.
     * @param cleanup_interval Optional interval for cleanup operations.
     * @param config Optional configuration for cache behavior.
     * @param eviction_callback Optional callback for eviction events.
     */
    explicit TTLCache(Duration ttl, size_t max_capacity,
                      std::optional<Duration> cleanup_interval = std::nullopt,
                      TTLCacheConfig config = TTLCacheConfig{},
                      EvictionCallback eviction_callback = nullptr);

    ~TTLCache() noexcept;

    TTLCache(const TTLCache&) = delete;
    TTLCache& operator=(const TTLCache&) = delete;
    TTLCache(TTLCache&& other) noexcept;
    TTLCache& operator=(TTLCache&& other) noexcept;

    // Core operations
    void put(const Key& key, const Value& value,
             std::optional<Duration> custom_ttl = std::nullopt);
    void put(const Key& key, Value&& value,
             std::optional<Duration> custom_ttl = std::nullopt);

    template <typename... Args>
    void emplace(const Key& key, std::optional<Duration> custom_ttl,
                 Args&&... args);

    void batch_put(const std::vector<std::pair<Key, Value>>& items,
                   std::optional<Duration> custom_ttl = std::nullopt);

    [[nodiscard]] std::optional<Value> get(const Key& key,
                                           bool update_access_time = true);
    [[nodiscard]] ValuePtr get_shared(const Key& key,
                                      bool update_access_time = true);
    [[nodiscard]] ValueContainer batch_get(const KeyContainer& keys,
                                           bool update_access_time = true);

    template <typename Factory>
    Value get_or_compute(const Key& key, Factory&& factory,
                         std::optional<Duration> custom_ttl = std::nullopt);

    bool remove(const Key& key) noexcept;
    size_t batch_remove(const KeyContainer& keys) noexcept;
    [[nodiscard]] bool contains(const Key& key) const noexcept;

    // TTL management
    bool update_ttl(const Key& key, Duration new_ttl) noexcept;
    [[nodiscard]] std::optional<Duration> get_remaining_ttl(
        const Key& key) const noexcept;

    // Size and capacity
    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] size_t capacity() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    void clear() noexcept;
    void resize(size_t new_capacity);

    // Statistics
    [[nodiscard]] CacheStatistics get_statistics() const noexcept;
    void reset_statistics() noexcept;
    [[nodiscard]] double hit_rate() const noexcept;

    // Configuration
    void set_eviction_callback(EvictionCallback callback);
    void set_default_ttl(Duration ttl);

private:
    struct CacheEntry {
        ValuePtr value;
        TimePoint expiry_time;
        TimePoint last_access;
        typename std::list<Key>::iterator lru_iterator;
    };

    void evict_items(UniqueLock<SharedMutex>& lock, size_t count);
    void cleanup_expired_items(UniqueLock<SharedMutex>& lock);
    void cleanup_thread_func();

    template <typename LockType>
    std::optional<Value> get_impl(const Key& key, bool update_access_time,
                                  LockType& lock);

    template <typename LockType>
    ValuePtr get_shared_impl(const Key& key, bool update_access_time,
                             LockType& lock);

    Duration default_ttl_;
    size_t max_capacity_;
    Duration cleanup_interval_;
    TTLCacheConfig config_;
    EvictionCallback eviction_callback_;

    std::unordered_map<Key, CacheEntry, Hash, KeyEqual> cache_map_;
    std::list<Key> lru_list_;
    mutable SharedMutex mutex_;

    Atomic<bool> stop_cleanup_{false};
    std::thread cleanup_thread_;
    ConditionVariableAny cleanup_cv_;

    mutable Atomic<size_t> hits_{0};
    mutable Atomic<size_t> misses_{0};
    mutable Atomic<size_t> evictions_{0};
    mutable Atomic<size_t> expirations_{0};
};

// =============================================================================
// Implementation
// =============================================================================

template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::TTLCache(
    Duration ttl, size_t max_capacity, std::optional<Duration> cleanup_interval,
    TTLCacheConfig config, EvictionCallback eviction_callback)
    : default_ttl_(ttl),
      max_capacity_(max_capacity),
      cleanup_interval_(cleanup_interval.value_or(Duration(ttl.count() / 2))),
      config_(std::move(config)),
      eviction_callback_(std::move(eviction_callback)) {
    if (ttl.count() <= 0) {
        throw TTLCacheException("TTL must be positive");
    }
    if (max_capacity == 0) {
        throw TTLCacheException("Max capacity must be greater than zero");
    }

    if (config_.enable_automatic_cleanup) {
        cleanup_thread_ = std::thread([this] { cleanup_thread_func(); });
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::~TTLCache() noexcept {
    stop_cleanup_.store(true);
    cleanup_cv_.notify_all();
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::TTLCache(TTLCache&& other) noexcept
    : default_ttl_(other.default_ttl_),
      max_capacity_(other.max_capacity_),
      cleanup_interval_(other.cleanup_interval_),
      config_(std::move(other.config_)),
      eviction_callback_(std::move(other.eviction_callback_)),
      cache_map_(std::move(other.cache_map_)),
      lru_list_(std::move(other.lru_list_)) {
    other.stop_cleanup_.store(true);
    other.cleanup_cv_.notify_all();
    if (other.cleanup_thread_.joinable()) {
        other.cleanup_thread_.join();
    }

    if (config_.enable_automatic_cleanup) {
        cleanup_thread_ = std::thread([this] { cleanup_thread_func(); });
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>&
TTLCache<Key, Value, Hash, KeyEqual>::operator=(TTLCache&& other) noexcept {
    if (this != &other) {
        stop_cleanup_.store(true);
        cleanup_cv_.notify_all();
        if (cleanup_thread_.joinable()) {
            cleanup_thread_.join();
        }

        default_ttl_ = other.default_ttl_;
        max_capacity_ = other.max_capacity_;
        cleanup_interval_ = other.cleanup_interval_;
        config_ = std::move(other.config_);
        eviction_callback_ = std::move(other.eviction_callback_);
        cache_map_ = std::move(other.cache_map_);
        lru_list_ = std::move(other.lru_list_);

        other.stop_cleanup_.store(true);
        other.cleanup_cv_.notify_all();
        if (other.cleanup_thread_.joinable()) {
            other.cleanup_thread_.join();
        }

        stop_cleanup_.store(false);
        if (config_.enable_automatic_cleanup) {
            cleanup_thread_ = std::thread([this] { cleanup_thread_func(); });
        }
    }
    return *this;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::put(
    const Key& key, const Value& value, std::optional<Duration> custom_ttl) {
    UniqueLock<SharedMutex> lock(mutex_);

    auto it = cache_map_.find(key);
    if (it != cache_map_.end()) {
        // Update existing
        it->second.value = std::make_shared<Value>(value);
        it->second.expiry_time =
            Clock::now() + custom_ttl.value_or(default_ttl_);
        it->second.last_access = Clock::now();
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second.lru_iterator);
    } else {
        // Insert new
        if (cache_map_.size() >= max_capacity_) {
            evict_items(lock, 1);
        }

        lru_list_.push_front(key);
        CacheEntry entry;
        entry.value = std::make_shared<Value>(value);
        entry.expiry_time = Clock::now() + custom_ttl.value_or(default_ttl_);
        entry.last_access = Clock::now();
        entry.lru_iterator = lru_list_.begin();
        cache_map_[key] = std::move(entry);
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::put(
    const Key& key, Value&& value, std::optional<Duration> custom_ttl) {
    UniqueLock<SharedMutex> lock(mutex_);

    auto it = cache_map_.find(key);
    if (it != cache_map_.end()) {
        it->second.value = std::make_shared<Value>(std::move(value));
        it->second.expiry_time =
            Clock::now() + custom_ttl.value_or(default_ttl_);
        it->second.last_access = Clock::now();
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second.lru_iterator);
    } else {
        if (cache_map_.size() >= max_capacity_) {
            evict_items(lock, 1);
        }

        lru_list_.push_front(key);
        CacheEntry entry;
        entry.value = std::make_shared<Value>(std::move(value));
        entry.expiry_time = Clock::now() + custom_ttl.value_or(default_ttl_);
        entry.last_access = Clock::now();
        entry.lru_iterator = lru_list_.begin();
        cache_map_[key] = std::move(entry);
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
template <typename... Args>
void TTLCache<Key, Value, Hash, KeyEqual>::emplace(
    const Key& key, std::optional<Duration> custom_ttl, Args&&... args) {
    UniqueLock<SharedMutex> lock(mutex_);

    if (cache_map_.size() >= max_capacity_) {
        evict_items(lock, 1);
    }

    lru_list_.push_front(key);
    CacheEntry entry;
    entry.value = std::make_shared<Value>(std::forward<Args>(args)...);
    entry.expiry_time = Clock::now() + custom_ttl.value_or(default_ttl_);
    entry.last_access = Clock::now();
    entry.lru_iterator = lru_list_.begin();
    cache_map_[key] = std::move(entry);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::batch_put(
    const std::vector<std::pair<Key, Value>>& items,
    std::optional<Duration> custom_ttl) {
    for (const auto& [key, value] : items) {
        put(key, value, custom_ttl);
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::optional<Value> TTLCache<Key, Value, Hash, KeyEqual>::get(
    const Key& key, bool update_access_time) {
    UniqueLock<SharedMutex> lock(mutex_);
    return get_impl(key, update_access_time, lock);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
typename TTLCache<Key, Value, Hash, KeyEqual>::ValuePtr
TTLCache<Key, Value, Hash, KeyEqual>::get_shared(const Key& key,
                                                 bool update_access_time) {
    UniqueLock<SharedMutex> lock(mutex_);
    return get_shared_impl(key, update_access_time, lock);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
typename TTLCache<Key, Value, Hash, KeyEqual>::ValueContainer
TTLCache<Key, Value, Hash, KeyEqual>::batch_get(const KeyContainer& keys,
                                                bool update_access_time) {
    ValueContainer results;
    results.reserve(keys.size());

    UniqueLock<SharedMutex> lock(mutex_);
    for (const auto& key : keys) {
        results.push_back(get_impl(key, update_access_time, lock));
    }

    return results;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
template <typename Factory>
Value TTLCache<Key, Value, Hash, KeyEqual>::get_or_compute(
    const Key& key, Factory&& factory, std::optional<Duration> custom_ttl) {
    {
        SharedLock<SharedMutex> lock(mutex_);
        auto it = cache_map_.find(key);
        if (it != cache_map_.end() && Clock::now() < it->second.expiry_time) {
            if (config_.enable_statistics) {
                hits_++;
            }
            return *it->second.value;
        }
    }

    // Compute and insert
    Value value = factory();
    put(key, value, custom_ttl);
    return value;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::remove(const Key& key) noexcept {
    try {
        UniqueLock<SharedMutex> lock(mutex_);

        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return false;
        }

        if (eviction_callback_ && it->second.value) {
            try {
                eviction_callback_(key, *it->second.value, false);
            } catch (...) {
            }
        }

        lru_list_.erase(it->second.lru_iterator);
        cache_map_.erase(it);
        return true;
    } catch (...) {
        return false;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
size_t TTLCache<Key, Value, Hash, KeyEqual>::batch_remove(
    const KeyContainer& keys) noexcept {
    size_t removed = 0;
    for (const auto& key : keys) {
        if (remove(key)) {
            removed++;
        }
    }
    return removed;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::contains(
    const Key& key) const noexcept {
    SharedLock<SharedMutex> lock(mutex_);
    auto it = cache_map_.find(key);
    return it != cache_map_.end() && Clock::now() < it->second.expiry_time;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::update_ttl(
    const Key& key, Duration new_ttl) noexcept {
    try {
        UniqueLock<SharedMutex> lock(mutex_);
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return false;
        }
        it->second.expiry_time = Clock::now() + new_ttl;
        return true;
    } catch (...) {
        return false;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::optional<Duration> TTLCache<Key, Value, Hash, KeyEqual>::get_remaining_ttl(
    const Key& key) const noexcept {
    SharedLock<SharedMutex> lock(mutex_);
    auto it = cache_map_.find(key);
    if (it == cache_map_.end()) {
        return std::nullopt;
    }
    auto remaining = it->second.expiry_time - Clock::now();
    if (remaining.count() < 0) {
        return Duration(0);
    }
    return std::chrono::duration_cast<Duration>(remaining);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
size_t TTLCache<Key, Value, Hash, KeyEqual>::size() const noexcept {
    SharedLock<SharedMutex> lock(mutex_);
    return cache_map_.size();
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
size_t TTLCache<Key, Value, Hash, KeyEqual>::capacity() const noexcept {
    return max_capacity_;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::empty() const noexcept {
    SharedLock<SharedMutex> lock(mutex_);
    return cache_map_.empty();
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::clear() noexcept {
    try {
        UniqueLock<SharedMutex> lock(mutex_);
        if (config_.enable_statistics) {
            evictions_ += cache_map_.size();
        }
        cache_map_.clear();
        lru_list_.clear();
    } catch (...) {
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::resize(size_t new_capacity) {
    UniqueLock<SharedMutex> lock(mutex_);
    max_capacity_ = new_capacity;
    while (cache_map_.size() > max_capacity_) {
        evict_items(lock, 1);
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
CacheStatistics TTLCache<Key, Value, Hash, KeyEqual>::get_statistics()
    const noexcept {
    CacheStatistics stats;
    stats.hits = hits_.load();
    stats.misses = misses_.load();
    stats.evictions = evictions_.load();
    stats.expirations = expirations_.load();
    stats.current_size = cache_map_.size();
    stats.max_capacity = max_capacity_;
    size_t total = stats.hits + stats.misses;
    stats.hit_rate = total > 0 ? static_cast<double>(stats.hits) / total : 0.0;
    return stats;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::reset_statistics() noexcept {
    if (config_.enable_statistics) {
        hits_.store(0);
        misses_.store(0);
        evictions_.store(0);
        expirations_.store(0);
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
double TTLCache<Key, Value, Hash, KeyEqual>::hit_rate() const noexcept {
    if (!config_.enable_statistics) {
        return 0.0;
    }
    size_t total = hits_.load() + misses_.load();
    return total > 0 ? static_cast<double>(hits_.load()) / total : 0.0;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::set_eviction_callback(
    EvictionCallback callback) {
    UniqueLock<SharedMutex> lock(mutex_);
    eviction_callback_ = std::move(callback);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::set_default_ttl(Duration ttl) {
    default_ttl_ = ttl;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::evict_items(
    UniqueLock<SharedMutex>& /*lock*/, size_t count) {
    for (size_t i = 0; i < count && !lru_list_.empty(); ++i) {
        const Key& key = lru_list_.back();
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            if (eviction_callback_ && it->second.value) {
                try {
                    eviction_callback_(key, *it->second.value, true);
                } catch (...) {
                }
            }
            if (config_.enable_statistics) {
                evictions_++;
            }
            cache_map_.erase(it);
        }
        lru_list_.pop_back();
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::cleanup_expired_items(
    UniqueLock<SharedMutex>& /*lock*/) {
    auto now = Clock::now();
    size_t batch_count = 0;

    for (auto it = cache_map_.begin();
         it != cache_map_.end() && batch_count < config_.cleanup_batch_size;) {
        if (now > it->second.expiry_time) {
            if (eviction_callback_ && it->second.value) {
                try {
                    eviction_callback_(it->first, *it->second.value, false);
                } catch (...) {
                }
            }
            if (config_.enable_statistics) {
                expirations_++;
            }
            lru_list_.erase(it->second.lru_iterator);
            it = cache_map_.erase(it);
            batch_count++;
        } else {
            ++it;
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::cleanup_thread_func() {
    while (!stop_cleanup_.load()) {
        {
            UniqueLock<SharedMutex> lock(mutex_);
            cleanup_cv_.wait_for(lock, cleanup_interval_,
                                 [this] { return stop_cleanup_.load(); });

            if (!stop_cleanup_.load()) {
                cleanup_expired_items(lock);
            }
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
template <typename LockType>
std::optional<Value> TTLCache<Key, Value, Hash, KeyEqual>::get_impl(
    const Key& key, bool update_access_time, LockType& /*lock*/) {
    auto it = cache_map_.find(key);
    if (it == cache_map_.end()) {
        if (config_.enable_statistics) {
            misses_++;
        }
        return std::nullopt;
    }

    if (Clock::now() > it->second.expiry_time) {
        if (config_.enable_statistics) {
            misses_++;
        }
        return std::nullopt;
    }

    if (config_.enable_statistics) {
        hits_++;
    }

    if (update_access_time) {
        it->second.last_access = Clock::now();
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second.lru_iterator);
    }

    return *it->second.value;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
template <typename LockType>
typename TTLCache<Key, Value, Hash, KeyEqual>::ValuePtr
TTLCache<Key, Value, Hash, KeyEqual>::get_shared_impl(const Key& key,
                                                      bool update_access_time,
                                                      LockType& /*lock*/) {
    auto it = cache_map_.find(key);
    if (it == cache_map_.end()) {
        if (config_.enable_statistics) {
            misses_++;
        }
        return nullptr;
    }

    if (Clock::now() > it->second.expiry_time) {
        if (config_.enable_statistics) {
            misses_++;
        }
        return nullptr;
    }

    if (config_.enable_statistics) {
        hits_++;
    }

    if (update_access_time) {
        it->second.last_access = Clock::now();
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second.lru_iterator);
    }

    return it->second.value;
}

}  // namespace atom::search::cache

// Expose in atom::search namespace for backward compatibility
namespace atom::search {
using cache::TTLCache;
using cache::TTLCacheConfig;
}  // namespace atom::search

#endif  // ATOM_SEARCH_CACHE_TTL_CACHE_HPP
