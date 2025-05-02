#ifndef THREADSAFE_LRU_CACHE_H
#define THREADSAFE_LRU_CACHE_H

#include <atomic>
#include <cassert>
#include <chrono>
#include <exception>
#include <fstream>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// Boost support
#if defined(ATOM_USE_BOOST_THREAD) || defined(ATOM_USE_BOOST_LOCKFREE)
#include <boost/config.hpp>
#endif

#ifdef ATOM_USE_BOOST_THREAD
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>
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

struct PairStringHash {
    size_t operator()(const std::pair<std::string, std::string>& p) const {
        size_t h1 = std::hash<std::string>()(p.first);
        size_t h2 = std::hash<std::string>()(p.second);
        return h1 ^ (h2 << 1);
    }
};

// Define aliases based on whether we're using Boost or STL
#if defined(ATOM_USE_BOOST_THREAD)
template <typename T>
using shared_mutex = boost::shared_mutex;

template <typename T>
using shared_lock = boost::shared_lock<T>;

template <typename T>
using unique_lock = boost::unique_lock<T>;

template <typename... Args>
using future = boost::future<Args...>;

template <typename... Args>
using promise = boost::promise<Args...>;
#else
template <typename T>
using shared_mutex = std::shared_mutex;

template <typename T>
using shared_lock = std::shared_lock<T>;

template <typename T>
using unique_lock = std::unique_lock<T>;

template <typename... Args>
using future = std::future<Args...>;

template <typename... Args>
using promise = std::promise<Args...>;
#endif

#if defined(ATOM_USE_BOOST_LOCKFREE)
template <typename T>
using atomic = boost::atomic<T>;

template <typename T>
struct lockfree_queue {
    boost::lockfree::queue<T> queue;

    lockfree_queue(size_t capacity) : queue(capacity) {}

    bool push(const T& item) { return queue.push(item); }

    bool pop(T& item) { return queue.pop(item); }

    bool empty() const { return queue.empty(); }
};
#else
template <typename T>
using atomic = std::atomic<T>;

// Fallback implementation using std containers when Boost.lockfree is not
// available
template <typename T>
struct lockfree_queue {
    std::mutex mutex;
    std::vector<T> items;
    size_t capacity;

    lockfree_queue(size_t capacity) : capacity(capacity) {
        items.reserve(capacity);
    }

    bool push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex);
        if (items.size() >= capacity) {
            return false;
        }
        items.push_back(item);
        return true;
    }

    bool pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex);
        if (items.empty()) {
            return false;
        }
        item = items.front();
        items.erase(items.begin());
        return true;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mutex);
        return items.empty();
    }
};
#endif

/**
 * @brief Custom exceptions for ThreadSafeLRUCache
 */
class LRUCacheException : public std::runtime_error {
public:
    explicit LRUCacheException(const std::string& message)
        : std::runtime_error(message) {}
};

class LRUCacheLockException : public LRUCacheException {
public:
    explicit LRUCacheLockException(const std::string& message)
        : LRUCacheException(message) {}
};

class LRUCacheIOException : public LRUCacheException {
public:
    explicit LRUCacheIOException(const std::string& message)
        : LRUCacheException(message) {}
};

/**
 * @brief A thread-safe LRU (Least Recently Used) cache implementation with
 * enhanced features.
 *
 * This class implements a highly-optimized LRU cache with thread safety using a
 * combination of a doubly-linked list and an unordered map. It supports adding,
 * retrieving, and removing cache items, as well as persisting cache contents to
 * and loading from a file.
 *
 * @tparam Key Type of the cache keys.
 * @tparam Value Type of the cache values.
 */
template <typename Key, typename Value>
class ThreadSafeLRUCache {
public:
    using KeyValuePair =
        std::pair<Key, Value>;  ///< Type alias for a key-value pair
    using ListIterator =
        typename std::list<KeyValuePair>::iterator;  ///< Iterator type for the
                                                     ///< list
    using Clock =
        std::chrono::steady_clock;  ///< Clock type for timing operations
    using TimePoint =
        std::chrono::time_point<Clock>;  ///< Time point type for expiry times
    using ValuePtr = std::shared_ptr<Value>;  ///< Smart pointer for Value type
    using BatchKeyType =
        std::vector<Key>;  ///< Type for batch operations with keys
    using BatchValueType =
        std::vector<ValuePtr>;  ///< Type for batch operation results

