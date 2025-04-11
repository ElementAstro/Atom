#ifndef ATOM_SEARCH_TTL_CACHE_HPP
#define ATOM_SEARCH_TTL_CACHE_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

// Boost support
#if defined(ATOM_USE_BOOST_THREAD) || defined(ATOM_USE_BOOST_LOCKFREE)
  #include <boost/config.hpp>
#endif

#ifdef ATOM_USE_BOOST_THREAD
  #include <boost/thread.hpp>
  #include <boost/thread/mutex.hpp>
  #include <boost/thread/shared_mutex.hpp>
  #include <boost/thread/lock_types.hpp>
  #include <boost/thread/condition_variable.hpp>
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
 * @brief A Time-to-Live (TTL) Cache with a maximum capacity.
 *
 * This class implements a TTL cache with an LRU eviction policy. Items in the
 * cache expire after a specified duration and are evicted when the cache
 * exceeds its maximum capacity.
 *
 * @tparam Key The type of the cache keys.
 * @tparam Value The type of the cache values.
 */
template <typename Key, typename Value>
class TTLCache {
public:
    using Clock = std::chrono::steady_clock;  ///< Type alias for the clock used
                                              ///< for timestamps.
    using TimePoint =
        std::chrono::time_point<Clock>;  ///< Type alias for time points.
    using Duration = std::chrono::milliseconds;  ///< Type alias for durations.
    using ValuePtr =
        std::shared_ptr<Value>;  ///< Type alias for shared pointer to value.

    /**
     * @brief Constructs a TTLCache object with the given TTL and maximum
     * capacity.
     *
     * @param ttl Duration after which items expire and are removed from the
     * cache.
     * @param max_capacity Maximum number of items the cache can hold.
     * @param cleanup_interval Optional interval for cleanup operations
     * (defaults to ttl/2).
     * @throws TTLCacheException if ttl <= 0 or max_capacity == 0
     */
    TTLCache(Duration ttl, size_t max_capacity,
             std::optional<Duration> cleanup_interval = std::nullopt);

    /**
     * @brief Destroys the TTLCache object and stops the cleaner thread.
     */
    ~TTLCache() noexcept;

    /**
     * @brief Copy constructor is deleted to prevent accidental copies.
     */
    TTLCache(const TTLCache&) = delete;

    /**
     * @brief Copy assignment operator is deleted to prevent accidental copies.
     */
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
     * @brief Inserts a new key-value pair into the cache or updates an existing
     * key.
     *
     * @param key The key to insert or update.
     * @param value The value associated with the key.
     * @throws std::bad_alloc if memory allocation fails
     * @throws TTLCacheException for other internal errors
     */
    void put(const Key& key, const Value& value);

    /**
     * @brief Inserts a new key-value pair into the cache using move semantics.
     *
     * @param key The key to insert or update.
     * @param value The value to be moved into the cache.
     * @throws std::bad_alloc if memory allocation fails
     * @throws TTLCacheException for other internal errors
     */
    void put(const Key& key, Value&& value);

    /**
     * @brief Batch insertion of multiple key-value pairs.
     *
     * @param items Vector of key-value pairs to insert.
     * @throws std::bad_alloc if memory allocation fails
     * @throws TTLCacheException for other internal errors
     */
    void batch_put(const std::vector<std::pair<Key, Value>>& items);

    /**
     * @brief Retrieves the value associated with the given key from the cache.
     *
     * @param key The key whose associated value is to be retrieved.
     * @return An optional containing the value if found and not expired;
     * otherwise, std::nullopt.
     */
    [[nodiscard]] std::optional<Value> get(const Key& key);

    /**
     * @brief Retrieves the value as a shared pointer, avoiding copies for large
     * objects.
     *
     * @param key The key whose associated value is to be retrieved.
     * @return A shared pointer to the value if found and not expired;
     * otherwise, nullptr.
     */
    [[nodiscard]] ValuePtr get_shared(const Key& key);

    /**
     * @brief Batch retrieval of multiple values by keys.
     *
     * @param keys Vector of keys to retrieve.
     * @return Vector of optional values corresponding to the keys.
     */
    [[nodiscard]] std::vector<std::optional<Value>> batch_get(
        const std::vector<Key>& keys);

