/**
 * @file cache.hpp
 * @brief A high-performance, thread-safe, sharded resource cache for Atom Search.
 * @date 2025-07-16
 */

#ifndef ATOM_SEARCH_CACHE_HPP
#define ATOM_SEARCH_CACHE_HPP

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <concepts>
#include <fstream>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "atom/containers/high_performance.hpp"
#include "atom/type/json.hpp"

namespace atom::search {

using json = nlohmann::json;
using atom::containers::HashMap;
using atom::containers::String;
using atom::containers::Vector;

/**
 * @brief Concept for types that can be stored in the ResourceCache.
 * @details Ensures that the type is both copy-constructible and copy-assignable.
 */
template <typename T>
concept Cacheable = std::copy_constructible<T> && std::is_copy_assignable_v<T>;

/**
 * @brief A high-performance, thread-safe, sharded cache for storing and managing
 * resources with expiration times.
 *
 * This class provides a highly concurrent caching mechanism with an LRU eviction
 * policy. It achieves scalability by partitioning the cache into multiple shards,
 * each with its own lock, minimizing contention on multi-core systems. It
 * features automatic expiration cleanup and supports both synchronous and
 * asynchronous operations.
 *
 * @tparam T The type of the resources to be cached. Must satisfy the Cacheable
 * concept.
 */
template <Cacheable T>
class ResourceCache {
public:
    using Callback = std::function<void(const String& key)>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::seconds;

    /**
     * @brief Constructs a ResourceCache.
     *
     * @param max_size The maximum number of items the cache can hold across all
     * shards.
     * @param cleanup_interval The interval at which the cleanup thread checks
     * for expired items.
     */
    explicit ResourceCache(size_t max_size,
                           Duration cleanup_interval = Duration(5));

    /**
     * @brief Destructs the ResourceCache, stopping the background cleanup
     * thread.
     */
    ~ResourceCache();

    ResourceCache(const ResourceCache&) = delete;
    ResourceCache& operator=(const ResourceCache&) = delete;
    ResourceCache(ResourceCache&&) = delete;
    ResourceCache& operator=(ResourceCache&&) = delete;

    /**
     * @brief Inserts a resource into the cache with an expiration time.
     *
     * @param key The key associated with the resource.
     * @param value The resource to be cached.
     * @param expiration_time The duration after which the resource expires.
     */
    void insert(const String& key, const T& value, Duration expiration_time);

    /**
     * @brief Checks if the cache contains a resource with the specified key.
     *
     * @param key The key to check.
     * @return True if the cache contains a non-expired resource, false
     * otherwise.
     */
    [[nodiscard]] auto contains(const String& key) const -> bool;

    /**
     * @brief Retrieves a resource from the cache.
     *
     * @param key The key associated with the resource.
     * @return An optional containing the resource if found and not expired,
     * otherwise std::nullopt.
     */
    [[nodiscard]] auto get(const String& key) -> std::optional<T>;

    /**
     * @brief Removes a resource from the cache.
     *
     * @param key The key associated with the resource to be removed.
     */
    void remove(const String& key);

    /**
     * @brief Asynchronously retrieves a resource from the cache.
     *
     * @param key The key associated with the resource.
     * @return A future containing an optional with the resource if found,
     * otherwise std::nullopt.
     */
    [[nodiscard]] auto async_get(const String& key) -> std::future<std::optional<T>>;

    /**
     * @brief Asynchronously inserts a resource into the cache.
     *
     * @param key The key associated with the resource.
     * @param value The resource to be cached.
     * @param expiration_time The time after which the resource expires.
     * @return A future that completes when the insertion is done.
     */
    auto async_insert(const String& key, const T& value, Duration expiration_time)
        -> std::future<void>;

    /**
     * @brief Clears all resources from the cache.
     */
    void clear();

    /**
     * @brief Gets the approximate number of resources in the cache.
     *
     * @return The number of resources currently in the cache.
     */
    [[nodiscard]] auto size() const -> size_t;

    /**
     * @brief Checks if the cache is empty.
     *
     * @return True if the cache is empty, false otherwise.
     */
    [[nodiscard]] auto empty() const -> bool;

    /**
     * @brief Sets the maximum size of the cache.
     * @details This will re-distribute the capacity among shards and may cause
     * evictions.
     * @param new_max_size The new maximum size of the cache.
     */
    void set_max_size(size_t new_max_size);

