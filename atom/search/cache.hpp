/*
 * cache.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file cache.hpp
 * @brief ResourceCache class for Atom Search
 * @date 2023-12-6
 */

#ifndef ATOM_SEARCH_CACHE_HPP
#define ATOM_SEARCH_CACHE_HPP

#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <future>
#include <list>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <utility>

#include "atom/containers/high_performance.hpp"

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

#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

using json = nlohmann::json;
using atom::containers::HashMap;
using atom::containers::String;
using atom::containers::Vector;

namespace atom::search {

#if defined(ATOM_USE_BOOST_THREAD)
using SharedMutex = boost::shared_mutex;
template <typename T>
using SharedLock = boost::shared_lock<T>;
template <typename T>
using UniqueLock = boost::unique_lock<T>;
template <typename... Args>
using Future = boost::future<Args...>;
template <typename... Args>
using Promise = boost::promise<Args...>;
using Thread = boost::thread;
using JThread = boost::thread;
#else
using SharedMutex = std::shared_mutex;
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

template <typename T>
class LockFreeQueue {
private:
    std::mutex mutex_;
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
 * This class provides a high-performance, thread-safe caching mechanism with
 * LRU eviction, automatic expiration cleanup, and support for both synchronous
 * and asynchronous operations.
 *
 * @tparam T The type of the resources to be cached. Must satisfy the Cacheable
 * concept.
 */
template <Cacheable T>
class ResourceCache {
public:
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
    void insert(const String &key, const T &value,
                std::chrono::seconds expirationTime);

    /**
     * @brief Checks if the cache contains a resource with the specified key.
     *
     * @param key The key to check.
     * @return True if the cache contains the resource, false otherwise.
     */
    auto contains(const String &key) const -> bool;

    /**
     * @brief Retrieves a resource from the cache.
     *
     * @param key The key associated with the resource.
     * @return An optional containing the resource if found, otherwise
     * std::nullopt.
     */
    auto get(const String &key) -> std::optional<T>;

    /**
     * @brief Removes a resource from the cache.
     *
     * @param key The key associated with the resource to be removed.
     */
    void remove(const String &key);

    /**
     * @brief Asynchronously retrieves a resource from the cache.
     *
     * @param key The key associated with the resource.
     * @return A future containing an optional with the resource if found,
     * otherwise std::nullopt.
     */
    auto asyncGet(const String &key) -> Future<std::optional<T>>;

    /**
     * @brief Asynchronously inserts a resource into the cache with an
     * expiration time.
     *
     * @param key The key associated with the resource.
     * @param value The resource to be cached.
     * @param expirationTime The time after which the resource expires.
     * @return A future that completes when the insertion is done.
     */
    auto asyncInsert(const String &key, const T &value,
                     std::chrono::seconds expirationTime) -> Future<void>;

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
    auto isExpired(const String &key) const -> bool;

    /**
     * @brief Asynchronously loads a resource into the cache using a provided
     * function.
     *
     * @param key The key associated with the resource.
     * @param loadDataFunction The function to load the resource.
     * @return A future that completes when the resource is loaded.
     */
    auto asyncLoad(const String &key, std::function<T()> loadDataFunction)
        -> Future<void>;

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
    void setExpirationTime(const String &key,
                           std::chrono::seconds expirationTime);

    /**
     * @brief Reads resources from a file and inserts them into the cache.
     *
     * @param filePath The path to the file.
     * @param deserializer The function to deserialize the resources.
     */
    void readFromFile(const String &filePath,
                      const std::function<T(const String &)> &deserializer);

    /**
     * @brief Writes the resources in the cache to a file.
     *
     * @param filePath The path to the file.
     * @param serializer The function to serialize the resources.
     */
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
    void readFromJsonFile(const String &filePath,
                          const std::function<T(const json &)> &fromJson);