    /**
     * @brief Removes an item from the cache.
     *
     * @param key The key to remove.
     * @return true if the item was found and removed, false otherwise.
     */
    bool remove(const Key& key) noexcept;

    /**
     * @brief Checks if a key exists in the cache and has not expired.
     *
     * @param key The key to check.
     * @return true if the key exists and has not expired, false otherwise.
     */
    [[nodiscard]] bool contains(const Key& key) const noexcept;

    /**
     * @brief Performs cache cleanup by removing expired items.
     */
    void cleanup() noexcept;

    /**
     * @brief Manually trigger a cleanup operation.
     */
    void force_cleanup() noexcept;

    /**
     * @brief Gets the cache hit rate.
     *
     * @return The ratio of cache hits to total accesses.
     */
    [[nodiscard]] double hitRate() const noexcept;

    /**
     * @brief Gets the current number of items in the cache.
     *
     * @return The number of items in the cache.
     */
    [[nodiscard]] size_t size() const noexcept;

    /**
     * @brief Gets the maximum capacity of the cache.
     *
     * @return The maximum capacity of the cache.
     */
    [[nodiscard]] constexpr size_t capacity() const noexcept {
        return max_capacity_;
    }

    /**
     * @brief Gets the TTL duration of the cache.
     *
     * @return The TTL duration.
     */
    [[nodiscard]] constexpr Duration ttl() const noexcept { return ttl_; }

    /**
     * @brief Clears all items from the cache and resets hit/miss counts.
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

private:
    /**
     * @brief Structure representing a cache item.
     */
    struct CacheItem {
        Key key;  ///< The key of the cache item.
        std::shared_ptr<Value>
            value;  ///< The value of the cache item (shared ownership).
        TimePoint expiry_time;  ///< The expiration time of the cache item.

        CacheItem(const Key& k, const Value& v, const TimePoint& t);
        CacheItem(const Key& k, Value&& v, const TimePoint& t);
    };

    using CacheList = std::list<CacheItem>;  ///< Type alias for the list used
                                             ///< to store cache items.
    using CacheMap = std::unordered_map<
        Key, typename CacheList::iterator>;  ///< Type alias for the map.

    Duration ttl_;               ///< Duration after which cache items expire.
    Duration cleanup_interval_;  ///< Interval between cleanup operations.
    size_t max_capacity_;        ///< Maximum capacity of the cache.
    CacheList cache_list_;       ///< List of cache items, ordered by recency.
    CacheMap cache_map_;  ///< Map of cache keys to iterators in the list.

    mutable SharedMutex<std::shared_mutex>
        mutex_;  ///< Mutex for synchronizing access to cache data.
    Atomic<size_t> hit_count_{0};   ///< Number of cache hits.
    Atomic<size_t> miss_count_{0};  ///< Number of cache misses.

    Thread
        cleaner_thread_;  ///< Background thread for cleaning up expired items.
    Atomic<bool> stop_{
        false};  ///< Flag to signal the cleaner thread to stop.
    CondVarAny
        cv_;  ///< Condition variable used to wake up the cleaner thread.

    /**
     * @brief The task run by the cleaner thread to periodically clean up
     * expired items.
     */
    void cleanerTask() noexcept;

    /**
     * @brief Helper method to evict items when capacity is exceeded.
     *
     * @param lock An already acquired unique lock.
     * @param count Number of items to evict (default: 1).
     */
    void evictItems(UniqueLock<std::shared_mutex>& lock,
                    size_t count = 1) noexcept;

    /**
     * @brief Helper method to check if an item is expired.
     *
     * @param expiry_time The expiry time to check.
     * @return true if expired, false otherwise.
     */
    [[nodiscard]] inline bool isExpired(
        const TimePoint& expiry_time) const noexcept {
        return expiry_time <= Clock::now();
    }
};

