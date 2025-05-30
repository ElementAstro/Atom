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

#include <spdlog/spdlog.h>

namespace atom::search {

struct PairStringHash {
    size_t operator()(const std::pair<std::string, std::string>& p) const {
        size_t h1 = std::hash<std::string>()(p.first);
        size_t h2 = std::hash<std::string>()(p.second);
        return h1 ^ (h2 << 1);
    }
};

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
    using KeyValuePair = std::pair<Key, Value>;
    using ListIterator = typename std::list<KeyValuePair>::iterator;
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using ValuePtr = std::shared_ptr<Value>;
    using BatchKeyType = std::vector<Key>;
    using BatchValueType = std::vector<ValuePtr>;

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
     * @brief Destructor ensures proper cleanup.
     */
    ~ThreadSafeLRUCache() = default;

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
     * @brief Retrieves all values in the cache.
     *
     * @return A vector containing all values currently in the cache.
     */
    [[nodiscard]] auto values() const -> std::vector<Value>;

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
     * @brief Checks if the cache is empty.
     *
     * @return True if the cache is empty, false otherwise.
     */
    [[nodiscard]] bool empty() const noexcept;

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
     * @brief Resets cache statistics.
     */
    void resetStatistics() noexcept;

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

    /**
     * @brief Asynchronously retrieves a value from the cache.
     *
     * @param key The key of the item to retrieve.
     * @return A future containing an optional with the value if found,
     * otherwise std::nullopt.
     */
    [[nodiscard]] auto asyncGet(const Key& key) -> future<std::optional<Value>>;

    /**
     * @brief Asynchronously inserts or updates a value in the cache.
     *
     * @param key The key of the item to insert or update.
     * @param value The value to associate with the key.
     * @param ttl Optional time-to-live duration for the cache item.
     * @return A future that completes when the operation is done.
     */
    auto asyncPut(const Key& key, Value value,
                  std::optional<std::chrono::seconds> ttl = std::nullopt)
        -> future<void>;

    /**
     * @brief Sets the default TTL for cache items.
     *
     * @param ttl The default time-to-live duration.
     */
    void setDefaultTTL(std::chrono::seconds ttl);

    /**
     * @brief Gets the default TTL for cache items.
     *
     * @return The default time-to-live duration.
     */
    [[nodiscard]] auto getDefaultTTL() const noexcept
        -> std::optional<std::chrono::seconds>;

private:
    mutable shared_mutex<std::shared_mutex> mutex_;
    std::list<KeyValuePair> cache_items_list_;
    size_t max_size_;
    std::unordered_map<
        Key, CacheItem,
        std::conditional_t<
            std::is_same_v<Key, std::pair<std::string, std::string>>,
            PairStringHash, std::hash<Key>>>
        cache_items_map_;
    atomic<size_t> hit_count_{0};
    atomic<size_t> miss_count_{0};
    std::function<void(const Key&, const Value&)> on_insert_;
    std::function<void(const Key&)> on_erase_;
    std::function<void()> on_clear_;
    std::optional<std::chrono::seconds> default_ttl_;

    [[nodiscard]] auto isExpired(const CacheItem& item) const noexcept -> bool;
    auto removeLRUItem() noexcept -> std::optional<Key>;
    [[nodiscard]] auto acquireReadLock(std::chrono::milliseconds timeout_ms =
                                           std::chrono::milliseconds(100)) const
        -> std::optional<shared_lock<std::shared_mutex>>;
    [[nodiscard]] auto acquireWriteLock(
        std::chrono::milliseconds timeout_ms = std::chrono::milliseconds(100))
        -> std::optional<unique_lock<std::shared_mutex>>;
};