    /**
     * @brief Writes the resources in the cache to a JSON file.
     *
     * @param filePath The path to the JSON file.
     * @param toJson The function to serialize the resources to JSON.
     */
    void writeToJsonFile(const String &filePath,
                         const std::function<json(const T &)> &toJson);

    /**
     * @brief Inserts multiple resources into the cache with an expiration time.
     *
     * @param items The vector of key-value pairs to insert.
     * @param expirationTime The time after which the resources expire.
     */
    void insertBatch(const Vector<std::pair<String, T>> &items,
                     std::chrono::seconds expirationTime);

    /**
     * @brief Removes multiple resources from the cache.
     *
     * @param keys The vector of keys associated with the resources to remove.
     */
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
    void evict();
    void cleanupExpiredEntries();

    HashMap<String, std::pair<T, std::chrono::steady_clock::time_point>> cache_;
    int maxSize_;
    HashMap<String, std::chrono::seconds> expirationTimes_;
    HashMap<String, std::chrono::steady_clock::time_point> lastAccessTimes_;
    std::list<String> lruList_;
    mutable SharedMutex cacheMutex_;
    JThread cleanupThread_;
    Atomic<bool> stopCleanupThread_{false};
    Callback insertCallback_;
    Callback removeCallback_;
    mutable Atomic<size_t> hitCount_{0};
    mutable Atomic<size_t> missCount_{0};
    std::chrono::seconds cleanupInterval_{1};
};

template <Cacheable T>
ResourceCache<T>::ResourceCache(int maxSize) : maxSize_(maxSize) {
    cleanupThread_ = JThread([this] { cleanupExpiredEntries(); });
}

template <Cacheable T>
ResourceCache<T>::~ResourceCache() {
    stopCleanupThread_.store(true);
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
}

template <Cacheable T>
void ResourceCache<T>::insert(const String &key, const T &value,
                              std::chrono::seconds expirationTime) {
    try {
        UniqueLock lock(cacheMutex_);
        if (cache_.size() >= static_cast<size_t>(maxSize_)) {
            evictOldest();
        }

        if (cache_.size() >= static_cast<size_t>(maxSize_)) {
            spdlog::warn("Cache still full after eviction attempt for key {}",
                         key.c_str());
            return;
        }

        cache_[key] = {value, std::chrono::steady_clock::now()};
        expirationTimes_[key] = expirationTime;
        lastAccessTimes_[key] = std::chrono::steady_clock::now();
        lruList_.remove(key);
        lruList_.push_front(key);

        if (insertCallback_) {
            insertCallback_(key);
        }
    } catch (const std::exception &e) {
        spdlog::error("Insert failed for key {}: {}", key.c_str(), e.what());
    }
}

template <Cacheable T>
auto ResourceCache<T>::contains(const String &key) const -> bool {
    SharedLock lock(cacheMutex_);
    return cache_.find(key) != cache_.end();
}

template <Cacheable T>
auto ResourceCache<T>::get(const String &key) -> std::optional<T> {
    try {
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

            auto expIt = expirationTimes_.find(key);
            if (expIt != expirationTimes_.end()) {
                if ((std::chrono::steady_clock::now() - it->second.second) >=
                    expIt->second) {
                    expired = true;
                }
            }

            if (expired) {
                missCount_++;
            } else {
                value = it->second.first;
                found = true;
                hitCount_++;
            }
        }

        if (expired) {
            remove(key);
            return std::nullopt;
        }

        if (found) {
            UniqueLock uniqueLock(cacheMutex_);
            if (lastAccessTimes_.count(key)) {
                lastAccessTimes_[key] = std::chrono::steady_clock::now();
                lruList_.remove(key);
                lruList_.push_front(key);
            } else {
                return std::nullopt;
            }
            return value;
        }

        return std::nullopt;
    } catch (const std::exception &e) {
        spdlog::error("Get failed for key {}: {}", key.c_str(), e.what());
        return std::nullopt;
    }
}

template <Cacheable T>
void ResourceCache<T>::remove(const String &key) {
    try {
        UniqueLock lock(cacheMutex_);
        size_t erasedCount = cache_.erase(key);
        expirationTimes_.erase(key);
        lastAccessTimes_.erase(key);

        if (erasedCount > 0) {
            lruList_.remove(key);
            if (removeCallback_) {
                removeCallback_(key);
            }
        }
    } catch (const std::exception &e) {
        spdlog::error("Remove failed for key {}: {}", key.c_str(), e.what());
    }
}

template <Cacheable T>
void ResourceCache<T>::onInsert(Callback callback) {
    UniqueLock lock(cacheMutex_);
    insertCallback_ = std::move(callback);
}

template <Cacheable T>
void ResourceCache<T>::onRemove(Callback callback) {
    UniqueLock lock(cacheMutex_);
    removeCallback_ = std::move(callback);
}

template <Cacheable T>
std::pair<size_t, size_t> ResourceCache<T>::getStatistics() const {
    return {hitCount_.load(), missCount_.load()};
}

template <Cacheable T>
auto ResourceCache<T>::asyncGet(const String &key) -> Future<std::optional<T>> {
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
    if (lruList_.empty()) {
        return;
    }

    String keyToEvict = lruList_.back();
    lruList_.pop_back();

    size_t erasedCount = cache_.erase(keyToEvict);
    expirationTimes_.erase(keyToEvict);
    lastAccessTimes_.erase(keyToEvict);

    if (erasedCount > 0 && removeCallback_) {
        removeCallback_(keyToEvict);
    }

    spdlog::info("Evicted key: {}", keyToEvict.c_str());
}

template <Cacheable T>
void ResourceCache<T>::evictOldest() {
    evict();
}

template <Cacheable T>
auto ResourceCache<T>::isExpired(const String &key) const -> bool {
    auto expIt = expirationTimes_.find(key);
    if (expIt == expirationTimes_.end()) {
        return false;
    }

    auto cacheIt = cache_.find(key);
    if (cacheIt == cache_.end()) {
        spdlog::error(
            "Inconsistency: Key {} found in expirationTimes_ but not in cache_",
            key.c_str());
        return true;
    }

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
            insert(key, value, std::chrono::seconds(60));
        } catch (const std::exception &e) {
            spdlog::error("Async load failed for key {}: {}", key.c_str(),
                          e.what());
        }
    });
}

