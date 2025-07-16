#ifndef ATOM_SEARCH_TTL_CACHE_HPP
#define ATOM_SEARCH_TTL_CACHE_HPP

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace atom::search {

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
    std::atomic<size_t> hits{0};
    std::atomic<size_t> misses{0};
    std::atomic<size_t> evictions{0};
    std::atomic<size_t> expirations{0};
    size_t current_size{0};
    size_t max_capacity{0};
    double hit_rate{0.0};
};

/**
 * @brief Configuration options for TTL Cache behavior.
 */
struct CacheConfig {
    bool enable_automatic_cleanup{true};
    bool enable_statistics{true};
    bool thread_safe{true};
    size_t cleanup_batch_size{100};
};

/**
 * @brief A high-performance, thread-safe Time-to-Live (TTL) Cache with an
 * LRU eviction policy.
 *
 * This implementation uses a sharded, lock-based approach to achieve high
 * concurrency and scalability on multi-core architectures. It is designed for
 * minimal contention and high throughput by partitioning the cache space and
 * using per-shard locks.
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
    TTLCache(TTLCache&&) = delete;
    TTLCache& operator=(TTLCache&&) = delete;

    /**
     * @brief Inserts or updates a key-value pair in the cache.
     *
     * @param key The key to insert or update.
     * @param value The value associated with the key.
     * @param custom_ttl Optional custom TTL for this specific item.
     */
    void put(const Key& key, const Value& value,
             std::optional<Duration> custom_ttl = std::nullopt);

    /**
     * @brief Inserts or updates a key-value pair using move semantics.
     *
     * @param key The key to insert or update.
     * @param value The value to be moved into the cache.
     * @param custom_ttl Optional custom TTL for this specific item.
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
     */
    template <typename... Args>
    void emplace(const Key& key, std::optional<Duration> custom_ttl,
                 Args&&... args);

    /**
     * @brief Batch insertion of multiple key-value pairs.
     *
     * @param items Vector of key-value pairs to insert.
     * @param custom_ttl Optional custom TTL for all items in the batch.
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
     * @param new_capacity The new maximum capacity.
     * @throws TTLCacheException if new_capacity == 0
     */
    void resize(size_t new_capacity);

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
                  const TimePoint& access)
            : key(k),
              value(std::make_shared<Value>(v)),
              expiry_time(expiry),
              access_time(access) {}
        CacheItem(const Key& k, Value&& v, const TimePoint& expiry,
                  const TimePoint& access)
            : key(k),
              value(std::make_shared<Value>(std::move(v))),
              expiry_time(expiry),
              access_time(access) {}
        template <typename... Args>
        CacheItem(const Key& k, const TimePoint& expiry,
                  const TimePoint& access, Args&&... args)
            : key(k),
              value(std::make_shared<Value>(std::forward<Args>(args)...)),
              expiry_time(expiry),
              access_time(access) {}
    };

    using CacheList = std::list<CacheItem>;
    using CacheMap =
        std::unordered_map<Key, typename CacheList::iterator, Hash, KeyEqual>;

    struct Shard {
        explicit Shard(size_t capacity) : max_capacity(capacity) {}
        CacheList list;
        CacheMap map;
        mutable std::shared_mutex mutex;
        size_t max_capacity;
    };

    Shard& get_shard(const Key& key) const;

    template <typename V>
    void put_impl(const Key& key, V&& value,
                  std::optional<Duration> custom_ttl);

    void move_to_front(Shard& shard, typename CacheList::iterator item);
    void evict_items(Shard& shard, size_t count) noexcept;
    void cleanup_expired_items(Shard& shard) noexcept;
    void notify_eviction(const Key& key, const Value& value,
                         bool expired) noexcept;
    [[nodiscard]] inline bool is_expired(
        const TimePoint& expiry_time) const noexcept;
    void cleaner_task() noexcept;
    void cleanup() noexcept;

    Duration ttl_;
    Duration cleanup_interval_;
    std::atomic<size_t> max_capacity_;
    CacheConfig config_;
    EvictionCallback eviction_callback_;

    std::vector<std::unique_ptr<Shard>> shards_;
    const size_t shard_mask_;

    std::atomic<size_t> current_size_{0};
    std::atomic<size_t> hit_count_{0};
    std::atomic<size_t> miss_count_{0};
    std::atomic<size_t> eviction_count_{0};
    std::atomic<size_t> expiration_count_{0};

    std::thread cleaner_thread_;
    std::atomic<bool> stop_flag_{false};
    std::mutex cleanup_mutex_;
    std::condition_variable cleanup_cv_;
};

