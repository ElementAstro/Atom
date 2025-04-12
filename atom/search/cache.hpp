/*
 * cache.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-6

Description: ResourceCache class for Atom Search

**************************************************/

#ifndef ATOM_SEARCH_CACHE_HPP
#define ATOM_SEARCH_CACHE_HPP

#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <future>
#include <list>  // Keep std::list for LRU specifically
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <utility>

// Include high performance containers first
#include "atom/containers/high_performance.hpp"

// Boost support (Keep conditional includes for synchronization/threading if
// needed)
#if defined(ATOM_USE_BOOST_THREAD) || defined(ATOM__USE_BOOST_LOCKFREE)
#include <boost/config.hpp>
#endif

#ifdef ATOM_USE_BOOST_THREAD
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
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

#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
using json = nlohmann::json;

// Use type aliases from high_performance.hpp
using atom::containers::HashMap;
using atom::containers::String;
using atom::containers::Vector;

namespace atom::search {

// Define type aliases based on available libraries (Keep for sync/thread
// primitives)
#if defined(ATOM_USE_BOOST_THREAD)
using SharedMutex = boost::shared_mutex;  // Use boost::shared_mutex directly

template <typename T>
using SharedLock = boost::shared_lock<T>;

template <typename T>
using UniqueLock = boost::unique_lock<T>;

template <typename... Args>
using Future = boost::future<Args...>;

template <typename... Args>
using Promise = boost::promise<Args...>;

using Thread = boost::thread;
using JThread =
    boost::thread;  // Boost doesn't have a direct jthread equivalent
#else
using SharedMutex = std::shared_mutex;  // Use std::shared_mutex directly

template <typename T>
using SharedLock = std::shared_lock<T>;

template <typename T>
using UniqueLock = std::unique_lock<T>;

template <typename... Args>
using Future = std::future<Args...>;

template <typename... Args>
using Promise = std::promise<Args...>;

using Thread = std::thread;
using JThread = std::jthread;
#endif

#if defined(ATOM_USE_BOOST_LOCKFREE)
template <typename T>
using Atomic = boost::atomic<T>;

// Keep custom LockFreeQueue for now due to T* usage,
// or adapt to use atom::containers::hp::lockfree::queue if
// ATOM_HAS_BOOST_LOCKFREE is set
template <typename T>
class LockFreeQueue {
private:
    boost::lockfree::queue<T *> queue;

public:
    explicit LockFreeQueue(size_t capacity) : queue(capacity) {}

    bool push(const T &item) {
        T *ptr = new T(item);
        if (!queue.push(ptr)) {
            delete ptr;
            return false;
        }
        return true;
    }

    bool pop(T &item) {
        T *ptr = nullptr;
        if (!queue.pop(ptr)) {
            return false;
        }
        item = *ptr;
        delete ptr;
        return true;
    }

    bool empty() const { return queue.empty(); }

    ~LockFreeQueue() {
        T *ptr;
        while (queue.pop(ptr)) {
            delete ptr;
        }
    }
};
#else
template <typename T>
using Atomic = std::atomic<T>;

// Fallback implementation using std containers when Boost.lockfree is not
// available
template <typename T>
class LockFreeQueue {
private:
    std::mutex mutex_;
    // Use atom::containers::Vector if appropriate, but std::vector is fine here
    // too
    std::vector<T> items_;
    size_t capacity_;

public:
    explicit LockFreeQueue(size_t capacity) : capacity_(capacity) {
        items_.reserve(capacity);
    }

    bool push(const T &item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (items_.size() >= capacity_) {
            return false;
        }
        items_.push_back(item);
        return true;
    }

    bool pop(T &item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (items_.empty()) {
            return false;
        }
        item = items_.front();
        // Use vector erase for front element (less efficient than deque)
        items_.erase(items_.begin());
        return true;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.empty();
    }
};
#endif

template <typename T>
concept Cacheable = std::copy_constructible<T> && std::is_copy_assignable_v<T>;

/**
 * @brief A thread-safe cache for storing and managing resources with expiration
 * times.
 *
 * @tparam T The type of the resources to be cached. Must satisfy the Cacheable
 * concept.
 */
template <Cacheable T>
class ResourceCache {
public:
    // Use atom::containers::String for keys in callbacks
    using Callback = std::function<void(const String &key)>;
    /**
     * @brief Constructs a ResourceCache with a specified maximum size.
     *
     * @param maxSize The maximum number of items the cache can hold.
     */
    explicit ResourceCache(int maxSize);