template <Cacheable T>
void ResourceCache<T>::setMaxSize(int maxSize) {
    UniqueLock lock(cacheMutex_);
    if (maxSize > 0) {
        this->maxSize_ = maxSize;
        while (cache_.size() > static_cast<size_t>(maxSize_)) {
            evict();
        }
    } else {
        spdlog::warn("Attempted to set invalid cache max size: {}", maxSize);
    }
}

template <Cacheable T>
void ResourceCache<T>::setExpirationTime(const String &key,
                                         std::chrono::seconds expirationTime) {
    UniqueLock lock(cacheMutex_);
    if (cache_.find(key) != cache_.end()) {
        expirationTimes_[key] = expirationTime;
    }
}

template <Cacheable T>
void ResourceCache<T>::readFromFile(
    const String &filePath,
    const std::function<T(const String &)> &deserializer) {
    std::ifstream inputFile(filePath.c_str());
    if (inputFile.is_open()) {
        UniqueLock lock(cacheMutex_);
        std::string line;
        while (std::getline(inputFile, line)) {
            auto separatorIndex = line.find(':');
            if (separatorIndex != std::string::npos) {
                String key(line.substr(0, separatorIndex));
                String valueString(line.substr(separatorIndex + 1));
                try {
                    T value = deserializer(valueString);
                    if (cache_.size() >= static_cast<size_t>(maxSize_)) {
                        evict();
                    }
                    if (cache_.size() < static_cast<size_t>(maxSize_)) {
                        cache_[key] = {value, std::chrono::steady_clock::now()};
                        lastAccessTimes_[key] =
                            std::chrono::steady_clock::now();
                        expirationTimes_[key] = std::chrono::seconds(3600);
                        lruList_.remove(key);
                        lruList_.push_front(key);
                    } else {
                        spdlog::warn(
                            "Cache full, could not insert key {} from file",
                            key.c_str());
                    }
                } catch (const std::exception &e) {
                    spdlog::error(
                        "Deserialization failed for key {} from file: {}",
                        key.c_str(), e.what());
                }
            }
        }
        inputFile.close();
    } else {
        spdlog::error("Failed to open file for reading: {}", filePath.c_str());
    }
}

