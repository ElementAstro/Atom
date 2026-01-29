/*
 * resource_cache.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file resource_cache.hpp
 * @brief ResourceCache class - a thread-safe cache with LRU eviction and TTL.
 * @details This file provides a thread-safe resource cache with LRU eviction,
 *          automatic expiration cleanup, and support for both synchronous and
 *          asynchronous operations.
 */

#ifndef ATOM_SEARCH_CACHE_RESOURCE_CACHE_HPP
#define ATOM_SEARCH_CACHE_RESOURCE_CACHE_HPP

#include <fstream>
#include <list>
#include <utility>

#include "exceptions.hpp"
#include "types.hpp"

#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

namespace atom::search::cache {

using json = nlohmann::json;

/**
 * @brief A thread-safe cache for storing and managing resources with
 * expiration.
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
    using Callback = std::function<void(const String& key)>;

    /**
     * @brief Constructs a ResourceCache with a specified maximum size.
     * @param maxSize The maximum number of items the cache can hold.
     */
    explicit ResourceCache(int maxSize);

    /**
     * @brief Destructs the ResourceCache and stops the cleanup thread.
     */
    ~ResourceCache();

    ResourceCache(const ResourceCache&) = delete;
    ResourceCache& operator=(const ResourceCache&) = delete;
    ResourceCache(ResourceCache&&) = default;
    ResourceCache& operator=(ResourceCache&&) = default;

    // Core operations
    void insert(const String& key, const T& value,
                std::chrono::seconds expirationTime);
    [[nodiscard]] bool contains(const String& key) const;
    [[nodiscard]] std::optional<T> get(const String& key);
    void remove(const String& key);
    void clear();

    // Async operations
    [[nodiscard]] Future<std::optional<T>> asyncGet(const String& key);
    [[nodiscard]] Future<void> asyncInsert(const String& key, const T& value,
                                           std::chrono::seconds expirationTime);
    [[nodiscard]] Future<void> asyncLoad(const String& key,
                                         std::function<T()> loadDataFunction);

    // Size and capacity
    [[nodiscard]] size_t size() const;
    [[nodiscard]] bool empty() const;
    void setMaxSize(int maxSize);

    // Expiration
    void evictOldest();
    [[nodiscard]] bool isExpired(const String& key) const;
    void setExpirationTime(const String& key,
                           std::chrono::seconds expirationTime);
    void removeExpired();

    // Persistence
    void readFromFile(const String& filePath,
                      const std::function<T(const String&)>& deserializer);
    void writeToFile(const String& filePath,
                     const std::function<String(const T&)>& serializer);
    void readFromJsonFile(const String& filePath,
                          const std::function<T(const json&)>& fromJson);
    void writeToJsonFile(const String& filePath,
                         const std::function<json(const T&)>& toJson);

    // Batch operations
    void insertBatch(const Vector<std::pair<String, T>>& items,
                     std::chrono::seconds expirationTime);
    void removeBatch(const Vector<String>& keys);

    // Callbacks
    void onInsert(Callback callback);
    void onRemove(Callback callback);