template <typename Key, typename Value>
TTLCache<Key, Value>::TTLCache(Duration ttl, size_t max_capacity,
                               std::optional<Duration> cleanup_interval)
    : ttl_(ttl),
      cleanup_interval_(cleanup_interval.value_or(ttl / 2)),
      max_capacity_(max_capacity) {
    // Input validation
    if (ttl <= Duration::zero()) {
        throw TTLCacheException("TTL must be greater than zero");
    }

    if (max_capacity == 0) {
        throw TTLCacheException("Maximum capacity must be greater than zero");
    }

    try {
        cleaner_thread_ = Thread([this] { this->cleanerTask(); });
    } catch (const std::exception& e) {
        throw TTLCacheException(
            std::string("Failed to create cleaner thread: ") + e.what());
    }
}

template <typename Key, typename Value>
TTLCache<Key, Value>::~TTLCache() noexcept {
    try {
        stop_ = true;
        cv_.notify_all();
        if (cleaner_thread_.joinable()) {
            cleaner_thread_.join();
        }
    } catch (...) {
        // Ensure no exceptions escape the destructor
    }
}

template <typename Key, typename Value>
TTLCache<Key, Value>::TTLCache(TTLCache&& other) noexcept
    : ttl_(other.ttl_),
      cleanup_interval_(other.cleanup_interval_),
      max_capacity_(other.max_capacity_),
      hit_count_(other.hit_count_.load()),
      miss_count_(other.miss_count_.load()) {
    // Lock the other cache to safely move its content
    UniqueLock lock(other.mutex_);

    // Move containers
    cache_list_ = std::move(other.cache_list_);
    cache_map_ = std::move(other.cache_map_);

    // Set up the cleaner thread
    other.stop_ = true;
    other.cv_.notify_all();

    if (other.cleaner_thread_.joinable()) {
        other.cleaner_thread_.join();
    }

    stop_ = false;
    cleaner_thread_ = Thread([this] { this->cleanerTask(); });
}

template <typename Key, typename Value>
TTLCache<Key, Value>& TTLCache<Key, Value>::operator=(
    TTLCache&& other) noexcept {
    if (this != &other) {
        // Stop our cleaner thread
        stop_ = true;
        cv_.notify_all();
        if (cleaner_thread_.joinable()) {
            cleaner_thread_.join();
        }

        // Lock both caches
        UniqueLock lock1(mutex_, std::defer_lock);
        UniqueLock lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);

        // Move data
        ttl_ = other.ttl_;
        cleanup_interval_ = other.cleanup_interval_;
        max_capacity_ = other.max_capacity_;
        cache_list_ = std::move(other.cache_list_);
        cache_map_ = std::move(other.cache_map_);
        hit_count_ = other.hit_count_.load();
        miss_count_ = other.miss_count_.load();

        // Stop other's cleaner thread
        other.stop_ = true;
        other.cv_.notify_all();
        if (other.cleaner_thread_.joinable()) {
            other.cleaner_thread_.join();
        }

        // Start our cleaner thread
        stop_ = false;
        cleaner_thread_ = Thread([this] { this->cleanerTask(); });
    }
    return *this;
}

template <typename Key, typename Value>
void TTLCache<Key, Value>::put(const Key& key, const Value& value) {
    try {
        UniqueLock lock(mutex_);

        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Update existing item
            cache_list_.erase(it->second);
            cache_map_.erase(it);
        } else if (cache_list_.size() >= max_capacity_) {
            // Remove least recently used item when capacity is reached
            evictItems(lock);
        }

        // Create new item with expiry time
        auto expiry_time = Clock::now() + ttl_;
        cache_list_.emplace_front(key, value, expiry_time);
        cache_map_[key] = cache_list_.begin();

    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    } catch (const std::exception& e) {
        throw TTLCacheException(std::string("Error putting item in cache: ") +
                                e.what());
    }
}

template <typename Key, typename Value>
void TTLCache<Key, Value>::put(const Key& key, Value&& value) {
    try {
        UniqueLock lock(mutex_);

        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Update existing item
            cache_list_.erase(it->second);
            cache_map_.erase(it);
        } else if (cache_list_.size() >= max_capacity_) {
            // Remove least recently used item when capacity is reached
            evictItems(lock);
        }

        // Create new item with expiry time
        auto expiry_time = Clock::now() + ttl_;
        cache_list_.emplace_front(key, std::move(value), expiry_time);
        cache_map_[key] = cache_list_.begin();

    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    } catch (const std::exception& e) {
        throw TTLCacheException(std::string("Error putting item in cache: ") +
                                e.what());
    }
}

