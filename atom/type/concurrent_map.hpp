#ifndef ATOM_TYPE_CONCURRENT_MAP_HPP
#define ATOM_TYPE_CONCURRENT_MAP_HPP

#include <cassert>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "atom/search/lru.hpp"

namespace atom::type {
/**
 * @brief A thread-safe map that supports concurrent operations.
 *
 * This class provides a map-like container that supports concurrent read and
 * write operations. It also includes a thread pool for executing tasks in
 * parallel and an optional LRU cache.
 *
 * @tparam Key The type of the keys in the map.
 * @tparam T The type of the values in the map.
 * @tparam MapType The underlying map type. Defaults to std::unordered_map.
 */
template <typename Key, typename T,
          typename MapType = std::unordered_map<Key, T>>
class concurrent_map {
private:
    MapType data;  ///< The underlying data storage.
    mutable std::shared_mutex
        mtx;  ///< Mutex for protecting read and write operations.
    std::queue<std::function<void()>>
        task_queue;  ///< Queue of tasks to be executed by the thread pool.
    std::vector<std::thread>
        thread_pool;         ///< The thread pool for executing tasks.
    bool stop_pool = false;  ///< Flag to stop the thread pool.

    atom::search::ThreadSafeLRUCache<Key, T>
        lru_cache;  ///< Optional LRU cache.

    /**
     * @brief Worker function for the thread pool.
     *
     * This function runs in each thread of the thread pool and executes tasks
     * from the task queue.
     */
    void thread_pool_worker() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(pool_mutex);
                pool_cv.wait(
                    lock, [this] { return !task_queue.empty() || stop_pool; });

                if (stop_pool && task_queue.empty()) {
                    return;
                }

                task = task_queue.front();
                task_queue.pop();
            }

            task();
        }
    }