    struct CacheItem {
        ValuePtr value;
        TimePoint expiryTime;
        ListIterator iterator;
    };

    struct CacheStatistics {
        size_t hitCount;
        size_t missCount;
        float hitRate;
        size_t size;
        size_t maxSize;
        float loadFactor;
    };

    /**
     * @brief Constructs a ThreadSafeLRUCache with a specified maximum size.
     *
     * @param max_size The maximum number of items that the cache can hold.
     * @throws std::invalid_argument if max_size is zero
     */
    explicit ThreadSafeLRUCache(size_t max_size);

    /**
     * @brief Retrieves a value from the cache.
     *
     * Moves the accessed item to the front of the cache, indicating it was
     * recently used.
     *
     * @param key The key of the item to retrieve.
     * @return An optional containing the value if found and not expired,
     * otherwise std::nullopt.
     * @throws LRUCacheLockException if a deadlock is detected
     */
    [[nodiscard]] auto get(const Key& key) -> std::optional<Value>;

    /**
     * @brief Retrieves a value as a shared pointer from the cache.
     *
     * @param key The key of the item to retrieve.
     * @return A shared pointer to the value if found and not expired, otherwise
     * nullptr.
     * @throws LRUCacheLockException if a deadlock is detected
     */
    [[nodiscard]] auto getShared(const Key& key) noexcept -> ValuePtr;

    /**
     * @brief Batch retrieval of multiple values from the cache.
     *
     * @param keys Vector of keys to retrieve.
     * @return Vector of shared pointers to values (nullptr for missing items).
     */
    [[nodiscard]] auto getBatch(const BatchKeyType& keys) noexcept
        -> BatchValueType;

    /**
     * @brief Checks if a key exists in the cache.
     *
     * @param key The key to check.
     * @return True if the key exists and is not expired, false otherwise.
     */
    [[nodiscard]] bool contains(const Key& key) const noexcept;

    /**
     * @brief Inserts or updates a value in the cache.
     *
     * If the cache is full, the least recently used item is removed.
     *
     * @param key The key of the item to insert or update.
     * @param value The value to associate with the key.
     * @param ttl Optional time-to-live duration for the cache item.
     * @throws std::bad_alloc if memory allocation fails
     */
    void put(const Key& key, Value value,
             std::optional<std::chrono::seconds> ttl = std::nullopt);

    /**
     * @brief Inserts or updates a batch of values in the cache.
     *
     * @param items Vector of key-value pairs to insert.
     * @param ttl Optional time-to-live duration for all cache items.
     */
    void putBatch(const std::vector<KeyValuePair>& items,
                  std::optional<std::chrono::seconds> ttl = std::nullopt);

    /**
     * @brief Erases an item from the cache.
     *
     * @param key The key of the item to remove.
     * @return True if the item was found and removed, false otherwise.
     */
    bool erase(const Key& key) noexcept;

    /**
     * @brief Clears all items from the cache.
     */
    void clear() noexcept;

    /**
     * @brief Retrieves all keys in the cache.
     *
     * @return A vector containing all keys currently in the cache.
     */
    [[nodiscard]] auto keys() const -> std::vector<Key>;

    /**
     * @brief Removes and returns the least recently used item.
     *
     * @return An optional containing the key-value pair if the cache is not
     * empty, otherwise std::nullopt.
     */
    [[nodiscard]] auto popLru() noexcept -> std::optional<KeyValuePair>;

    /**
     * @brief Resizes the cache to a new maximum size.
     *
     * If the new size is smaller, the least recently used items are removed
     * until the cache size fits.
     *
     * @param new_max_size The new maximum size of the cache.
     * @throws std::invalid_argument if new_max_size is zero
     */
    void resize(size_t new_max_size);

    /**
     * @brief Gets the current size of the cache.
     *
     * @return The number of items currently in the cache.
     */
    [[nodiscard]] auto size() const noexcept -> size_t;