template <typename Key, typename Value>
void TTLCache<Key, Value>::batch_put(
    const std::vector<std::pair<Key, Value>>& items) {
    if (items.empty()) {
        return;
    }

    try {
        UniqueLock lock(mutex_);
        auto now = Clock::now();

        // Reserve space if possible
        if (std::is_integral_v<Key>) {
            cache_map_.reserve(
                std::min(cache_map_.size() + items.size(), max_capacity_));
        }

        for (const auto& [key, value] : items) {
            auto it = cache_map_.find(key);
            if (it != cache_map_.end()) {
                // Update existing item
                *(it->second->value) = value;
                it->second->expiry_time = now + ttl_;

                // Move to front (MRU position)
                cache_list_.splice(cache_list_.begin(), cache_list_,
                                   it->second);
            } else {
                // Evict if necessary
                if (cache_list_.size() >= max_capacity_) {
                    evictItems(lock);
                }

                // Insert new item
                cache_list_.emplace_front(key, value, now + ttl_);
                cache_map_[key] = cache_list_.begin();
            }
        }
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    } catch (const std::exception& e) {
        throw TTLCacheException(
            std::string("Error batch putting items in cache: ") + e.what());
    }
}

template <typename Key, typename Value>
std::optional<Value> TTLCache<Key, Value>::get(const Key& key) {
    try {
        SharedLock lock(mutex_);
        auto now = Clock::now();

        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Check if the item has expired
            if (it->second->expiry_time > now) {
                hit_count_++;
                return *(it->second->value);
            }
            // Item exists but has expired - count as miss
        }

        miss_count_++;
        return std::nullopt;
    } catch (const std::exception& e) {
        // Log error but return nullopt instead of throwing
        miss_count_++;
        return std::nullopt;
    }
}

template <typename Key, typename Value>
typename TTLCache<Key, Value>::ValuePtr TTLCache<Key, Value>::get_shared(
    const Key& key) {
    try {
        SharedLock lock(mutex_);
        auto now = Clock::now();

        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Check if the item has expired
            if (it->second->expiry_time > now) {
                hit_count_++;
                return it->second->value;
            }
            // Item exists but has expired - count as miss
        }

        miss_count_++;
        return nullptr;
    } catch (const std::exception& e) {
        // Log error but return nullptr instead of throwing
        miss_count_++;
        return nullptr;
    }
}

template <typename Key, typename Value>
std::vector<std::optional<Value>> TTLCache<Key, Value>::batch_get(
    const std::vector<Key>& keys) {
    if (keys.empty()) {
        return {};
    }

    std::vector<std::optional<Value>> results;
    results.reserve(keys.size());

    try {
        SharedLock lock(mutex_);
        auto now = Clock::now();

        for (const auto& key : keys) {
            auto it = cache_map_.find(key);
            if (it != cache_map_.end() && it->second->expiry_time > now) {
                hit_count_++;
                results.emplace_back(*(it->second->value));
            } else {
                miss_count_++;
                results.emplace_back(std::nullopt);
            }
        }
    } catch (const std::exception& e) {
        // Complete the results vector with nullopt for any remaining keys
        while (results.size() < keys.size()) {
            miss_count_++;
            results.emplace_back(std::nullopt);
        }
    }

    return results;
}

template <typename Key, typename Value>
bool TTLCache<Key, Value>::remove(const Key& key) noexcept {
    try {
        UniqueLock lock(mutex_);

        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            cache_list_.erase(it->second);
            cache_map_.erase(it);
            return true;
        }
        return false;
    } catch (...) {
        // Ensure no exceptions escape
        return false;
    }
}

template <typename Key, typename Value>
bool TTLCache<Key, Value>::contains(const Key& key) const noexcept {
    try {
        SharedLock lock(mutex_);
        auto now = Clock::now();

        auto it = cache_map_.find(key);
        return (it != cache_map_.end() && it->second->expiry_time > now);
    } catch (...) {
        return false;
    }
}