public:
    mutable std::mutex pool_mutex;  ///< Mutex for protecting the task queue.
    std::condition_variable
        pool_cv;  ///< Condition variable for task queue synchronization.

    /**
     * @brief Constructs a concurrent_map with a specified number of threads and
     * cache size.
     *
     * @param num_threads The number of threads in the thread pool. Defaults to
     * the number of hardware threads.
     * @param cache_size The size of the LRU cache. Defaults to 0 (no cache).
     */
    concurrent_map(size_t num_threads = std::thread::hardware_concurrency(),
                   size_t cache_size = 0)
        : lru_cache(cache_size) {
        // Start the thread pool
        for (size_t i = 0; i < num_threads; ++i) {
            thread_pool.push_back(
                std::thread(&concurrent_map::thread_pool_worker, this));
        }
    }

    /**
     * @brief Destructor for concurrent_map.
     *
     * Stops the thread pool and joins all threads.
     */
    ~concurrent_map() {
        // Stop the thread pool and join all threads
        stop_pool = true;
        pool_cv.notify_all();
        for (auto& t : thread_pool) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    /**
     * @brief Inserts or updates an element in the map.
     *
     * This operation is thread-safe.
     *
     * @param key The key to insert or update.
     * @param value The value to insert or update.
     */
    void insert(const Key& key, const T& value) {
        std::unique_lock lock(mtx);
        data[key] = value;
        auto cache_it = lru_cache.get(key);
        if (cache_it.has_value()) {
            lru_cache.put(key, value);
        }
    }

    /**
     * @brief Finds an element in the map.
     *
     * This operation is thread-safe.
     *
     * @param key The key to find.
     * @return An optional containing the value if found, std::nullopt
     * otherwise.
     */
    std::optional<T> find(const Key& key) {
        {
            std::shared_lock lock(mtx);
            auto it = data.find(key);
            if (it != data.end()) {
                return it->second;
            }
        }

        auto cache_value = lru_cache.get(key);
        if (cache_value.has_value()) {
            return cache_value;
        }

        return std::nullopt;
    }

    /**
     * @brief Inserts an element if it does not exist.
     *
     * This operation is thread-safe.
     *
     * @param key The key to insert.
     * @param value The value to insert.
     */
    void find_or_insert(const Key& key, const T& value) {
        std::unique_lock lock(mtx);
        auto it = data.find(key);
        if (it == data.end()) {
            data[key] = value;
            lru_cache.put(key, value);  // Insert into cache
        }
    }

    /**
     * @brief Merges another concurrent_map into this one.
     *
     * This operation is thread-safe.
     *
     * @param other The other concurrent_map to merge.
     */
    void merge(const concurrent_map<Key, T, MapType>& other) {
        std::unique_lock lock(mtx);
        std::shared_lock other_lock(other.mtx);
        for (const auto& kv : other.data) {
            data[kv.first] = kv.second;
            lru_cache.put(kv.first, kv.second);  // Update cache
        }
    }

    /**
     * @brief Performs a batch find operation.
     *
     * This operation is thread-safe.
     *
     * @param keys The keys to find.
     * @return A vector of optionals containing the values if found,
     * std::nullopt otherwise.
     */
    std::vector<std::optional<T>> batch_find(const std::vector<Key>& keys) {
        std::vector<std::optional<T>> result;
        for (const auto& key : keys) {
            result.push_back(find(key));
        }
        return result;
    }

    /**
     * @brief Performs a batch update operation.
     *
     * This operation is thread-safe.
     *
     * @param updates The key-value pairs to update.
     */
    void batch_update(const std::vector<std::pair<Key, T>>& updates) {
        std::unique_lock lock(mtx);
        for (const auto& kv : updates) {
            data[kv.first] = kv.second;
            lru_cache.put(kv.first, kv.second);
        }
    }

    /**
     * @brief Performs a batch erase operation.
     *
     * This operation is thread-safe.
     *
     * @param keys The keys to erase.
     */
    void batch_erase(const std::vector<Key>& keys) {
        std::unique_lock lock(mtx);
        for (const auto& key : keys) {
            data.erase(key);
            auto cache_it = lru_cache.get(key);
            if (cache_it.has_value()) {
                lru_cache.erase(key);
            }
        }
    }

    /**
     * @brief Performs a range query on the keys.
     *
     * This operation is thread-safe.
     *
     * @param start The start key of the range.
     * @param end The end key of the range.
     * @return A vector of key-value pairs within the specified range.
     */
    std::vector<std::pair<Key, T>> range_query(const Key& start,
                                               const Key& end) {
        std::shared_lock lock(mtx);
        std::vector<std::pair<Key, T>> result;
        for (auto& kv : data) {
            if (kv.first >= start && kv.first <= end) {
                result.push_back(kv);
            }
        }
        return result;
    }

    /**
     * @brief Gets a constant reference to the current data.
     *
     * This operation is thread-safe for reading.
     *
     * @return A constant reference to the data.
     */
    const MapType& get_data() const {
        std::shared_lock lock(mtx);
        return data;
    }

    /**
     * @brief Clears all elements from the map.
     *
     * This operation is thread-safe.
     */
    void clear() {
        std::unique_lock lock(mtx);
        data.clear();
        lru_cache.clear();
    }

    /**
     * @brief Adjusts the size of the thread pool.
     *
     * This operation is thread-safe.
     *
     * @param new_size The new size of the thread pool.
     */
    void adjust_thread_pool_size(size_t new_size) {
        std::unique_lock lock(pool_mutex);
        if (new_size > thread_pool.size()) {
            for (size_t i = thread_pool.size(); i < new_size; ++i) {
                thread_pool.push_back(
                    std::thread(&concurrent_map::thread_pool_worker, this));
            }
        } else if (new_size < thread_pool.size()) {
            stop_pool = true;
            pool_cv.notify_all();
            for (size_t i = new_size; i < thread_pool.size(); ++i) {
                if (thread_pool[i].joinable()) {
                    thread_pool[i].join();
                }
            }
            thread_pool.resize(new_size);
            stop_pool = false;
        }
    }
};

}  // namespace atom::type

#endif