template <typename Key, typename Value>
ThreadSafeLRUCache<Key, Value>::ThreadSafeLRUCache(size_t max_size)
    : max_size_(max_size) {
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
            spdlog::warn("Failed to acquire lock for get operation on key");
            return nullptr;
        }

        auto iterator = cache_items_map_.find(key);
        if (iterator == cache_items_map_.end() || isExpired(iterator->second)) {
            miss_count_++;
            if (iterator != cache_items_map_.end()) {
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
    } catch (const std::exception& e) {
        spdlog::error("Exception in getShared: {}", e.what());
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
            spdlog::warn("Failed to acquire lock for batch get operation");
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
                    cache_items_list_.erase(iterator->second.iterator);
                    cache_items_map_.erase(iterator);
                    if (on_erase_) {
                        on_erase_(key);
                    }
                }
                results.push_back(nullptr);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception in getBatch: {}", e.what());
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
    } catch (const std::exception& e) {
        spdlog::error("Exception in contains: {}", e.what());
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

        auto effectiveTtl = ttl ? ttl : default_ttl_;
        auto expiryTime =
            effectiveTtl ? Clock::now() + *effectiveTtl : TimePoint::max();
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
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Failed to add item to cache: {}", e.what());
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

        auto effectiveTtl = ttl ? ttl : default_ttl_;
        auto expiryTime =
            effectiveTtl ? Clock::now() + *effectiveTtl : TimePoint::max();

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

        while (cache_items_map_.size() > max_size_) {
            removeLRUItem();
        }
    } catch (const LRUCacheLockException&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Failed to add batch items to cache: {}", e.what());
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
    } catch (const std::exception& e) {
        spdlog::error("Exception in erase: {}", e.what());
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
    } catch (const std::exception& e) {
        spdlog::error("Exception in clear: {}", e.what());
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
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Failed to retrieve keys: {}", e.what());
        throw std::runtime_error(std::string("Failed to retrieve keys: ") +
                                 e.what());
    }
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::values() const -> std::vector<Value> {
    try {
        auto lock = acquireReadLock();
        if (!lock) {
            throw LRUCacheLockException(
                "Failed to acquire read lock during values operation");
        }

        std::vector<Value> values;
        values.reserve(cache_items_map_.size());

        for (const auto& pair : cache_items_list_) {
            values.push_back(pair.second);
        }

        return values;
    } catch (const LRUCacheLockException&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Failed to retrieve values: {}", e.what());
        throw std::runtime_error(std::string("Failed to retrieve values: ") +
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
            on_erase_(keyValuePair.first);
        }

        return keyValuePair;
    } catch (const std::exception& e) {
        spdlog::error("Exception in popLru: {}", e.what());
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
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Failed to resize cache: {}", e.what());
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
bool ThreadSafeLRUCache<Key, Value>::empty() const noexcept {
    auto lock = acquireReadLock();
    if (!lock) {
        return true;
    }
    return cache_items_map_.empty();
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
void ThreadSafeLRUCache<Key, Value>::resetStatistics() noexcept {
    hit_count_.store(0, std::memory_order_relaxed);
    miss_count_.store(0, std::memory_order_relaxed);
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

        size_t size = cache_items_map_.size();
        ofs.write(reinterpret_cast<const char*>(&size), sizeof(size));
        ofs.write(reinterpret_cast<const char*>(&max_size_), sizeof(max_size_));

        for (const auto& pair : cache_items_list_) {
            auto it = cache_items_map_.find(pair.first);
            if (it == cache_items_map_.end()) {
                continue;
            }

            if (isExpired(it->second)) {
                continue;
            }

            auto now = Clock::now();
            int64_t remainingTtl = -1;

            if (it->second.expiryTime != TimePoint::max()) {
                auto ttlDuration =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        it->second.expiryTime - now);
                remainingTtl = ttlDuration.count();

                if (remainingTtl <= 0) {
                    continue;
                }
            }

            ofs.write(reinterpret_cast<const char*>(&pair.first),
                      sizeof(pair.first));
            ofs.write(reinterpret_cast<const char*>(&remainingTtl),
                      sizeof(remainingTtl));

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
        spdlog::error("Failed to save cache: {}", e.what());
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

        cache_items_list_.clear();
        cache_items_map_.clear();

        size_t size;
        size_t storedMaxSize;
        ifs.read(reinterpret_cast<char*>(&size), sizeof(size));
        ifs.read(reinterpret_cast<char*>(&storedMaxSize),
                 sizeof(storedMaxSize));

        if (!ifs) {
            throw LRUCacheIOException(
                "Failed to read cache metadata from file");
        }

        for (size_t i = 0; i < size && ifs; ++i) {
            Key key;
            ifs.read(reinterpret_cast<char*>(&key), sizeof(key));

            int64_t ttlSeconds;
            ifs.read(reinterpret_cast<char*>(&ttlSeconds), sizeof(ttlSeconds));

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

            std::optional<std::chrono::seconds> ttl =
                (ttlSeconds >= 0) ? std::optional<std::chrono::seconds>(
                                        std::chrono::seconds(ttlSeconds))
                                  : std::nullopt;

            put(key, std::move(value), ttl);

            if (cache_items_map_.size() >= max_size_) {
                break;
            }
        }
    } catch (const LRUCacheLockException&) {
        throw;
    } catch (const LRUCacheIOException&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Failed to load cache: {}", e.what());
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
    } catch (const std::exception& e) {
        spdlog::error("Exception in pruneExpired: {}", e.what());
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
            return 0;
        }

        std::vector<KeyValuePair> loadedItems;
        loadedItems.reserve(keysToLoad.size());

        for (const auto& key : keysToLoad) {
            try {
                Value value = loader(key);
                loadedItems.emplace_back(key, std::move(value));
            } catch (const std::exception& e) {
                spdlog::warn("Failed to load key in prefetch: {}", e.what());
                continue;
            }
        }

        putBatch(loadedItems, ttl);
        return loadedItems.size();
    } catch (const std::exception& e) {
        spdlog::error("Exception in prefetch: {}", e.what());
        return 0;
    }
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::asyncGet(const Key& key)
    -> future<std::optional<Value>> {
    return std::async(
        std::launch::async,
        [this, key]() -> std::optional<Value> { return get(key); });
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::asyncPut(
    const Key& key, Value value, std::optional<std::chrono::seconds> ttl)
    -> future<void> {
    return std::async(std::launch::async,
                      [this, key, value = std::move(value), ttl]() mutable {
                          put(key, std::move(value), ttl);
                      });
}

template <typename Key, typename Value>
void ThreadSafeLRUCache<Key, Value>::setDefaultTTL(std::chrono::seconds ttl) {
    auto lock = acquireWriteLock();
    if (!lock) {
        throw LRUCacheLockException(
            "Failed to acquire write lock when setting default TTL");
    }
    default_ttl_ = ttl;
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::getDefaultTTL() const noexcept
    -> std::optional<std::chrono::seconds> {
    return default_ttl_;
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::isExpired(
    const CacheItem& item) const noexcept -> bool {
    return Clock::now() > item.expiryTime;
}

template <typename Key, typename Value>
auto ThreadSafeLRUCache<Key, Value>::removeLRUItem() noexcept
    -> std::optional<Key> {
    if (cache_items_list_.empty()) {
        return std::nullopt;
    }

    auto last = cache_items_list_.end();
    --last;
    Key key = last->first;

    if (on_erase_) {
        try {
            on_erase_(key);
        } catch (const std::exception& e) {
            spdlog::warn("Exception in erase callback: {}", e.what());
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
    if (lock.try_lock()) {
        return lock;
    }
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
    if (lock.try_lock()) {
        return lock;
    }
#endif

    return std::nullopt;
}

}  // namespace atom::search

#endif  // THREADSAFE_LRU_CACHE_H