template <typename Key, typename Value>
void TTLCache<Key, Value>::cleanup() noexcept {
    try {
        UniqueLock lock(mutex_);
        auto now = Clock::now();

        // Use reverse iterator to efficiently remove items from the end
        auto it = cache_list_.begin();
        while (it != cache_list_.end()) {
            if (it->expiry_time <= now) {
                auto key = it->key;
                it = cache_list_.erase(it);
                cache_map_.erase(key);
            } else {
                ++it;
            }
        }
    } catch (...) {
        // Ensure no exceptions escape
    }
}

template <typename Key, typename Value>
void TTLCache<Key, Value>::force_cleanup() noexcept {
    cleanup();
    cv_.notify_one();
}

template <typename Key, typename Value>
double TTLCache<Key, Value>::hitRate() const noexcept {
    size_t hits = hit_count_.load();
    size_t misses = miss_count_.load();
    size_t total = hits + misses;

    return total > 0 ? static_cast<double>(hits) / total : 0.0;
}

template <typename Key, typename Value>
size_t TTLCache<Key, Value>::size() const noexcept {
    try {
        SharedLock lock(mutex_);
        return cache_map_.size();
    } catch (...) {
        return 0;
    }
}

template <typename Key, typename Value>
void TTLCache<Key, Value>::clear() noexcept {
    try {
        UniqueLock lock(mutex_);
        cache_list_.clear();
        cache_map_.clear();
        hit_count_ = 0;
        miss_count_ = 0;
    } catch (...) {
        // Ensure no exceptions escape
    }
}

template <typename Key, typename Value>
void TTLCache<Key, Value>::resize(size_t new_capacity) {
    if (new_capacity == 0) {
        throw TTLCacheException("New capacity must be greater than zero");
    }

    try {
        UniqueLock lock(mutex_);

        max_capacity_ = new_capacity;

        // If current size exceeds new capacity, evict excess items
        if (cache_list_.size() > max_capacity_) {
            size_t excess = cache_list_.size() - max_capacity_;
            evictItems(lock, excess);
        }
    } catch (const TTLCacheException&) {
        throw;
    } catch (const std::exception& e) {
        throw TTLCacheException(std::string("Error resizing cache: ") +
                                e.what());
    }
}

template <typename Key, typename Value>
TTLCache<Key, Value>::CacheItem::CacheItem(const Key& k, const Value& v,
                                           const TimePoint& t)
    : key(k), value(std::make_shared<Value>(v)), expiry_time(t) {}

template <typename Key, typename Value>
TTLCache<Key, Value>::CacheItem::CacheItem(const Key& k, Value&& v,
                                           const TimePoint& t)
    : key(k), value(std::make_shared<Value>(std::move(v))), expiry_time(t) {}

template <typename Key, typename Value>
void TTLCache<Key, Value>::cleanerTask() noexcept {
    while (!stop_) {
        try {
            SharedLock lock(mutex_);
            cv_.wait_for(lock, cleanup_interval_,
                         [this] { return stop_.load(); });

            if (stop_) {
                break;
            }

            lock.unlock();
            cleanup();

        } catch (...) {
            // Ensure no exceptions escape the cleaner task
            std::this_thread::sleep_for(cleanup_interval_);
        }
    }
}

template <typename Key, typename Value>
void TTLCache<Key, Value>::evictItems(
    [[maybe_unused]] UniqueLock<std::shared_mutex>& lock,
    size_t count) noexcept {
    try {
        // First, try to remove expired items
        auto now = Clock::now();
        auto it = cache_list_.rbegin();  // Start from the least recently used

        while (count > 0 && it != cache_list_.rend()) {
            if (it->expiry_time <= now) {
                auto key = it->key;
                auto list_it = std::next(it).base();
                --it;  // Move reverse iterator before erasing

                cache_list_.erase(list_it);
                cache_map_.erase(key);
                --count;
            } else {
                ++it;
            }
        }

        // If we still need to evict more items, remove by LRU order
        while (count > 0 && !cache_list_.empty()) {
            auto last = cache_list_.back();
            cache_map_.erase(last.key);
            cache_list_.pop_back();
            --count;
        }
    } catch (...) {
        // Ensure no exceptions escape
    }
}

}  // namespace atom::search

#endif  // ATOM_SEARCH_TTL_CACHE_HPP