    /**
     * @brief Destructs the ResourceCache and stops the cleanup thread.
     */
    ~ResourceCache();

    /**
     * @brief Inserts a resource into the cache with an expiration time.
     *
     * @param key The key associated with the resource.
     * @param value The resource to be cached.
     * @param expirationTime The time after which the resource expires.
     */
    // Use atom::containers::String for key
    void insert(const String &key, const T &value,
                std::chrono::seconds expirationTime);

    /**
     * @brief Checks if the cache contains a resource with the specified key.
     *
     * @param key The key to check.
     * @return True if the cache contains the resource, false otherwise.
     */
    // Use atom::containers::String for key
    auto contains(const String &key) const -> bool;

    /**
     * @brief Retrieves a resource from the cache.
     *
     * @param key The key associated with the resource.
     * @return An optional containing the resource if found, otherwise
     * std::nullopt.
     */
    // Use atom::containers::String for key
    auto get(const String &key) -> std::optional<T>;

    /**
     * @brief Removes a resource from the cache.
     *
     * @param key The key associated with the resource to be removed.
     */
    // Use atom::containers::String for key
    void remove(const String &key);

    /**
     * @brief Asynchronously retrieves a resource from the cache.
     *
     * @param key The key associated with the resource.
     * @return A future containing an optional with the resource if found,
     * otherwise std::nullopt.
     */
    // Use atom::containers::String for key
    auto asyncGet(const String &key)
        -> Future<std::optional<T>>;  // Use Future alias

    /**
     * @brief Asynchronously inserts a resource into the cache with an
     * expiration time.
     *
     * @param key The key associated with the resource.
     * @param value The resource to be cached.
     * @param expirationTime The time after which the resource expires.
     * @return A future that completes when the insertion is done.
     */
    // Use atom::containers::String for key
    auto asyncInsert(const String &key, const T &value,
                     std::chrono::seconds expirationTime)
        -> Future<void>;  // Use Future alias

    /**
     * @brief Clears all resources from the cache.
     */
    void clear();

    /**
     * @brief Gets the number of resources in the cache.
     *
     * @return The number of resources in the cache.
     */
    auto size() const -> size_t;

    /**
     * @brief Checks if the cache is empty.
     *
     * @return True if the cache is empty, false otherwise.
     */
    auto empty() const -> bool;

    /**
     * @brief Evicts the oldest resource from the cache.
     */
    void evictOldest();

    /**
     * @brief Checks if a resource with the specified key is expired.
     *
     * @param key The key associated with the resource.
     * @return True if the resource is expired, false otherwise.
     */
    // Use atom::containers::String for key
    auto isExpired(const String &key) const -> bool;

    /**
     * @brief Asynchronously loads a resource into the cache using a provided
     * function.
     *
     * @param key The key associated with the resource.
     * @param loadDataFunction The function to load the resource.
     * @return A future that completes when the resource is loaded.
     */
    // Use atom::containers::String for key
    auto asyncLoad(const String &key,
                   std::function<T()> loadDataFunction)
        -> Future<void>;  // Use Future alias

    /**
     * @brief Sets the maximum size of the cache.
     *
     * @param maxSize The new maximum size of the cache.
     */
    void setMaxSize(int maxSize);

    /**
     * @brief Sets the expiration time for a resource in the cache.
     *
     * @param key The key associated with the resource.
     * @param expirationTime The new expiration time for the resource.
     */
    // Use atom::containers::String for key
    void setExpirationTime(const String &key,
                           std::chrono::seconds expirationTime);

    /**
     * @brief Reads resources from a file and inserts them into the cache.
     *
     * @param filePath The path to the file.
     * @param deserializer The function to deserialize the resources.
     */
    // Use atom::containers::String for filePath and deserializer input
    void readFromFile(const String &filePath,
                      const std::function<T(const String &)> &deserializer);