    /**
     * @brief Gets the maximum size of the cache.
     *
     * @return The maximum number of items the cache can hold.
     */
    [[nodiscard]] auto maxSize() const noexcept -> size_t;

    /**
     * @brief Gets the current load factor of the cache.
     *
     * The load factor is the ratio of the current size to the maximum size.
     *
     * @return The load factor of the cache.
     */
    [[nodiscard]] auto loadFactor() const noexcept -> float;

    /**
     * @brief Sets the callback function to be called when a new item is
     * inserted.
     *
     * @param callback The callback function that takes a key and value.
     */
    void setInsertCallback(
        std::function<void(const Key&, const Value&)> callback);

    /**
     * @brief Sets the callback function to be called when an item is erased.
     *
     * @param callback The callback function that takes a key.
     */
    void setEraseCallback(std::function<void(const Key&)> callback);

    /**
     * @brief Sets the callback function to be called when the cache is cleared.
     *
     * @param callback The callback function.
     */
    void setClearCallback(std::function<void()> callback);

    /**
     * @brief Gets the hit rate of the cache.
     *
     * The hit rate is the ratio of cache hits to the total number of cache
     * accesses.
     *
     * @return The hit rate of the cache.
     */
    [[nodiscard]] auto hitRate() const noexcept -> float;

    /**
     * @brief Gets comprehensive statistics about the cache.
     *
     * @return A CacheStatistics struct containing various metrics.
     */
    [[nodiscard]] auto getStatistics() const noexcept -> CacheStatistics;

    /**
     * @brief Saves the cache contents to a file.
     *
     * @param filename The name of the file to save to.
     * @throws LRUCacheLockException If a deadlock is avoided while locking.
     * @throws LRUCacheIOException If file operations fail.
     */
    void saveToFile(const std::string& filename) const;

    /**
     * @brief Loads cache contents from a file.
     *
     * @param filename The name of the file to load from.
     * @throws LRUCacheLockException If a deadlock is avoided while locking.
     * @throws LRUCacheIOException If file operations fail.
     */
    void loadFromFile(const std::string& filename);

    /**
     * @brief Prune expired items from the cache.
     *
     * @return Number of items pruned.
     */
    size_t pruneExpired() noexcept;

    /**
     * @brief Prefetch keys into the cache to improve hit rate.
     *
     * @param keys Vector of keys to prefetch.
     * @param loader Function to load values for keys not in cache.
     * @param ttl Optional time-to-live for prefetched items.
     * @return Number of items successfully prefetched.
     */
    size_t prefetch(const std::vector<Key>& keys,
                    std::function<Value(const Key&)> loader,
                    std::optional<std::chrono::seconds> ttl = std::nullopt);

private:
    mutable shared_mutex<std::shared_mutex>
        mutex_;  ///< Mutex for protecting shared data.
    std::list<KeyValuePair>
        cache_items_list_;  ///< List for maintaining item order.
    size_t max_size_;       ///< Maximum number of items in the cache.
    std::unordered_map<std::pair<std::string, std::string>, CacheItem,
                       PairStringHash>
        cache_items_map_;           ///< Map for fast key lookups.
    atomic<size_t> hit_count_{0};   ///< Number of cache hits.
    atomic<size_t> miss_count_{0};  ///< Number of cache misses.

    std::function<void(const Key&, const Value&)>
        on_insert_;  ///< Callback for item insertion.
    std::function<void(const Key&)> on_erase_;  ///< Callback for item removal.
    std::function<void()> on_clear_;  ///< Callback for cache clearing.

    /**
     * @brief Checks if a cache item has expired.
     *
     * @param item The cache item to check.
     * @return True if the item is expired, false otherwise.
     */
    [[nodiscard]] auto isExpired(const CacheItem& item) const noexcept -> bool;

    /**
     * @brief Removes the least recently used item from the cache.
     *
     * @note Assumes the mutex is already locked.
     * @return Key of the removed item, or std::nullopt if cache is empty.
     */
    auto removeLRUItem() noexcept -> std::optional<Key>;