    /**
     * @brief Reads resources from a JSON file and inserts them into the cache.
     *
     * @param file_path The path to the JSON file.
     * @param from_json A function to deserialize a resource from a JSON object.
     * @param expiration_time The expiration time to apply to all loaded items.
     */
    void read_from_json_file(const String& file_path,
                             const std::function<T(const json&)>& from_json,
                             Duration expiration_time);

    /**
     * @brief Writes the resources in the cache to a JSON file.
     *
     * @param file_path The path to the JSON file.
     * @param to_json A function to serialize a resource to a JSON object.
     */
    void write_to_json_file(const String& file_path,
                            const std::function<json(const T&)>& to_json) const;

    /**
     * @brief Inserts multiple resources into the cache.
     *
     * @param items The vector of key-value pairs to insert.
     * @param expiration_time The time after which the resources expire.
     */
    void insert_batch(const Vector<std::pair<String, T>>& items,
                      Duration expiration_time);

    /**
     * @brief Removes multiple resources from the cache.
     *
     * @param keys The vector of keys associated with the resources to remove.
     */
    void remove_batch(const Vector<String>& keys);

    /**
     * @brief Registers a callback to be called on insertion.
     *
     * @param callback The callback function.
     */
    void on_insert(Callback callback);

    /**
     * @brief Registers a callback to be called on removal.
     *
     * @param callback The callback function.
     */
    void on_remove(Callback callback);

    /**
     * @brief Retrieves cache performance statistics.
     *
     * @return A pair containing hit count and miss count.
     */
    [[nodiscard]] auto get_statistics() const -> std::pair<size_t, size_t>;

private:
    struct CacheEntry {
        T value;
        TimePoint creation_time;
        Duration expiration_time;
    };

    struct Shard {
        HashMap<String, typename std::list<String>::iterator> map;
        std::list<String> lru_list;
        HashMap<String, CacheEntry> entries;
        mutable std::shared_mutex mutex;
        size_t max_size;

        explicit Shard(size_t capacity) : max_size(capacity) {}
    };

    void evict(Shard& shard);
    void cleanup_expired_entries();
    auto get_shard(const String& key) const -> Shard&;

    std::vector<std::unique_ptr<Shard>> shards_;
    const size_t shard_mask_;
    std::atomic<size_t> max_size_;
    std::atomic<size_t> current_size_{0};

    std::jthread cleanup_thread_;
    std::atomic<bool> stop_cleanup_{false};
    Duration cleanup_interval_;

    Callback insert_callback_;
    Callback remove_callback_;
    mutable std::mutex callback_mutex_;