template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::TTLCache(
    Duration ttl, size_t max_capacity, std::optional<Duration> cleanup_interval,
    CacheConfig config, EvictionCallback eviction_callback)
    : ttl_(ttl),
      cleanup_interval_(cleanup_interval.value_or(ttl / 2)),
      max_capacity_(max_capacity),
      config_(std::move(config)),
      eviction_callback_(std::move(eviction_callback)),
      shard_mask_([&] {
          size_t shard_count = 1;
          if (config_.thread_safe) {
              shard_count = std::thread::hardware_concurrency();
              if (shard_count == 0) shard_count = 4;
              size_t power = 1;
              while (power < shard_count) power <<= 1;
              shard_count = power;
          }
          return shard_count - 1;
      }()) {
    if (ttl <= Duration::zero()) {
        throw TTLCacheException("TTL must be greater than zero");
    }
    if (max_capacity == 0) {
        throw TTLCacheException("Maximum capacity must be greater than zero");
    }

    size_t shard_count = shard_mask_ + 1;
    shards_.reserve(shard_count);
    size_t per_shard_capacity = (max_capacity + shard_count - 1) / shard_count;
    for (size_t i = 0; i < shard_count; ++i) {
        shards_.emplace_back(std::make_unique<Shard>(per_shard_capacity));
    }

    if (config_.enable_automatic_cleanup) {
        try {
            cleaner_thread_ = std::thread([this] { cleaner_task(); });
        } catch (const std::exception& e) {
            spdlog::error("Failed to create cleaner thread: {}", e.what());
            throw TTLCacheException("Failed to create cleaner thread: " +
                                    std::string(e.what()));
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::~TTLCache() noexcept {
    stop_flag_ = true;
    cleanup_cv_.notify_all();
    if (cleaner_thread_.joinable()) {
        cleaner_thread_.join();
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
template <typename V>
void TTLCache<Key, Value, Hash, KeyEqual>::put_impl(
    const Key& key, V&& value, std::optional<Duration> custom_ttl) {
    try {
        auto& shard = get_shard(key);
        std::unique_lock lock(shard.mutex);
        auto now = Clock::now();
        auto expiry = now + custom_ttl.value_or(ttl_);

        auto it = shard.map.find(key);
        if (it != shard.map.end()) {
            it->second->value =
                std::make_shared<Value>(std::forward<V>(value));
            it->second->expiry_time = expiry;
            it->second->access_time = now;
            move_to_front(shard, it->second);
        } else {
            if (shard.map.size() >= shard.max_capacity) {
                evict_items(shard, 1);
            }
            shard.list.emplace_front(key, std::forward<V>(value), expiry, now);
            shard.map[key] = shard.list.begin();
            current_size_++;
        }
    } catch (const std::bad_alloc&) {
        spdlog::error("Memory allocation failed while putting item in cache.");
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Error putting item in cache: {}", e.what());
        throw TTLCacheException(std::string("Error putting item in cache: ") +
                                e.what());
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::put(
    const Key& key, const Value& value, std::optional<Duration> custom_ttl) {
    put_impl(key, value, custom_ttl);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::put(
    const Key& key, Value&& value, std::optional<Duration> custom_ttl) {
    put_impl(key, std::move(value), custom_ttl);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
template <typename... Args>
void TTLCache<Key, Value, Hash, KeyEqual>::emplace(
    const Key& key, std::optional<Duration> custom_ttl, Args&&... args) {
    try {
        auto& shard = get_shard(key);
        std::unique_lock lock(shard.mutex);
        auto now = Clock::now();
        auto expiry = now + custom_ttl.value_or(ttl_);

        if (shard.map.count(key)) {
            // In-place update not straightforward, fall back to remove and
            // insert
            auto it = shard.map.find(key);
            notify_eviction(it->second->key, *(it->second->value), false);
            shard.list.erase(it->second);
            shard.map.erase(it);
            current_size_--;
        }

        if (shard.map.size() >= shard.max_capacity) {
            evict_items(shard, 1);
        }

        shard.list.emplace_front(key, expiry, now, std::forward<Args>(args)...);
        shard.map[key] = shard.list.begin();
        current_size_++;
    } catch (const std::bad_alloc&) {
        spdlog::error("Memory allocation failed while emplacing item.");
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Error emplacing item in cache: {}", e.what());
        throw TTLCacheException(std::string("Error emplacing item in cache: ") +
                                e.what());
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::batch_put(
    const std::vector<std::pair<Key, Value>>& items,
    std::optional<Duration> custom_ttl) {
    if (items.empty()) return;
    try {
        auto ttl_to_use = custom_ttl.value_or(ttl_);
        std::vector<std::vector<std::pair<Key, Value>>> keys_by_shard(
            shards_.size());
        for (const auto& item : items) {
            keys_by_shard[std::hash<Key>{}(item.first) & shard_mask_].push_back(
                item);
        }

        for (size_t i = 0; i < shards_.size(); ++i) {
            if (keys_by_shard[i].empty()) continue;
            auto& shard = *shards_[i];
            std::unique_lock lock(shard.mutex);
            auto now = Clock::now();
            for (const auto& item : keys_by_shard[i]) {
                auto expiry = now + ttl_to_use;
                auto it = shard.map.find(item.first);
                if (it != shard.map.end()) {
                    it->second->value = std::make_shared<Value>(item.second);
                    it->second->expiry_time = expiry;
                    it->second->access_time = now;
                    move_to_front(shard, it->second);
                } else {
                    if (shard.map.size() >= shard.max_capacity) {
                        evict_items(shard, 1);
                    }
                    shard.list.emplace_front(item.first, item.second, expiry,
                                             now);
                    shard.map[item.first] = shard.list.begin();
                    current_size_++;
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error during batch put: {}", e.what());
        throw TTLCacheException(std::string("Error during batch put: ") +
                                e.what());
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::optional<Value> TTLCache<Key, Value, Hash, KeyEqual>::get(
    const Key& key, bool update_access_time) {
    auto shared_val = get_shared(key, update_access_time);
    return shared_val ? std::optional<Value>(*shared_val) : std::nullopt;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
auto TTLCache<Key, Value, Hash, KeyEqual>::get_shared(
    const Key& key, bool update_access_time) -> ValuePtr {
    try {
        auto& shard = get_shard(key);
        if (update_access_time) {
            std::unique_lock lock(shard.mutex);
            auto it = shard.map.find(key);
            if (it == shard.map.end() || is_expired(it->second->expiry_time)) {
                if (config_.enable_statistics) miss_count_++;
                return nullptr;
            }
            it->second->access_time = Clock::now();
            move_to_front(shard, it->second);
            if (config_.enable_statistics) hit_count_++;
            return it->second->value;
        } else {
            std::shared_lock lock(shard.mutex);
            auto it = shard.map.find(key);
            if (it == shard.map.end() || is_expired(it->second->expiry_time)) {
                if (config_.enable_statistics) miss_count_++;
                return nullptr;
            }
            if (config_.enable_statistics) hit_count_++;
            return it->second->value;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error getting item from cache: {}", e.what());
        if (config_.enable_statistics) miss_count_++;
        return nullptr;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
auto TTLCache<Key, Value, Hash, KeyEqual>::batch_get(
    const KeyContainer& keys, bool update_access_time) -> ValueContainer {
    if (keys.empty()) return {};

    ValueContainer results(keys.size());
    std::unordered_map<const Key*, size_t> key_to_idx;
    for (size_t i = 0; i < keys.size(); ++i) key_to_idx[&keys[i]] = i;

    std::vector<std::vector<const Key*>> keys_by_shard(shards_.size());
    for (const auto& key : keys) {
        keys_by_shard[std::hash<Key>{}(key) & shard_mask_].push_back(&key);
    }

    for (size_t i = 0; i < shards_.size(); ++i) {
        if (keys_by_shard[i].empty()) continue;
        auto& shard = *shards_[i];
        auto now = Clock::now();
        if (update_access_time) {
            std::unique_lock lock(shard.mutex);
            for (const Key* key_ptr : keys_by_shard[i]) {
                auto it = shard.map.find(*key_ptr);
                if (it != shard.map.end() &&
                    !is_expired(it->second->expiry_time)) {
                    it->second->access_time = now;
                    move_to_front(shard, it->second);
                    results[key_to_idx[key_ptr]] = *(it->second->value);
                    if (config_.enable_statistics) hit_count_++;
                } else {
                    if (config_.enable_statistics) miss_count_++;
                }
            }
        } else {
            std::shared_lock lock(shard.mutex);
            for (const Key* key_ptr : keys_by_shard[i]) {
                auto it = shard.map.find(*key_ptr);
                if (it != shard.map.end() &&
                    !is_expired(it->second->expiry_time)) {
                    results[key_to_idx[key_ptr]] = *(it->second->value);
                    if (config_.enable_statistics) hit_count_++;
                } else {
                    if (config_.enable_statistics) miss_count_++;
                }
            }
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
        auto& shard = get_shard(key);
        std::unique_lock lock(shard.mutex);
        auto it = shard.map.find(key);
        if (it != shard.map.end()) {
            notify_eviction(it->second->key, *(it->second->value), false);
            shard.list.erase(it->second);
            shard.map.erase(it);
            current_size_--;
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Error removing item from cache: {}", e.what());
        return false;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
size_t TTLCache<Key, Value, Hash, KeyEqual>::batch_remove(
    const KeyContainer& keys) noexcept {
    if (keys.empty()) return 0;
    size_t removed_count = 0;
    std::vector<std::vector<Key>> keys_by_shard(shards_.size());
    for (const auto& key : keys) {
        keys_by_shard[std::hash<Key>{}(key) & shard_mask_].push_back(key);
    }

    for (size_t i = 0; i < shards_.size(); ++i) {
        if (keys_by_shard[i].empty()) continue;
        auto& shard = *shards_[i];
        std::unique_lock lock(shard.mutex);
        for (const auto& key : keys_by_shard[i]) {
            auto it = shard.map.find(key);
            if (it != shard.map.end()) {
                notify_eviction(it->second->key, *(it->second->value), false);
                shard.list.erase(it->second);
                shard.map.erase(it);
                current_size_--;
                removed_count++;
            }
        }
    }
    return removed_count;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::contains(
    const Key& key) const noexcept {
    try {
        auto& shard = get_shard(key);
        std::shared_lock lock(shard.mutex);
        auto it = shard.map.find(key);
        return (it != shard.map.end() && !is_expired(it->second->expiry_time));
    } catch (const std::exception& e) {
        spdlog::error("Error in contains check: {}", e.what());
        return false;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::update_ttl(
    const Key& key, Duration new_ttl) noexcept {
    try {
        auto& shard = get_shard(key);
        std::unique_lock lock(shard.mutex);
        auto it = shard.map.find(key);
        if (it != shard.map.end() && !is_expired(it->second->expiry_time)) {
            it->second->expiry_time = Clock::now() + new_ttl;
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Error updating TTL: {}", e.what());
        return false;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::optional<typename TTLCache<Key, Value, Hash, KeyEqual>::Duration>
TTLCache<Key, Value, Hash, KeyEqual>::get_remaining_ttl(
    const Key& key) const noexcept {
    try {
        auto& shard = get_shard(key);
        std::shared_lock lock(shard.mutex);
        auto it = shard.map.find(key);
        if (it != shard.map.end()) {
            auto now = Clock::now();
            if (it->second->expiry_time > now) {
                return std::chrono::duration_cast<Duration>(
                    it->second->expiry_time - now);
            }
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        spdlog::error("Error getting remaining TTL: {}", e.what());
        return std::nullopt;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::force_cleanup() noexcept {
    cleanup();
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
CacheStatistics TTLCache<Key, Value, Hash, KeyEqual>::get_statistics()
    const noexcept {
    CacheStatistics stats;
    stats.hits = hit_count_.load();
    stats.misses = miss_count_.load();
    stats.evictions = eviction_count_.load();
    stats.expirations = expiration_count_.load();
    stats.current_size = current_size_.load();
    stats.max_capacity = max_capacity_.load();

    size_t total = stats.hits + stats.misses;
    stats.hit_rate =
        total > 0 ? static_cast<double>(stats.hits) / total : 0.0;
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
    if (!config_.enable_statistics) return 0.0;
    size_t hits = hit_count_.load();
    size_t misses = miss_count_.load();
    size_t total = hits + misses;
    return total > 0 ? static_cast<double>(hits) / total : 0.0;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
size_t TTLCache<Key, Value, Hash, KeyEqual>::size() const noexcept {
    return current_size_.load();
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::empty() const noexcept {
    return size() == 0;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
auto TTLCache<Key, Value, Hash, KeyEqual>::get_keys() const -> KeyContainer {
    KeyContainer all_keys;
    all_keys.reserve(size());
    for (const auto& shard_ptr : shards_) {
        std::shared_lock lock(shard_ptr->mutex);
        for (const auto& item : shard_ptr->list) {
            if (!is_expired(item.expiry_time)) {
                all_keys.push_back(item.key);
            }
        }
    }
    return all_keys;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::clear() noexcept {
    try {
        for (auto& shard_ptr : shards_) {
            std::unique_lock lock(shard_ptr->mutex);
            if (eviction_callback_) {
                for (const auto& item : shard_ptr->list) {
                    notify_eviction(item.key, *(item.value), false);
                }
            }
            shard_ptr->list.clear();
            shard_ptr->map.clear();
        }
        current_size_ = 0;
        reset_statistics();
    } catch (const std::exception& e) {
        spdlog::error("Error clearing cache: {}", e.what());
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::resize(size_t new_capacity) {
    if (new_capacity == 0) {
        throw TTLCacheException("New capacity must be greater than zero");
    }
    max_capacity_ = new_capacity;
    size_t per_shard_capacity =
        (new_capacity + shards_.size() - 1) / shards_.size();
    for (auto& shard_ptr : shards_) {
        std::unique_lock lock(shard_ptr->mutex);
        shard_ptr->max_capacity = per_shard_capacity;
        if (shard_ptr->map.size() > per_shard_capacity) {
            evict_items(*shard_ptr, shard_ptr->map.size() - per_shard_capacity);
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::set_eviction_callback(
    EvictionCallback callback) noexcept {
    std::lock_guard<std::mutex> lock(cleanup_mutex_);
    eviction_callback_ = std::move(callback);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::update_config(
    const CacheConfig& new_config) noexcept {
    std::lock_guard<std::mutex> lock(cleanup_mutex_);
    config_ = new_config;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
CacheConfig TTLCache<Key, Value, Hash, KeyEqual>::get_config() const noexcept {
    std::lock_guard<std::mutex> lock(
        const_cast<std::mutex&>(cleanup_mutex_));
    return config_;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
auto TTLCache<Key, Value, Hash, KeyEqual>::get_shard(const Key& key) const
    -> Shard& {
    return *shards_[std::hash<Key>{}(key) & shard_mask_];
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::move_to_front(
    Shard& shard, typename CacheList::iterator item) {
    if (item != shard.list.begin()) {
        shard.list.splice(shard.list.begin(), shard.list, item);
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::evict_items(Shard& shard,
                                                       size_t count) noexcept {
    for (size_t i = 0; i < count && !shard.list.empty(); ++i) {
        auto& last = shard.list.back();
        notify_eviction(last.key, *(last.value), false);
        shard.map.erase(last.key);
        shard.list.pop_back();
        current_size_--;
        if (config_.enable_statistics) eviction_count_++;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::cleanup_expired_items(
    Shard& shard) noexcept {
    auto now = Clock::now();
    size_t batch_count = 0;

    for (auto it = shard.list.begin();
         it != shard.list.end() && batch_count < config_.cleanup_batch_size;) {
        if (is_expired(it->expiry_time)) {
            notify_eviction(it->key, *(it->value), true);
            shard.map.erase(it->key);
            it = shard.list.erase(it);
            current_size_--;
            batch_count++;
            if (config_.enable_statistics) expiration_count_++;
        } else {
            ++it;
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::notify_eviction(
    const Key& key, const Value& value, bool expired) noexcept {
    try {
        if (eviction_callback_) {
            eviction_callback_(key, value, expired);
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception in eviction callback: {}", e.what());
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
inline bool TTLCache<Key, Value, Hash, KeyEqual>::is_expired(
    const TimePoint& expiry_time) const noexcept {
    return expiry_time <= Clock::now();
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::cleaner_task() noexcept {
    while (!stop_flag_) {
        try {
            std::unique_lock<std::mutex> lock(cleanup_mutex_);
            cleanup_cv_.wait_for(lock, cleanup_interval_,
                                 [this] { return stop_flag_.load(); });
            if (stop_flag_) break;
            lock.unlock();
            cleanup();
        } catch (const std::exception& e) {
            spdlog::error("Exception in cleaner task: {}", e.what());
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::cleanup() noexcept {
    for (auto& shard_ptr : shards_) {
        if (stop_flag_) return;
        try {
            std::unique_lock lock(shard_ptr->mutex);
            cleanup_expired_items(*shard_ptr);
        } catch (const std::exception& e) {
            spdlog::error("Error during shard cleanup: {}", e.what());
        }
    }
}

}  // namespace atom::search

#endif  // ATOM_SEARCH_TTL_CACHE_HPP