    /**
     * @brief Writes the resources in the cache to a file.
     *
     * @param filePath The path to the file.
     * @param serializer The function to serialize the resources.
     */
    // Use atom::containers::String for filePath and serializer output
    void writeToFile(const String &filePath,
                     const std::function<String(const T &)> &serializer);

    /**
     * @brief Removes expired resources from the cache.
     */
    void removeExpired();

    /**
     * @brief Reads resources from a JSON file and inserts them into the cache.
     *
     * @param filePath The path to the JSON file.
     * @param fromJson The function to deserialize the resources from JSON.
     */
    // Use atom::containers::String for filePath
    void readFromJsonFile(const String &filePath,
                          const std::function<T(const json &)> &fromJson);

    /**
     * @brief Writes the resources in the cache to a JSON file.
     *
     * @param filePath The path to the JSON file.
     * @param toJson The function to serialize the resources to JSON.
     */
    // Use atom::containers::String for filePath
    void writeToJsonFile(const String &filePath,
                         const std::function<json(const T &)> &toJson);

    /**
     * @brief Inserts multiple resources into the cache with an expiration time.
     *
     * @param items The vector of key-value pairs to insert.
     * @param expirationTime The time after which the resources expire.
     */
    // Use atom::containers::Vector and atom::containers::String
    void insertBatch(const Vector<std::pair<String, T>> &items,
                     std::chrono::seconds expirationTime);

    /**
     * @brief Removes multiple resources from the cache.
     *
     * @param keys The vector of keys associated with the resources to remove.
     */
    // Use atom::containers::Vector and atom::containers::String
    void removeBatch(const Vector<String> &keys);

    /**
     * @brief Registers a callback to be called on insertion.
     *
     * @param callback The callback function.
     */
    void onInsert(Callback callback);

    /**
     * @brief Registers a callback to be called on removal.
     *
     * @param callback The callback function.
     */
    void onRemove(Callback callback);

    /**
     * @brief Retrieves cache statistics.
     *
     * @return A pair containing hit count and miss count.
     */
    std::pair<size_t, size_t> getStatistics() const;

private:
    /**
     * @brief Evicts resources from the cache if it exceeds the maximum size.
     */
    void evict();

    /**
     * @brief Cleans up expired resources from the cache.
     */
    void cleanupExpiredEntries();

    // Use atom::containers::HashMap and atom::containers::String
    HashMap<String, std::pair<T, std::chrono::steady_clock::time_point>>
        cache_;    ///< The cache storing the resources and their expiration
                   ///< times.
    int maxSize_;  ///< The maximum number of resources the cache can hold.
    // Use atom::containers::HashMap and atom::containers::String
    HashMap<String, std::chrono::seconds>
        expirationTimes_;  ///< The expiration times for the resources.
    // Use atom::containers::HashMap and atom::containers::String
    HashMap<String, std::chrono::steady_clock::time_point>
        lastAccessTimes_;  ///< The last access times for the resources.
    // Use std::list for LRU, key type is atom::containers::String
    std::list<String>
        lruList_;  ///< The list of keys in least recently used order.
    // Corrected mutex type: Use the SharedMutex alias directly
    mutable SharedMutex
        cacheMutex_;         ///< Mutex for thread-safe access to the cache.
    JThread cleanupThread_;  ///< Thread for cleaning up expired resources.
    // Use Atomic alias
    Atomic<bool> stopCleanupThread_{
        false};  ///< Flag to stop the cleanup thread.

    Callback insertCallback_;
    Callback removeCallback_;
    // Use Atomic alias
    mutable Atomic<size_t> hitCount_{0};
    // Use Atomic alias
    mutable Atomic<size_t> missCount_{0};