    mutable std::atomic<size_t> hit_count_{0};
    mutable std::atomic<size_t> miss_count_{0};
};

template <Cacheable T>
ResourceCache<T>::ResourceCache(size_t max_size, Duration cleanup_interval)
    : shard_mask_([&] {
        size_t shard_count = std::thread::hardware_concurrency();
        if (shard_count == 0) shard_count = 4;
        size_t power = 1;
        while (power < shard_count) power <<= 1;
        return power - 1;
    }()),
      max_size_(max_size),
      cleanup_interval_(cleanup_interval) {
    size_t shard_count = shard_mask_ + 1;
    shards_.reserve(shard_count);
    size_t per_shard_capacity = (max_size + shard_count - 1) / shard_count;
    for (size_t i = 0; i < shard_count; ++i) {
        shards_.emplace_back(std::make_unique<Shard>(per_shard_capacity));
    }
    cleanup_thread_ = std::jthread([this] { cleanup_expired_entries(); });
}

template <Cacheable T>
ResourceCache<T>::~ResourceCache() {
    stop_cleanup_.store(true);
}

template <Cacheable T>
auto ResourceCache<T>::get_shard(const String& key) const -> Shard& {
    return *shards_[std::hash<String>{}(key) & shard_mask_];
}

template <Cacheable T>
void ResourceCache<T>::insert(const String& key, const T& value,
                              Duration expiration_time) {
    try {
        auto& shard = get_shard(key);
        std::unique_lock lock(shard.mutex);

        auto it = shard.map.find(key);
        if (it != shard.map.end()) {
            shard.lru_list.erase(it->second);
            shard.map.erase(it);
            shard.entries.erase(key);
            current_size_--;
        }

        if (shard.entries.size() >= shard.max_size) {
            evict(shard);
        }

        shard.lru_list.push_front(key);
        shard.map[key] = shard.lru_list.begin();
        shard.entries[key] = {value, Clock::now(), expiration_time};
        current_size_++;

        if (insert_callback_) {
            std::lock_guard cb_lock(callback_mutex_);
            if (insert_callback_) insert_callback_(key);
        }
    } catch (const std::exception& e) {
        spdlog::error("Insert failed for key {}: {}", key.c_str(), e.what());
    }
}

template <Cacheable T>
auto ResourceCache<T>::contains(const String& key) const -> bool {
    try {
        auto& shard = get_shard(key);
        std::shared_lock lock(shard.mutex);
        auto it = shard.entries.find(key);
        if (it == shard.entries.end()) {
            return false;
        }
        return (Clock::now() - it->second.creation_time) <
               it->second.expiration_time;
    } catch (const std::exception& e) {
        spdlog::error("Contains check failed for key {}: {}", key.c_str(),
                      e.what());
        return false;
    }
}

template <Cacheable T>
auto ResourceCache<T>::get(const String& key) -> std::optional<T> {
    try {
        auto& shard = get_shard(key);
        std::unique_lock lock(shard.mutex);

        auto map_it = shard.map.find(key);
        if (map_it == shard.map.end()) {
            miss_count_++;
            return std::nullopt;
        }

        auto& entry = shard.entries.at(key);
        if ((Clock::now() - entry.creation_time) >= entry.expiration_time) {
            miss_count_++;
            // Entry is expired, remove it
            shard.lru_list.erase(map_it->second);
            shard.map.erase(map_it);
            shard.entries.erase(key);
            current_size_--;
            if (remove_callback_) {
                std::lock_guard cb_lock(callback_mutex_);
                if (remove_callback_) remove_callback_(key);
            }
            return std::nullopt;
        }

        // Move to front of LRU list
        shard.lru_list.splice(shard.lru_list.begin(), shard.lru_list,
                              map_it->second);
        hit_count_++;
        return entry.value;
    } catch (const std::exception& e) {
        spdlog::error("Get failed for key {}: {}", key.c_str(), e.what());
        miss_count_++;
        return std::nullopt;
    }
}

template <Cacheable T>
void ResourceCache<T>::remove(const String& key) {
    try {
        auto& shard = get_shard(key);
        std::unique_lock lock(shard.mutex);
        auto it = shard.map.find(key);
        if (it != shard.map.end()) {
            shard.lru_list.erase(it->second);
            shard.map.erase(it);
            shard.entries.erase(key);
            current_size_--;
            if (remove_callback_) {
                std::lock_guard cb_lock(callback_mutex_);
                if (remove_callback_) remove_callback_(key);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Remove failed for key {}: {}", key.c_str(), e.what());
    }
}

template <Cacheable T>
auto ResourceCache<T>::async_get(const String& key)
    -> std::future<std::optional<T>> {
    return std::async(std::launch::async, [this, key]() { return get(key); });
}

template <Cacheable T>
auto ResourceCache<T>::async_insert(const String& key, const T& value,
                                    Duration expiration_time) -> std::future<void> {
    return std::async(std::launch::async, [this, key, value, expiration_time]() {
        insert(key, value, expiration_time);
    });
}

template <Cacheable T>
void ResourceCache<T>::clear() {
    for (auto& shard_ptr : shards_) {
        std::unique_lock lock(shard_ptr->mutex);
        shard_ptr->map.clear();
        shard_ptr->lru_list.clear();
        shard_ptr->entries.clear();
    }
    current_size_ = 0;
}

template <Cacheable T>
auto ResourceCache<T>::size() const -> size_t {
    return current_size_.load();
}

template <Cacheable T>
auto ResourceCache<T>::empty() const -> bool {
    return size() == 0;
}

template <Cacheable T>
void ResourceCache<T>::evict(Shard& shard) {
    if (shard.lru_list.empty()) {
        return;
    }
    String key_to_evict = shard.lru_list.back();
    shard.lru_list.pop_back();
    shard.map.erase(key_to_evict);
    shard.entries.erase(key_to_evict);
    current_size_--;

    if (remove_callback_) {
        std::lock_guard cb_lock(callback_mutex_);
        if (remove_callback_) remove_callback_(key_to_evict);
    }
    spdlog::info("Evicted key: {}", key_to_evict.c_str());
}

template <Cacheable T>
void ResourceCache<T>::cleanup_expired_entries() {
    while (!stop_cleanup_.load()) {
        std::this_thread::sleep_for(cleanup_interval_);
        if (stop_cleanup_.load()) break;

        for (auto& shard_ptr : shards_) {
            std::unique_lock lock(shard_ptr->mutex);
            Vector<String> expired_keys;
            for (const auto& key : shard_ptr->lru_list) {
                const auto& entry = shard_ptr->entries.at(key);
                if ((Clock::now() - entry.creation_time) >=
                    entry.expiration_time) {
                    expired_keys.push_back(key);
                }
            }

            for (const auto& key : expired_keys) {
                auto it = shard_ptr->map.find(key);
                if (it != shard_ptr->map.end()) {
                    shard_ptr->lru_list.erase(it->second);
                    shard_ptr->map.erase(it);
                    shard_ptr->entries.erase(key);
                    current_size_--;
                    if (remove_callback_) {
                        std::lock_guard cb_lock(callback_mutex_);
                        if (remove_callback_) remove_callback_(key);
                    }
                    spdlog::info("Removed expired key: {}", key.c_str());
                }
            }
        }
    }
}

template <Cacheable T>
void ResourceCache<T>::set_max_size(size_t new_max_size) {
    max_size_ = new_max_size;
    size_t per_shard_capacity =
        (new_max_size + shards_.size() - 1) / shards_.size();
    for (auto& shard_ptr : shards_) {
        std::unique_lock lock(shard_ptr->mutex);
        shard_ptr->max_size = per_shard_capacity;
        while (shard_ptr->entries.size() > per_shard_capacity) {
            evict(*shard_ptr);
        }
    }
}

template <Cacheable T>
void ResourceCache<T>::read_from_json_file(
    const String& file_path, const std::function<T(const json&)>& from_json,
    Duration expiration_time) {
    std::ifstream input_file(file_path.c_str());
    if (!input_file.is_open()) {
        spdlog::error("Failed to open JSON file for reading: {}",
                      file_path.c_str());
        return;
    }

    try {
        json data;
        input_file >> data;
        if (data.is_object()) {
            for (auto it = data.begin(); it != data.end(); ++it) {
                insert(String(it.key()), from_json(it.value()), expiration_time);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error processing JSON file {}: {}", file_path.c_str(),
                      e.what());
    }
}

template <Cacheable T>
void ResourceCache<T>::write_to_json_file(
    const String& file_path,
    const std::function<json(const T&)>& to_json) const {
    json data = json::object();
    for (const auto& shard_ptr : shards_) {
        std::shared_lock lock(shard_ptr->mutex);
        for (const auto& pair : shard_ptr->entries) {
            data[std::string(pair.first.c_str())] = to_json(pair.second.value);
        }
    }

    std::ofstream output_file(file_path.c_str());
    if (!output_file.is_open()) {
        spdlog::error("Failed to open JSON file for writing: {}",
                      file_path.c_str());
        return;
    }
    output_file << data.dump(4);
}

template <Cacheable T>
void ResourceCache<T>::insert_batch(const Vector<std::pair<String, T>>& items,
                                    Duration expiration_time) {
    for (const auto& [key, value] : items) {
        insert(key, value, expiration_time);
    }
}

template <Cacheable T>
void ResourceCache<T>::remove_batch(const Vector<String>& keys) {
    for (const auto& key : keys) {
        remove(key);
    }
}

template <Cacheable T>
void ResourceCache<T>::on_insert(Callback callback) {
    std::lock_guard lock(callback_mutex_);
    insert_callback_ = std::move(callback);
}

template <Cacheable T>
void ResourceCache<T>::on_remove(Callback callback) {
    std::lock_guard lock(callback_mutex_);
    remove_callback_ = std::move(callback);
}

template <Cacheable T>
auto ResourceCache<T>::get_statistics() const -> std::pair<size_t, size_t> {
    return {hit_count_.load(), miss_count_.load()};
}

}  // namespace atom::search

#endif  // ATOM_SEARCH_CACHE_HPP