    // Statistics
    [[nodiscard]] std::pair<size_t, size_t> getStatistics() const;

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

// =============================================================================
// Implementation
// =============================================================================

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
void ResourceCache<T>::insert(const String& key, const T& value,
                              std::chrono::seconds expirationTime) {
    try {
        UniqueLock<SharedMutex> lock(cacheMutex_);
        if (cache_.size() >= static_cast<size_t>(maxSize_)) {
            evict();
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
    } catch (const std::exception& e) {
        spdlog::error("Insert failed for key {}: {}", key.c_str(), e.what());
    }
}

template <Cacheable T>
bool ResourceCache<T>::contains(const String& key) const {
    auto now = std::chrono::steady_clock::now();
    SharedLock<SharedMutex> lock(cacheMutex_);
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return false;
    }
    auto expIt = expirationTimes_.find(key);
    if (expIt != expirationTimes_.end()) {
        if ((now - it->second.second) >= expIt->second) {
            lock.unlock();
            const_cast<ResourceCache<T>*>(this)->remove(key);
            return false;
        }
    }
    return true;
}

template <Cacheable T>
std::optional<T> ResourceCache<T>::get(const String& key) {
    try {
        T value;
        bool found = false;
        bool expired = false;

        {
            SharedLock<SharedMutex> lock(cacheMutex_);
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
            UniqueLock<SharedMutex> uniqueLock(cacheMutex_);
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
    } catch (const std::exception& e) {
        spdlog::error("Get failed for key {}: {}", key.c_str(), e.what());
        return std::nullopt;
    }
}

template <Cacheable T>
void ResourceCache<T>::remove(const String& key) {
    try {
        UniqueLock<SharedMutex> lock(cacheMutex_);
        size_t erasedCount = cache_.erase(key);
        expirationTimes_.erase(key);
        lastAccessTimes_.erase(key);

        if (erasedCount > 0) {
            lruList_.remove(key);
            if (removeCallback_) {
                removeCallback_(key);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Remove failed for key {}: {}", key.c_str(), e.what());
    }
}

template <Cacheable T>
void ResourceCache<T>::onInsert(Callback callback) {
    UniqueLock<SharedMutex> lock(cacheMutex_);
    insertCallback_ = std::move(callback);
}

template <Cacheable T>
void ResourceCache<T>::onRemove(Callback callback) {
    UniqueLock<SharedMutex> lock(cacheMutex_);
    removeCallback_ = std::move(callback);
}

template <Cacheable T>
std::pair<size_t, size_t> ResourceCache<T>::getStatistics() const {
    return {hitCount_.load(), missCount_.load()};
}

template <Cacheable T>
Future<std::optional<T>> ResourceCache<T>::asyncGet(const String& key) {
    return std::async(std::launch::async,
                      [this, key]() -> std::optional<T> { return get(key); });
}

template <Cacheable T>
Future<void> ResourceCache<T>::asyncInsert(
    const String& key, const T& value, std::chrono::seconds expirationTime) {
    return std::async(std::launch::async, [this, key, value, expirationTime]() {
        insert(key, value, expirationTime);
    });
}

template <Cacheable T>
void ResourceCache<T>::clear() {
    UniqueLock<SharedMutex> lock(cacheMutex_);
    cache_.clear();
    expirationTimes_.clear();
    lastAccessTimes_.clear();
    lruList_.clear();
}

template <Cacheable T>
size_t ResourceCache<T>::size() const {
    SharedLock<SharedMutex> lock(cacheMutex_);
    return cache_.size();
}

template <Cacheable T>
bool ResourceCache<T>::empty() const {
    SharedLock<SharedMutex> lock(cacheMutex_);
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

    spdlog::debug("Evicted key: {}", keyToEvict.c_str());
}

template <Cacheable T>
void ResourceCache<T>::evictOldest() {
    UniqueLock<SharedMutex> lock(cacheMutex_);
    evict();
}

template <Cacheable T>
bool ResourceCache<T>::isExpired(const String& key) const {
    auto expIt = expirationTimes_.find(key);
    if (expIt == expirationTimes_.end()) {
        return false;
    }

    auto cacheIt = cache_.find(key);
    if (cacheIt == cache_.end()) {
        return true;
    }

    return (std::chrono::steady_clock::now() - cacheIt->second.second) >=
           expIt->second;
}

template <Cacheable T>
Future<void> ResourceCache<T>::asyncLoad(const String& key,
                                         std::function<T()> loadDataFunction) {
    return std::async(std::launch::async, [this, key, loadDataFunction]() {
        try {
            T value = loadDataFunction();
            insert(key, value, std::chrono::seconds(60));
        } catch (const std::exception& e) {
            spdlog::error("Async load failed for key {}: {}", key.c_str(),
                          e.what());
        }
    });
}

template <Cacheable T>
void ResourceCache<T>::setMaxSize(int maxSize) {
    UniqueLock<SharedMutex> lock(cacheMutex_);
    if (maxSize > 0) {
        maxSize_ = maxSize;
        while (cache_.size() > static_cast<size_t>(maxSize_)) {
            evict();
        }
    } else {
        spdlog::warn("Attempted to set invalid cache max size: {}", maxSize);
    }
}

template <Cacheable T>
void ResourceCache<T>::setExpirationTime(const String& key,
                                         std::chrono::seconds expirationTime) {
    UniqueLock<SharedMutex> lock(cacheMutex_);
    if (cache_.find(key) != cache_.end()) {
        expirationTimes_[key] = expirationTime;
    }
}

template <Cacheable T>
void ResourceCache<T>::readFromFile(
    const String& filePath,
    const std::function<T(const String&)>& deserializer) {
    std::ifstream inputFile(filePath.c_str());
    if (inputFile.is_open()) {
        UniqueLock<SharedMutex> lock(cacheMutex_);
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
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Deserialization failed for key {}: {}",
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
    const String& filePath, const std::function<String(const T&)>& serializer) {
    std::ofstream outputFile(filePath.c_str());
    if (outputFile.is_open()) {
        SharedLock<SharedMutex> lock(cacheMutex_);
        for (const auto& pair : cache_) {
            try {
                String serializedValue = serializer(pair.second.first);
                outputFile << pair.first.c_str() << ":"
                           << serializedValue.c_str() << "\n";
            } catch (const std::exception& e) {
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
    UniqueLock<SharedMutex> lock(cacheMutex_);
    Vector<String> expiredKeys;

    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (isExpired(it->first)) {
            expiredKeys.push_back(it->first);
        }
    }

    for (const auto& key : expiredKeys) {
        cache_.erase(key);
        expirationTimes_.erase(key);
        lastAccessTimes_.erase(key);
        lruList_.remove(key);
        if (removeCallback_) {
            removeCallback_(key);
        }
        spdlog::debug("Removed expired key: {}", key.c_str());
    }
}

template <Cacheable T>
void ResourceCache<T>::readFromJsonFile(
    const String& filePath, const std::function<T(const json&)>& fromJson) {
    std::ifstream inputFile(filePath.c_str());
    if (inputFile.is_open()) {
        UniqueLock<SharedMutex> lock(cacheMutex_);
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
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Deserialization failed for key {}: {}",
                                      key.c_str(), e.what());
                    }
                }
            }
        } catch (const json::parse_error& e) {
            spdlog::error("Failed to parse JSON file {}: {}", filePath.c_str(),
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
    const String& filePath, const std::function<json(const T&)>& toJson) {
    std::ofstream outputFile(filePath.c_str());
    if (outputFile.is_open()) {
        SharedLock<SharedMutex> lock(cacheMutex_);
        json jsonData = json::object();
        for (const auto& pair : cache_) {
            try {
                jsonData[std::string(pair.first.c_str())] =
                    toJson(pair.second.first);
            } catch (const std::exception& e) {
                spdlog::error("Serialization to JSON failed for key {}: {}",
                              pair.first.c_str(), e.what());
            }
        }
        try {
            outputFile << jsonData.dump(4);
            outputFile.close();
        } catch (const std::exception& e) {
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
            UniqueLock<SharedMutex> lock(cacheMutex_);
            for (auto it = cache_.begin(); it != cache_.end(); ++it) {
                if (isExpired(it->first)) {
                    expiredKeys.push_back(it->first);
                }
            }

            for (const auto& key : expiredKeys) {
                cache_.erase(key);
                expirationTimes_.erase(key);
                lastAccessTimes_.erase(key);
                lruList_.remove(key);
                if (removeCallback_) {
                    removeCallback_(key);
                }
                spdlog::debug("Removed expired key: {}", key.c_str());
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
            }
        }

        cleanupInterval_ = nextInterval;
    }
}

template <Cacheable T>
void ResourceCache<T>::insertBatch(const Vector<std::pair<String, T>>& items,
                                   std::chrono::seconds expirationTime) {
    UniqueLock<SharedMutex> lock(cacheMutex_);
    for (const auto& [key, value] : items) {
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
        }
    }
}

template <Cacheable T>
void ResourceCache<T>::removeBatch(const Vector<String>& keys) {
    UniqueLock<SharedMutex> lock(cacheMutex_);
    for (const auto& key : keys) {
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

}  // namespace atom::search::cache

// Expose in atom::search namespace for backward compatibility
namespace atom::search {
using cache::ResourceCache;
}  // namespace atom::search

#endif  // ATOM_SEARCH_CACHE_RESOURCE_CACHE_HPP