template <Cacheable T>
void ResourceCache<T>::writeToFile(
    const String &filePath,
    const std::function<String(const T &)> &serializer) {
    std::ofstream outputFile(filePath.c_str());
    if (outputFile.is_open()) {
        SharedLock lock(cacheMutex_);
        for (const auto &pair : cache_) {
            try {
                String serializedValue = serializer(pair.second.first);
                std::string line = std::string(pair.first.c_str()) + ":" +
                                   std::string(serializedValue.c_str()) + "\n";
                outputFile << line;
            } catch (const std::exception &e) {
                spdlog::error("Serialization failed for key {}: {}",
                              pair.first.c_str(), e.what());
            }
        }
        outputFile.close();
    } else {
        spdlog::error("Failed to open file for writing: {}", filePath.c_str());
    }
}

template <Cacheable T>
void ResourceCache<T>::removeExpired() {
    UniqueLock lock(cacheMutex_);
    Vector<String> expiredKeys;

    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (isExpired(it->first)) {
            expiredKeys.push_back(it->first);
        }
    }

    for (const auto &key : expiredKeys) {
        cache_.erase(key);
        expirationTimes_.erase(key);
        lastAccessTimes_.erase(key);
        lruList_.remove(key);
        if (removeCallback_) {
            removeCallback_(key);
        }
        spdlog::info("Removed expired key: {}", key.c_str());
    }
}

template <Cacheable T>
void ResourceCache<T>::readFromJsonFile(
    const String &filePath, const std::function<T(const json &)> &fromJson) {
    std::ifstream inputFile(filePath.c_str());
    if (inputFile.is_open()) {
        UniqueLock lock(cacheMutex_);
        json jsonData;
        try {
            inputFile >> jsonData;
            inputFile.close();

            if (jsonData.is_object()) {
                for (auto it = jsonData.begin(); it != jsonData.end(); ++it) {
                    String key(it.key());
                    try {
                        T value = fromJson(it.value());
                        if (cache_.size() >= static_cast<size_t>(maxSize_)) {
                            evict();
                        }
                        if (cache_.size() < static_cast<size_t>(maxSize_)) {
                            cache_[key] = {value,
                                           std::chrono::steady_clock::now()};
                            lastAccessTimes_[key] =
                                std::chrono::steady_clock::now();
                            expirationTimes_[key] = std::chrono::seconds(3600);
                            lruList_.remove(key);
                            lruList_.push_front(key);
                        } else {
                            spdlog::warn(
                                "Cache full, could not insert key {} from JSON "
                                "file",
                                key.c_str());
                        }
                    } catch (const std::exception &e) {
                        spdlog::error(
                            "Deserialization failed for key {} from JSON file: "
                            "{}",
                            key.c_str(), e.what());
                    }
                }
            } else {
                spdlog::error("JSON file does not contain a root object: {}",
                              filePath.c_str());
            }
        } catch (const json::parse_error &e) {
            spdlog::error("Failed to parse JSON file {}: {}", filePath.c_str(),
                          e.what());
            inputFile.close();
        } catch (const std::exception &e) {
            spdlog::error("Error reading JSON file {}: {}", filePath.c_str(),
                          e.what());
            inputFile.close();
        }
    } else {
        spdlog::error("Failed to open JSON file for reading: {}",
                      filePath.c_str());
    }
}