    // Adaptive cleanup interval based on expired entry density
    std::chrono::seconds cleanupInterval_{
        1};  ///< The interval for cleaning up expired resources.
};

template <Cacheable T>
ResourceCache<T>::ResourceCache(int maxSize) : maxSize_(maxSize) {
    cleanupThread_ = JThread([this] { cleanupExpiredEntries(); });
}

template <Cacheable T>
ResourceCache<T>::~ResourceCache() {
    stopCleanupThread_.store(true);
    // Ensure thread is joinable before joining
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
}

template <Cacheable T>
void ResourceCache<T>::insert(const String &key, const T &value,
                              std::chrono::seconds expirationTime) {
    try {
        UniqueLock lock(cacheMutex_);
        // Use find instead of contains for map-like types if contains isn't
        // available Assuming HashMap has size()
        if (cache_.size() >= static_cast<size_t>(maxSize_)) {
            evictOldest();  // evictOldest already locks
        }
        // Re-lock if evictOldest unlocked, or ensure evictOldest keeps lock
        // Assuming evictOldest handles locking internally or we re-acquire if
        // needed. For simplicity, assume evictOldest is called under the lock.

        // Check again after potential eviction
        if (cache_.size() >= static_cast<size_t>(maxSize_)) {
            // This case should ideally not happen if eviction works correctly
            LOG_F(WARNING, "Cache still full after eviction attempt for key {}",
                  key.c_str());
            return;  // Or throw an exception
        }

        cache_[key] = {value, std::chrono::steady_clock::now()};
        expirationTimes_[key] = expirationTime;
        lastAccessTimes_[key] = std::chrono::steady_clock::now();
        // Remove existing key from list if present before adding to front
        lruList_.remove(key);
        lruList_.push_front(key);
        if (insertCallback_) {
            // Ensure callback is called outside the lock if it's long-running
            // For simplicity, calling it under the lock here.
            insertCallback_(key);
        }
    } catch (const std::exception &e) {
        // Use c_str() if String doesn't implicitly convert for logging
        LOG_F(ERROR, "Insert failed for key {}: {}", key.c_str(), e.what());
    }
}

template <Cacheable T>
auto ResourceCache<T>::contains(const String &key) const -> bool {
    SharedLock lock(cacheMutex_);
    // Use find for map-like types
    return cache_.find(key) != cache_.end();
}

template <Cacheable T>
auto ResourceCache<T>::get(const String &key) -> std::optional<T> {
    try {
        // Use find first under shared lock
        T value;
        bool found = false;
        bool expired = false;
        {
            SharedLock lock(cacheMutex_);
            auto it = cache_.find(key);
            if (it == cache_.end()) {
                missCount_++;
                return std::nullopt;
            }

            // Check expiration under shared lock
            auto expIt = expirationTimes_.find(key);
            if (expIt != expirationTimes_.end()) {
                if ((std::chrono::steady_clock::now() - it->second.second) >=
                    expIt->second) {
                    expired = true;
                }
            }

            if (expired) {
                // Don't remove under shared lock, just mark as expired
                missCount_++;
                // Need to return nullopt, but remove later or signal removal
            } else {
                // Found and not expired
                value = it->second.first;  // Copy value under shared lock
                found = true;
                hitCount_++;
            }
        }  // Shared lock released here

        if (expired) {
            // Remove the expired item under unique lock
            remove(key);  // remove handles unique locking
            return std::nullopt;
        }

        if (found) {
            // Update LRU list under unique lock
            UniqueLock uniqueLock(cacheMutex_);
            // Check if key still exists (might have been removed between locks)
            if (lastAccessTimes_.count(key)) {
                lastAccessTimes_[key] = std::chrono::steady_clock::now();
                lruList_.remove(key);  // O(n) for std::list, consider
                                       // alternatives if performance critical
                lruList_.push_front(key);
            } else {
                // Item was removed between locks, treat as miss
                // Decrement hit count? Or just return nullopt?
                return std::nullopt;
            }
            return value;  // Return the copied value
        }

        // Should not be reached if logic is correct
        return std::nullopt;

    } catch (const std::exception &e) {
        LOG_F(ERROR, "Get failed for key {}: {}", key.c_str(), e.what());
        return std::nullopt;
    }
}

template <Cacheable T>
void ResourceCache<T>::remove(const String &key) {
    try {
        UniqueLock lock(cacheMutex_);
        // Use erase for map-like types
        size_t erasedCount = cache_.erase(key);
        expirationTimes_.erase(key);
        lastAccessTimes_.erase(key);
        // Only remove from list and call callback if item was actually erased
        if (erasedCount > 0) {
            lruList_.remove(key);  // O(n) for std::list
            if (removeCallback_) {
                // Ensure callback is called outside the lock if it's
                // long-running
                removeCallback_(key);
            }
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Remove failed for key {}: {}", key.c_str(), e.what());
    }
}

template <Cacheable T>
void ResourceCache<T>::onInsert(Callback callback) {
    UniqueLock lock(cacheMutex_);  // Lock when modifying callback
    insertCallback_ = std::move(callback);
}

template <Cacheable T>
void ResourceCache<T>::onRemove(Callback callback) {
    UniqueLock lock(cacheMutex_);  // Lock when modifying callback
    removeCallback_ = std::move(callback);
}

template <Cacheable T>
std::pair<size_t, size_t> ResourceCache<T>::getStatistics() const {
    // Use load() for atomics
    return {hitCount_.load(), missCount_.load()};
}

template <Cacheable T>
auto ResourceCache<T>::asyncGet(const String &key) -> Future<std::optional<T>> {
    // Use std::async or boost::async depending on Future alias
    // Assuming std::async here based on default Future alias
    return std::async(std::launch::async,
                      [this, key]() -> std::optional<T> { return get(key); });
}

template <Cacheable T>
auto ResourceCache<T>::asyncInsert(const String &key, const T &value,
                                   std::chrono::seconds expirationTime)
    -> Future<void> {
    return std::async(std::launch::async, [this, key, value, expirationTime]() {
        insert(key, value, expirationTime);
    });
}

template <Cacheable T>
void ResourceCache<T>::clear() {
    UniqueLock lock(cacheMutex_);
    cache_.clear();
    expirationTimes_.clear();
    lastAccessTimes_.clear();
    lruList_.clear();
    // Reset stats?
    // hitCount_.store(0);
    // missCount_.store(0);
}

template <Cacheable T>
auto ResourceCache<T>::size() const -> size_t {
    SharedLock lock(cacheMutex_);
    return cache_.size();
}

template <Cacheable T>
auto ResourceCache<T>::empty() const -> bool {
    SharedLock lock(cacheMutex_);
    return cache_.empty();
}

template <Cacheable T>
void ResourceCache<T>::evict() {
    // This function should be called under a unique lock
    if (lruList_.empty()) {
        return;
    }
    // Get key from back of list
    String keyToEvict = lruList_.back();
    lruList_.pop_back();  // Remove from list

    // Remove from maps
    size_t erasedCount = cache_.erase(keyToEvict);
    expirationTimes_.erase(keyToEvict);
    lastAccessTimes_.erase(keyToEvict);

    if (erasedCount > 0 && removeCallback_) {
        // Call callback if needed (still under lock)
        removeCallback_(keyToEvict);
    }
    // Use c_str() for logging if needed
    LOG_F(INFO, "Evicted key: {}", keyToEvict.c_str());
}

template <Cacheable T>
void ResourceCache<T>::evictOldest() {
    // evictOldest is called from insert, which already holds a UniqueLock.
    // No need for an additional lock here.
    // UniqueLock lock(cacheMutex_); // Remove this redundant lock
    evict();
}

template <Cacheable T>
auto ResourceCache<T>::isExpired(const String &key) const -> bool {
    // This function might be called under shared or unique lock.
    // It only reads data.
    auto expIt = expirationTimes_.find(key);
    if (expIt == expirationTimes_.end()) {
        // No specific expiration time set, might depend on policy (never
        // expire?) Or maybe it should have been removed if not found here?
        // Let's assume if not in expirationTimes_, it's not expired by time.
        return false;
    }

    auto cacheIt = cache_.find(key);
    if (cacheIt == cache_.end()) {
        // Should not happen if key is in expirationTimes_
        // Log error or handle inconsistency
        LOG_F(
            ERROR,
            "Inconsistency: Key {} found in expirationTimes_ but not in cache_",
            key.c_str());
        return true;  // Treat as expired if inconsistent
    }

    // Check time difference
    return (std::chrono::steady_clock::now() - cacheIt->second.second) >=
           expIt->second;
}

template <Cacheable T>
auto ResourceCache<T>::asyncLoad(const String &key,
                                 std::function<T()> loadDataFunction)
    -> Future<void> {
    return std::async(std::launch::async, [this, key, loadDataFunction]() {
        try {
            T value = loadDataFunction();
            // Define a default expiration, e.g., 60 seconds
            insert(key, value, std::chrono::seconds(60));
        } catch (const std::exception &e) {
            LOG_F(ERROR, "Async load failed for key {}: {}", key.c_str(),
                  e.what());
        }
    });
}

template <Cacheable T>
void ResourceCache<T>::setMaxSize(int maxSize) {
    UniqueLock lock(cacheMutex_);
    if (maxSize > 0) {
        this->maxSize_ = maxSize;
        // Evict excess items if new max size is smaller
        while (cache_.size() > static_cast<size_t>(maxSize_)) {
            evict();
        }
    } else {
        LOG_F(WARNING, "Attempted to set invalid cache max size: {}", maxSize);
    }
}

template <Cacheable T>
void ResourceCache<T>::setExpirationTime(const String &key,
                                         std::chrono::seconds expirationTime) {
    UniqueLock lock(cacheMutex_);
    // Use find for map-like types
    if (cache_.find(key) != cache_.end()) {
        expirationTimes_[key] = expirationTime;
    }
}

template <Cacheable T>
void ResourceCache<T>::readFromFile(
    const String &filePath,
    const std::function<T(const String &)> &deserializer) {
    // Use c_str() for ifstream constructor if String is not std::string
    std::ifstream inputFile(filePath.c_str());
    if (inputFile.is_open()) {
        UniqueLock lock(cacheMutex_);
        std::string line;  // Use std::string for reading lines
        while (std::getline(inputFile, line)) {
            // Find separator in std::string
            auto separatorIndex = line.find(':');
            if (separatorIndex != std::string::npos) {
                // Convert std::string parts to atom::containers::String
                String key(line.substr(0, separatorIndex));
                String valueString(line.substr(separatorIndex + 1));
                try {
                    T value = deserializer(valueString);
                    // Check size before inserting
                    if (cache_.size() >= static_cast<size_t>(maxSize_)) {
                        evict();
                    }
                    // Check again after potential eviction
                    if (cache_.size() < static_cast<size_t>(maxSize_)) {
                        cache_[key] = {value, std::chrono::steady_clock::now()};
                        lastAccessTimes_[key] =
                            std::chrono::steady_clock::now();
                        // Set a default expiration? Or read from file? Assume
                        // default for now.
                        expirationTimes_[key] =
                            std::chrono::seconds(3600);  // e.g., 1 hour default
                        lruList_.remove(key);
                        lruList_.push_front(key);
                    } else {
                        LOG_F(WARNING,
                              "Cache full, could not insert key {} from file",
                              key.c_str());
                    }
                } catch (const std::exception &e) {
                    LOG_F(ERROR,
                          "Deserialization failed for key {} from file: {}",
                          key.c_str(), e.what());
                }
            }
        }
        inputFile.close();
    } else {
        LOG_F(ERROR, "Failed to open file for reading: {}", filePath.c_str());
    }
}

template <Cacheable T>
void ResourceCache<T>::writeToFile(
    const String &filePath,
    const std::function<String(const T &)> &serializer) {
    // Use c_str() for ofstream constructor
    std::ofstream outputFile(filePath.c_str());
    if (outputFile.is_open()) {
        SharedLock lock(cacheMutex_);
        for (const auto &pair : cache_) {
            try {
                // Ensure serializer output (String) and key (String) can be
                // written Need to convert atom::containers::String to something
                // std::ofstream understands Assuming String has c_str() or
                // data()
                String serializedValue = serializer(pair.second.first);
                // Use std::string for intermediate formatting if easier
                std::string line = std::string(pair.first.c_str()) + ":" +
                                   std::string(serializedValue.c_str()) + "\n";
                outputFile << line;
            } catch (const std::exception &e) {
                LOG_F(ERROR, "Serialization failed for key {}: {}",
                      pair.first.c_str(), e.what());
            }
        }
        outputFile.close();
    } else {
        LOG_F(ERROR, "Failed to open file for writing: {}", filePath.c_str());
    }
}

template <Cacheable T>
void ResourceCache<T>::removeExpired() {
    UniqueLock lock(cacheMutex_);
    Vector<String> expiredKeys;  // Use Vector<String>
    // Iterate safely, collecting keys to remove
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (isExpired(it->first)) {  // isExpired needs to be callable under
                                     // unique lock
            expiredKeys.push_back(it->first);
        }
    }

    // Remove collected keys
    for (const auto &key : expiredKeys) {
        cache_.erase(key);
        expirationTimes_.erase(key);
        lastAccessTimes_.erase(key);
        lruList_.remove(key);  // O(n) for std::list
        if (removeCallback_) {
            removeCallback_(key);
        }
        LOG_F(INFO, "Removed expired key: {}", key.c_str());
    }
}

template <Cacheable T>
void ResourceCache<T>::readFromJsonFile(
    const String &filePath, const std::function<T(const json &)> &fromJson) {
    // Use c_str() for ifstream
    std::ifstream inputFile(filePath.c_str());
    if (inputFile.is_open()) {
        UniqueLock lock(cacheMutex_);
        json jsonData;
        try {
            inputFile >> jsonData;
            inputFile.close();  // Close file after reading

            if (jsonData.is_object()) {
                for (auto it = jsonData.begin(); it != jsonData.end(); ++it) {
                    // Convert std::string key from json iterator to
                    // atom::containers::String
                    String key(it.key());
                    try {
                        T value = fromJson(it.value());
                        // Check size before inserting
                        if (cache_.size() >= static_cast<size_t>(maxSize_)) {
                            evict();
                        }
                        // Check again after potential eviction
                        if (cache_.size() < static_cast<size_t>(maxSize_)) {
                            cache_[key] = {value,
                                           std::chrono::steady_clock::now()};
                            lastAccessTimes_[key] =
                                std::chrono::steady_clock::now();
                            // Set a default expiration? Assume default for now.
                            expirationTimes_[key] = std::chrono::seconds(
                                3600);  // e.g., 1 hour default
                            lruList_.remove(key);
                            lruList_.push_front(key);
                        } else {
                            LOG_F(WARNING,
                                  "Cache full, could not insert key {} from "
                                  "JSON file",
                                  key.c_str());
                        }
                    } catch (const std::exception &e) {
                        LOG_F(ERROR,
                              "Deserialization failed for key {} from JSON "
                              "file: {}",
                              key.c_str(), e.what());
                    }
                }
            } else {
                LOG_F(ERROR, "JSON file does not contain a root object: {}",
                      filePath.c_str());
            }
        } catch (const json::parse_error &e) {
            LOG_F(ERROR, "Failed to parse JSON file {}: {}", filePath.c_str(),
                  e.what());
            inputFile.close();  // Ensure file is closed on error too
        } catch (const std::exception &e) {
            LOG_F(ERROR, "Error reading JSON file {}: {}", filePath.c_str(),
                  e.what());
            inputFile.close();
        }
    } else {
        LOG_F(ERROR, "Failed to open JSON file for reading: {}",
              filePath.c_str());
    }
}

template <Cacheable T>
void ResourceCache<T>::writeToJsonFile(
    const String &filePath, const std::function<json(const T &)> &toJson) {
    // Use c_str() for ofstream
    std::ofstream outputFile(filePath.c_str());
    if (outputFile.is_open()) {
        SharedLock lock(cacheMutex_);
        json jsonData = json::object();  // Initialize as JSON object
        for (const auto &pair : cache_) {
            try {
                // Convert atom::containers::String key to std::string for json
                // key
                jsonData[std::string(pair.first.c_str())] =
                    toJson(pair.second.first);
            } catch (const std::exception &e) {
                LOG_F(ERROR, "Serialization to JSON failed for key {}: {}",
                      pair.first.c_str(), e.what());
            }
        }
        try {
            outputFile << jsonData.dump(4);  // Pretty-print JSON
            outputFile.close();
        } catch (const std::exception &e) {
            LOG_F(ERROR, "Error writing JSON data to file {}: {}",
                  filePath.c_str(), e.what());
            outputFile.close();  // Ensure file is closed on error
        }
    } else {
        LOG_F(ERROR, "Failed to open JSON file for writing: {}",
              filePath.c_str());
    }
}

template <Cacheable T>
void ResourceCache<T>::cleanupExpiredEntries() {
    while (!stopCleanupThread_.load()) {
        // Use the calculated interval
        std::this_thread::sleep_for(cleanupInterval_);

        // Perform removal under unique lock
        Vector<String> expiredKeys;  // Use Vector<String>
        std::chrono::seconds nextInterval =
            std::chrono::seconds(5);  // Default interval
        {
            UniqueLock lock(cacheMutex_);
            // Collect expired keys
            for (auto it = cache_.begin(); it != cache_.end(); ++it) {
                if (isExpired(it->first)) {  // isExpired needs lock context
                    expiredKeys.push_back(it->first);
                }
            }

            // Remove collected keys
            for (const auto &key : expiredKeys) {
                cache_.erase(key);
                expirationTimes_.erase(key);
                lastAccessTimes_.erase(key);
                lruList_.remove(key);  // O(n) for std::list
                if (removeCallback_) {
                    removeCallback_(key);  // Called under lock
                }
                LOG_F(INFO, "Removed expired key: {}", key.c_str());
            }

            // Adjust cleanup interval adaptively (still under lock)
            size_t currentSize = cache_.size();
            if (currentSize > 0) {
                // Re-calculate expired count after removal if needed, or use
                // the count before removal Using expiredKeys.size() as the
                // count of expired items found in this run
                double density =
                    static_cast<double>(expiredKeys.size()) /
                    (currentSize +
                     expiredKeys.size());  // Density before removal
                if (density > 0.3) {
                    nextInterval = std::chrono::seconds(1);
                } else if (density < 0.1) {
                    nextInterval = std::chrono::seconds(5);
                } else {
                    nextInterval = std::chrono::seconds(3);
                }
            } else {
                nextInterval =
                    std::chrono::seconds(5);  // Default when cache is empty
            }
        }  // Unique lock released

        cleanupInterval_ = nextInterval;  // Update interval for next sleep
    }
}

// New method: Batch insert
template <Cacheable T>
void ResourceCache<T>::insertBatch(
    const Vector<std::pair<String, T>> &items,  // Use Vector<pair<String, T>>
    std::chrono::seconds expirationTime) {
    UniqueLock lock(cacheMutex_);
    for (const auto &[key, value] : items) {
        // Check size before inserting
        if (cache_.size() >= static_cast<size_t>(maxSize_)) {
            evict();  // evict is called under unique lock
        }
        // Check again after potential eviction
        if (cache_.size() < static_cast<size_t>(maxSize_)) {
            cache_[key] = {value, std::chrono::steady_clock::now()};
            expirationTimes_[key] = expirationTime;
            lastAccessTimes_[key] = std::chrono::steady_clock::now();
            lruList_.remove(key);  // Ensure no duplicates in list
            lruList_.push_front(key);
            if (insertCallback_) {
                insertCallback_(key);  // Called under lock
            }
        } else {
            LOG_F(WARNING,
                  "Cache full during batch insert, could not insert key {}",
                  key.c_str());
            // Optionally break or continue processing other items
        }
    }
}

// New method: Batch remove
template <Cacheable T>
void ResourceCache<T>::removeBatch(
    const Vector<String> &keys) {  // Use Vector<String>
    UniqueLock lock(cacheMutex_);
    for (const auto &key : keys) {
        size_t erasedCount = cache_.erase(key);
        expirationTimes_.erase(key);
        lastAccessTimes_.erase(key);
        if (erasedCount > 0) {
            lruList_.remove(key);  // O(n) for std::list
            if (removeCallback_) {
                removeCallback_(key);  // Called under lock
            }
        }
    }
}

}  // namespace atom::search

#endif  // ATOM_SEARCH_CACHE_HPP // Corrected endif guard