    /**
     * @brief Helper method to acquire a read lock with timeout.
     *
     * @param timeout_ms Maximum time to wait for lock in milliseconds.
     * @return The acquired lock or empty optional if timeout occurred.
     */
    [[nodiscard]] auto acquireReadLock(std::chrono::milliseconds timeout_ms =
                                           std::chrono::milliseconds(100)) const
        -> std::optional<shared_lock<std::shared_mutex>>;

    /**
     * @brief Helper method to acquire a write lock with timeout.
     *
     * @param timeout_ms Maximum time to wait for lock in milliseconds.
     * @return The acquired lock or empty optional if timeout occurred.
     */
    [[nodiscard]] auto acquireWriteLock(
        std::chrono::milliseconds timeout_ms = std::chrono::milliseconds(100))
        -> std::optional<unique_lock<std::shared_mutex>>;
};

template <typename Key, typename Value>
ThreadSafeLRUCache<Key, Value>::ThreadSafeLRUCache(size_t max_size)
    : max_size_(max_size), cache_items_map_() {
    if (max_size == 0) {
        throw std::invalid_argument("Cache max size must be greater than zero");
    }
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::get(const Key& key)
    -> std::optional<Value> {
    auto sharedPtr = getShared(key);
    if (sharedPtr) {
        return *sharedPtr;
    }
    return std::nullopt;
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::getShared(const Key& key) noexcept
    -> ValuePtr {
    try {
        auto lock = acquireWriteLock();
        if (!lock) {
            return nullptr;  // Couldn't acquire lock
        }

        auto iterator = cache_items_map_.find(key);
        if (iterator == cache_items_map_.end() || isExpired(iterator->second)) {
            miss_count_++;
            if (iterator != cache_items_map_.end()) {
                // Remove expired item
                cache_items_list_.erase(iterator->second.iterator);
                cache_items_map_.erase(iterator);
                if (on_erase_) {
                    on_erase_(key);
                }
            }
            return nullptr;
        }
        hit_count_++;
        cache_items_list_.splice(cache_items_list_.begin(), cache_items_list_,
                                 iterator->second.iterator);
        return iterator->second.value;
    } catch (...) {
        // If any exception occurs, fail gracefully
        return nullptr;
    }
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::getBatch(const BatchKeyType& keys) noexcept
    -> BatchValueType {
    BatchValueType results;
    results.reserve(keys.size());

    try {
        auto lock = acquireWriteLock();
        if (!lock) {
            // If we can't get a lock, return empty results
            results.resize(keys.size(), nullptr);
            return results;
        }

        for (const auto& key : keys) {
            auto iterator = cache_items_map_.find(key);
            if (iterator != cache_items_map_.end() &&
                !isExpired(iterator->second)) {
                hit_count_++;
                cache_items_list_.splice(cache_items_list_.begin(),
                                         cache_items_list_,
                                         iterator->second.iterator);
                results.push_back(iterator->second.value);
            } else {
                miss_count_++;
                if (iterator != cache_items_map_.end()) {
                    // Remove expired item
                    cache_items_list_.erase(iterator->second.iterator);
                    cache_items_map_.erase(iterator);
                    if (on_erase_) {
                        on_erase_(key);
                    }
                }
                results.push_back(nullptr);
            }
        }
    } catch (...) {
        // If any exception occurs, fill remaining results with nullptr
        results.resize(keys.size(), nullptr);
    }

    return results;
}

template <typename Key, typename Value>
bool ThreadSafeLRUCache<Key, Value>::contains(const Key& key) const noexcept {
    try {
        auto lock = acquireReadLock();
        if (!lock) {
            return false;
        }

        auto it = cache_items_map_.find(key);
        if (it == cache_items_map_.end()) {
            return false;
        }

        return !isExpired(it->second);
    } catch (...) {
        return false;
    }
}

template <typename Key, typename Value>
void ThreadSafeLRUCache<Key, Value>::put(
    const Key& key, Value value, std::optional<std::chrono::seconds> ttl) {
    try {
        auto lock = acquireWriteLock();
        if (!lock) {
            throw LRUCacheLockException(
                "Failed to acquire write lock during put operation");
        }

        auto expiryTime = ttl ? Clock::now() + *ttl : TimePoint::max();
        auto valuePtr = std::make_shared<Value>(std::move(value));

        auto iterator = cache_items_map_.find(key);
        if (iterator != cache_items_map_.end()) {
            cache_items_list_.splice(cache_items_list_.begin(),
                                     cache_items_list_,
                                     iterator->second.iterator);
            iterator->second.value = valuePtr;
            iterator->second.expiryTime = expiryTime;
        } else {
            cache_items_list_.emplace_front(key, *valuePtr);
            cache_items_map_[key] = {valuePtr, expiryTime,
                                     cache_items_list_.begin()};

            while (cache_items_map_.size() > max_size_) {
                removeLRUItem();
            }
        }

        if (on_insert_) {
            on_insert_(key, *valuePtr);
        }
    } catch (const LRUCacheLockException&) {
        throw;  // Rethrow lock exceptions
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to add item to cache: ") +
                                 e.what());
    }
}

template <typename Key, typename Value>
void ThreadSafeLRUCache<Key, Value>::putBatch(
    const std::vector<KeyValuePair>& items,
    std::optional<std::chrono::seconds> ttl) {
    try {
        if (items.empty()) {
            return;
        }

        auto lock = acquireWriteLock();
        if (!lock) {
            throw LRUCacheLockException(
                "Failed to acquire write lock during batch put operation");
        }

        auto expiryTime = ttl ? Clock::now() + *ttl : TimePoint::max();

        for (const auto& [key, value] : items) {
            auto valuePtr = std::make_shared<Value>(value);
            auto iterator = cache_items_map_.find(key);

            if (iterator != cache_items_map_.end()) {
                cache_items_list_.splice(cache_items_list_.begin(),
                                         cache_items_list_,
                                         iterator->second.iterator);
                iterator->second.value = valuePtr;
                iterator->second.expiryTime = expiryTime;
            } else {
                cache_items_list_.emplace_front(key, value);
                cache_items_map_[key] = {valuePtr, expiryTime,
                                         cache_items_list_.begin()};

                if (on_insert_) {
                    on_insert_(key, value);
                }
            }
        }

        // If we've added too many items, remove LRU items
        while (cache_items_map_.size() > max_size_) {
            removeLRUItem();
        }
    } catch (const LRUCacheLockException&) {
        throw;  // Rethrow lock exceptions
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("Failed to add batch items to cache: ") + e.what());
    }
}

template <typename Key, typename Value>
bool ThreadSafeLRUCache<Key, Value>::erase(const Key& key) noexcept {
    try {
        auto lock = acquireWriteLock();
        if (!lock) {
            return false;
        }

        auto iterator = cache_items_map_.find(key);
        if (iterator == cache_items_map_.end()) {
            return false;
        }

        cache_items_list_.erase(iterator->second.iterator);
        cache_items_map_.erase(iterator);

        if (on_erase_) {
            on_erase_(key);
        }

        return true;
    } catch (...) {
        return false;
    }
}

template <typename Key, typename Value>
void ThreadSafeLRUCache<Key, Value>::clear() noexcept {
    try {
        auto lock = acquireWriteLock();
        if (!lock) {
            return;
        }

        cache_items_list_.clear();
        cache_items_map_.clear();

        if (on_clear_) {
            on_clear_();
        }
    } catch (...) {
        // Silently fail
    }
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::keys() const -> std::vector<Key> {
    try {
        auto lock = acquireReadLock();
        if (!lock) {
            throw LRUCacheLockException(
                "Failed to acquire read lock during keys operation");
        }

        std::vector<Key> keys;
        keys.reserve(cache_items_map_.size());

        for (const auto& pair : cache_items_list_) {
            keys.push_back(pair.first);
        }

        return keys;
    } catch (const LRUCacheLockException&) {
        throw;  // Rethrow lock exceptions
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to retrieve keys: ") +
                                 e.what());
    }
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::popLru() noexcept
    -> std::optional<KeyValuePair> {
    try {
        auto lock = acquireWriteLock();
        if (!lock || cache_items_list_.empty()) {
            return std::nullopt;
        }

        auto last = cache_items_list_.end();
        --last;
        KeyValuePair keyValuePair = *last;

        cache_items_map_.erase(last->first);
        cache_items_list_.pop_back();

        if (on_erase_) {
            on_erase_(last->first);
        }

        return keyValuePair;
    } catch (...) {
        return std::nullopt;
    }
}

template <typename Key, typename Value>
void ThreadSafeLRUCache<Key, Value>::resize(size_t new_max_size) {
    if (new_max_size == 0) {
        throw std::invalid_argument("Cache max size must be greater than zero");
    }

    try {
        auto lock = acquireWriteLock();
        if (!lock) {
            throw LRUCacheLockException(
                "Failed to acquire write lock during resize operation");
        }

        max_size_ = new_max_size;

        while (cache_items_map_.size() > max_size_) {
            removeLRUItem();
        }
    } catch (const LRUCacheLockException&) {
        throw;  // Rethrow lock exceptions
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to resize cache: ") +
                                 e.what());
    }
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::size() const noexcept -> size_t {
    auto lock = acquireReadLock();
    if (!lock) {
        return 0;
    }
    return cache_items_map_.size();
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::maxSize() const noexcept -> size_t {
    return max_size_;
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::loadFactor() const noexcept -> float {
    auto lock = acquireReadLock();
    if (!lock) {
        return 0.0f;
    }
    return static_cast<float>(cache_items_map_.size()) /
           static_cast<float>(max_size_);
}

template <typename Key, typename Value>
void ThreadSafeLRUCache<Key, Value>::setInsertCallback(
    std::function<void(const Key&, const Value&)> callback) {
    auto lock = acquireWriteLock();
    if (!lock) {
        throw LRUCacheLockException(
            "Failed to acquire write lock when setting insert callback");
    }
    on_insert_ = std::move(callback);
}

template <typename Key, typename Value>
void ThreadSafeLRUCache<Key, Value>::setEraseCallback(
    std::function<void(const Key&)> callback) {
    auto lock = acquireWriteLock();
    if (!lock) {
        throw LRUCacheLockException(
            "Failed to acquire write lock when setting erase callback");
    }
    on_erase_ = std::move(callback);
}

template <typename Key, typename Value>
void ThreadSafeLRUCache<Key, Value>::setClearCallback(
    std::function<void()> callback) {
    auto lock = acquireWriteLock();
    if (!lock) {
        throw LRUCacheLockException(
            "Failed to acquire write lock when setting clear callback");
    }
    on_clear_ = std::move(callback);
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::hitRate() const noexcept -> float {
    size_t hits = hit_count_.load(std::memory_order_relaxed);
    size_t misses = miss_count_.load(std::memory_order_relaxed);
    size_t total = hits + misses;
    return total == 0 ? 0.0f
                      : static_cast<float>(hits) / static_cast<float>(total);
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::getStatistics() const noexcept
    -> CacheStatistics {
    size_t hits = hit_count_.load(std::memory_order_relaxed);
    size_t misses = miss_count_.load(std::memory_order_relaxed);
    size_t total = hits + misses;
    float rate = total == 0
                     ? 0.0f
                     : static_cast<float>(hits) / static_cast<float>(total);

    size_t currentSize = 0;
    float currentLoadFactor = 0.0f;

    auto lock = acquireReadLock();
    if (lock) {
        currentSize = cache_items_map_.size();
        currentLoadFactor =
            static_cast<float>(currentSize) / static_cast<float>(max_size_);
    }

    return CacheStatistics{hits,        misses,    rate,
                           currentSize, max_size_, currentLoadFactor};
}

template <typename Key, typename Value>
void ThreadSafeLRUCache<Key, Value>::saveToFile(
    const std::string& filename) const {
    try {
        auto lock = acquireReadLock();
        if (!lock) {
            throw LRUCacheLockException(
                "Failed to acquire read lock during save operation");
        }

        std::ofstream ofs(filename, std::ios::binary);
        if (!ofs) {
            throw LRUCacheIOException("Failed to open file for writing: " +
                                      filename);
        }

        // Write cache metadata
        size_t size = cache_items_map_.size();
        ofs.write(reinterpret_cast<const char*>(&size), sizeof(size));
        ofs.write(reinterpret_cast<const char*>(&max_size_), sizeof(max_size_));

        // Save items in LRU order
        for (const auto& pair : cache_items_list_) {
            // Find this item to get its expiry time
            auto it = cache_items_map_.find(pair.first);
            if (it == cache_items_map_.end()) {
                continue;  // Skip if not found (shouldn't happen)
            }

            // Don't save expired items
            if (isExpired(it->second)) {
                continue;
            }

            // Calculate remaining TTL in seconds
            auto now = Clock::now();
            int64_t remainingTtl = -1;  // -1 means no expiry

            if (it->second.expiryTime != TimePoint::max()) {
                auto ttlDuration =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        it->second.expiryTime - now);
                remainingTtl = ttlDuration.count();

                // Skip if already expired or about to expire
                if (remainingTtl <= 0) {
                    continue;
                }
            }

            // Write key
            ofs.write(reinterpret_cast<const char*>(&pair.first),
                      sizeof(pair.first));

            // Write TTL
            ofs.write(reinterpret_cast<const char*>(&remainingTtl),
                      sizeof(remainingTtl));

            // Write value (handling string type specially)
            if constexpr (std::is_same_v<Value, std::string>) {
                size_t valueSize = pair.second.size();
                ofs.write(reinterpret_cast<const char*>(&valueSize),
                          sizeof(valueSize));
                ofs.write(pair.second.c_str(), valueSize);
            } else {
                ofs.write(reinterpret_cast<const char*>(&pair.second),
                          sizeof(pair.second));
            }
        }

        if (!ofs) {
            throw LRUCacheIOException("Failed writing to file: " + filename);
        }
    } catch (const LRUCacheLockException&) {
        throw;
    } catch (const LRUCacheIOException&) {
        throw;
    } catch (const std::exception& e) {
        throw LRUCacheIOException(std::string("Failed to save cache: ") +
                                  e.what());
    }
}

template <typename Key, typename Value>
void ThreadSafeLRUCache<Key, Value>::loadFromFile(const std::string& filename) {
    try {
        auto lock = acquireWriteLock();
        if (!lock) {
            throw LRUCacheLockException(
                "Failed to acquire write lock during load operation");
        }

        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs) {
            throw LRUCacheIOException("Failed to open file for reading: " +
                                      filename);
        }

        // Clear current cache
        cache_items_list_.clear();
        cache_items_map_.clear();

        // Read cache metadata
        size_t size;
        size_t storedMaxSize;
        ifs.read(reinterpret_cast<char*>(&size), sizeof(size));
        ifs.read(reinterpret_cast<char*>(&storedMaxSize),
                 sizeof(storedMaxSize));

        if (!ifs) {
            throw LRUCacheIOException(
                "Failed to read cache metadata from file");
        }

        // Read cache items
        for (size_t i = 0; i < size && ifs; ++i) {
            Key key;
            ifs.read(reinterpret_cast<char*>(&key), sizeof(key));

            // Read TTL
            int64_t ttlSeconds;
            ifs.read(reinterpret_cast<char*>(&ttlSeconds), sizeof(ttlSeconds));

            // Read value
            Value value;
            if constexpr (std::is_same_v<Value, std::string>) {
                size_t valueSize;
                ifs.read(reinterpret_cast<char*>(&valueSize),
                         sizeof(valueSize));
                value.resize(valueSize);
                ifs.read(&value[0], static_cast<std::streamsize>(valueSize));
            } else {
                ifs.read(reinterpret_cast<char*>(&value), sizeof(value));
            }

            if (!ifs) {
                throw LRUCacheIOException(
                    "Failed to read cache item from file");
            }

            // Calculate expiry time
            std::optional<std::chrono::seconds> ttl =
                (ttlSeconds >= 0) ? std::optional<std::chrono::seconds>(
                                        std::chrono::seconds(ttlSeconds))
                                  : std::nullopt;

            // Add to cache
            put(key, std::move(value), ttl);

            // Check if we've reached max capacity
            if (cache_items_map_.size() >= max_size_) {
                break;
            }
        }
    } catch (const LRUCacheLockException&) {
        throw;
    } catch (const LRUCacheIOException&) {
        throw;
    } catch (const std::exception& e) {
        throw LRUCacheIOException(std::string("Failed to load cache: ") +
                                  e.what());
    }
}

template <typename Key, typename Value>
size_t ThreadSafeLRUCache<Key, Value>::pruneExpired() noexcept {
    try {
        auto lock = acquireWriteLock();
        if (!lock) {
            return 0;
        }

        size_t prunedCount = 0;
        auto it = cache_items_list_.begin();

        while (it != cache_items_list_.end()) {
            auto mapIt = cache_items_map_.find(it->first);
            if (mapIt != cache_items_map_.end() && isExpired(mapIt->second)) {
                // Item expired, remove it
                if (on_erase_) {
                    on_erase_(it->first);
                }
                cache_items_map_.erase(mapIt);
                it = cache_items_list_.erase(it);
                prunedCount++;
            } else {
                ++it;
            }
        }

        return prunedCount;
    } catch (...) {
        return 0;
    }
}

template <typename Key, typename Value>
size_t ThreadSafeLRUCache<Key, Value>::prefetch(
    const std::vector<Key>& keys, std::function<Value(const Key&)> loader,
    std::optional<std::chrono::seconds> ttl) {
    if (keys.empty() || !loader) {
        return 0;
    }

    try {
        // First identify which keys we need to load
        std::vector<Key> keysToLoad;
        {
            auto readLock = acquireReadLock();
            if (!readLock) {
                return 0;
            }

            for (const auto& key : keys) {
                auto it = cache_items_map_.find(key);
                if (it == cache_items_map_.end() || isExpired(it->second)) {
                    keysToLoad.push_back(key);
                }
            }
        }

        if (keysToLoad.empty()) {
            return 0;  // All keys are already in cache
        }

        // Load the values
        std::vector<KeyValuePair> loadedItems;
        loadedItems.reserve(keysToLoad.size());

        for (const auto& key : keysToLoad) {
            try {
                Value value = loader(key);
                loadedItems.emplace_back(key, std::move(value));
            } catch (...) {
                // Skip keys that fail to load
                continue;
            }
        }

        // Put all loaded items in the cache
        putBatch(loadedItems, ttl);
        return loadedItems.size();
    } catch (...) {
        return 0;
    }
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::isExpired(
    const CacheItem& item) const noexcept -> bool {
    return Clock::now() > item.expiryTime;
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::removeLRUItem() noexcept
    -> std::optional<Key> {
    // Assumes mutex is already locked
    if (cache_items_list_.empty()) {
        return std::nullopt;
    }

    auto last = cache_items_list_.end();
    --last;
    Key key = last->first;

    if (on_erase_) {
        try {
            on_erase_(key);
        } catch (...) {
            // Ignore callback exceptions
        }
    }

    cache_items_map_.erase(key);
    cache_items_list_.pop_back();

    return key;
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::acquireReadLock(
    [[maybe_unused]] std::chrono::milliseconds timeout_ms) const
    -> std::optional<shared_lock<std::shared_mutex>> {
    shared_lock<std::shared_mutex> lock(mutex_, std::defer_lock);

#if defined(ATOM_USE_BOOST_THREAD)
    if (lock.try_lock_for(timeout_ms)) {
        return lock;
    }
#else
    // Standard library doesn't have try_lock_for, use a simple try_lock instead
    if (lock.try_lock()) {
        return lock;
    }
    // Could implement a timed retry loop here if needed
#endif

    return std::nullopt;
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::acquireWriteLock(
    [[maybe_unused]] std::chrono::milliseconds timeout_ms)
    -> std::optional<unique_lock<std::shared_mutex>> {
    unique_lock<std::shared_mutex> lock(mutex_, std::defer_lock);

#if defined(ATOM_USE_BOOST_THREAD)
    if (lock.try_lock_for(timeout_ms)) {
        return lock;
    }
#else
    // Standard library doesn't have try_lock_for, use a simple try_lock instead
    if (lock.try_lock()) {
        return lock;
    }
    // Could implement a timed retry loop here if needed
#endif

    return std::nullopt;
}

}  // namespace atom::search

#endif  // THREADSAFE_LRU_CACHE_H