template <Cacheable T>
void ResourceCache<T>::writeToJsonFile(
    const String &filePath, const std::function<json(const T &)> &toJson) {
    std::ofstream outputFile(filePath.c_str());
    if (outputFile.is_open()) {
        SharedLock lock(cacheMutex_);
        json jsonData = json::object();
        for (const auto &pair : cache_) {
            try {
                jsonData[std::string(pair.first.c_str())] =
                    toJson(pair.second.first);
            } catch (const std::exception &e) {
                spdlog::error("Serialization to JSON failed for key {}: {}",
                              pair.first.c_str(), e.what());
            }
        }
        try {
            outputFile << jsonData.dump(4);
            outputFile.close();
        } catch (const std::exception &e) {
            spdlog::error("Error writing JSON data to file {}: {}",
                          filePath.c_str(), e.what());
            outputFile.close();
        }
    } else {
        spdlog::error("Failed to open JSON file for writing: {}",
                      filePath.c_str());
    }
}

template <Cacheable T>
void ResourceCache<T>::cleanupExpiredEntries() {
    while (!stopCleanupThread_.load()) {
        std::this_thread::sleep_for(cleanupInterval_);

        Vector<String> expiredKeys;
        std::chrono::seconds nextInterval = std::chrono::seconds(5);

        {
            UniqueLock lock(cacheMutex_);
            for (auto it = cache_.begin(); it != cache_.end(); ++it) {
                if (isExpired(it->first)) {
                    expiredKeys.push_back(it->first);
                }
            }

            for (const auto &key : expiredKeys) {
                cache_.erase(key);
                expirationTimes_.erase(key);
                lastAccessTimes_.erase(key);
                lruList_.remove(key);
                if (removeCallback_) {
                    removeCallback_(key);
                }
                spdlog::info("Removed expired key: {}", key.c_str());
            }

            size_t currentSize = cache_.size();
            if (currentSize > 0) {
                double density = static_cast<double>(expiredKeys.size()) /
                                 (currentSize + expiredKeys.size());
                if (density > 0.3) {
                    nextInterval = std::chrono::seconds(1);
                } else if (density < 0.1) {
                    nextInterval = std::chrono::seconds(5);
                } else {
                    nextInterval = std::chrono::seconds(3);
                }
            } else {
                nextInterval = std::chrono::seconds(5);
            }
        }

        cleanupInterval_ = nextInterval;
    }
}

template <Cacheable T>
void ResourceCache<T>::insertBatch(const Vector<std::pair<String, T>> &items,
                                   std::chrono::seconds expirationTime) {
    UniqueLock lock(cacheMutex_);
    for (const auto &[key, value] : items) {
        if (cache_.size() >= static_cast<size_t>(maxSize_)) {
            evict();
        }
        if (cache_.size() < static_cast<size_t>(maxSize_)) {
            cache_[key] = {value, std::chrono::steady_clock::now()};
            expirationTimes_[key] = expirationTime;
            lastAccessTimes_[key] = std::chrono::steady_clock::now();
            lruList_.remove(key);
            lruList_.push_front(key);
            if (insertCallback_) {
                insertCallback_(key);
            }
        } else {
            spdlog::warn(
                "Cache full during batch insert, could not insert key {}",
                key.c_str());
        }
    }
}

template <Cacheable T>
void ResourceCache<T>::removeBatch(const Vector<String> &keys) {
    UniqueLock lock(cacheMutex_);
    for (const auto &key : keys) {
        size_t erasedCount = cache_.erase(key);
        expirationTimes_.erase(key);
        lastAccessTimes_.erase(key);
        if (erasedCount > 0) {
            lruList_.remove(key);
            if (removeCallback_) {
                removeCallback_(key);
            }
        }
    }
}

}  // namespace atom::search

#endif  // ATOM_SEARCH_CACHE_HPP
