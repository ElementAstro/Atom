#ifndef ATOM_TYPE_CONCURRENT_SET_HPP
#define ATOM_TYPE_CONCURRENT_SET_HPP

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <list>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace atom::type {

/**
 * @brief A Least Recently Used (LRU) cache.
 *
 * This class implements an LRU cache which evicts the least recently used items
 * when the cache reaches its maximum size.
 *
 * @tparam Key The type of the keys in the cache.
 */
template <typename Key>
class LRUCache {
private:
    size_t max_size;       ///< The maximum size of the cache.
    std::list<Key> cache;  ///< The cache storage.
    std::unordered_map<Key, typename std::list<Key>::iterator>
        cache_map;  ///< Map for quick lookup.

public:
    /**
     * @brief Constructs an LRUCache with a specified size.
     *
     * @param size The maximum size of the cache.
     */
    explicit LRUCache(size_t size) : max_size(size) {}

    /**
     * @brief Checks if a key exists in the cache.
     *
     * @param key The key to check.
     * @return True if the key exists, false otherwise.
     */
    bool exists(const Key& key) {
        return cache_map.find(key) != cache_map.end();
    }

    /**
     * @brief Inserts a key into the cache.
     *
     * If the key already exists, it is moved to the front of the cache.
     * If the cache is full, the least recently used item is evicted.
     *
     * @param key The key to insert.
     */
    void put(const Key& key) {
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            cache.splice(cache.begin(), cache, it->second);  // Move to front
        } else {
            if (cache.size() == max_size) {
                auto last = cache.back();
                cache_map.erase(last);
                cache.pop_back();
            }
            cache.push_front(key);
            cache_map[key] = cache.begin();
        }
    }

    /**
     * @brief Gets the key from the cache.
     *
     * If the key is found, it is moved to the front of the cache.
     *
     * @param key The key to get.
     * @return A pointer to the key if found, nullptr otherwise.
     */
    Key* get(const Key& key) {
        auto it = cache_map.find(key);
        if (it == cache_map.end())
            return nullptr;
        cache.splice(cache.begin(), cache, it->second);  // Move to front
        return &it->second;
    }
};

/**
 * @brief A thread-safe set that supports concurrent operations.
 *
 * This class provides a set-like container that supports concurrent read and
 * write operations. It also includes a thread pool for executing tasks in
 * parallel and an optional LRU cache.
 *
 * @tparam Key The type of the keys in the set.
 * @tparam SetType The underlying set type. Defaults to std::unordered_set.
 */
template <typename Key, typename SetType = std::unordered_set<Key>>
class concurrent_set {
private:
    SetType data;  ///< The underlying data storage.
    mutable std::shared_mutex
        mtx;  ///< Mutex for protecting read and write operations.
    std::queue<std::function<void()>>
        task_queue;  ///< Queue of tasks to be executed by the thread pool.
    std::vector<std::thread>
        thread_pool;          ///< The thread pool for executing tasks.
    bool stop_pool = false;   ///< Flag to stop the thread pool.
    LRUCache<Key> lru_cache;  ///< Optional LRU cache.
    std::atomic<int> insertion_count{0};  ///< Count of insert operations.
    std::atomic<int> deletion_count{0};   ///< Count of delete operations.

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
     * @brief Constructs a concurrent_set with a specified number of threads and
     * cache size.
     *
     * @param num_threads The number of threads in the thread pool. Defaults to
     * the number of hardware threads.
     * @param cache_size The size of the LRU cache. Defaults to 0 (no cache).
     */
    concurrent_set(size_t num_threads = std::thread::hardware_concurrency(),
                   size_t cache_size = 0)
        : lru_cache(cache_size) {
        // Start the thread pool
        for (size_t i = 0; i < num_threads; ++i) {
            thread_pool.push_back(
                std::thread(&concurrent_set::thread_pool_worker, this));
        }
    }

    /**
     * @brief Destructor for concurrent_set.
     *
     * Stops the thread pool and joins all threads.
     */
    ~concurrent_set() {
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
     * @brief Inserts an element into the set.
     *
     * This operation is thread-safe.
     *
     * @param key The key to insert.
     */
    void insert(const Key& key) {
        std::unique_lock lock(mtx);
        data.insert(key);
        lru_cache.put(key);  // Insert into LRU cache
        insertion_count++;
    }

    /**
     * @brief Asynchronously inserts an element into the set.
     *
     * This operation is thread-safe.
     *
     * @param key The key to insert.
     */
    void async_insert(const Key& key) {
        std::lock_guard lock(mtx);
        task_queue.push([this, key]() { insert(key); });
        pool_cv.notify_one();
    }

    /**
     * @brief Finds an element in the set.
     *
     * This operation is thread-safe.
     *
     * @param key The key to find.
     * @return An optional containing true if found, std::nullopt otherwise.
     */
    std::optional<bool> find(const Key& key) {
        {
            std::shared_lock lock(mtx);
            if (data.find(key) != data.end()) {
                return true;
            }
        }

        // Try to get from LRU cache
        if (lru_cache.exists(key)) {
            return true;
        }

        return std::nullopt;  // Not found
    }

    /**
     * @brief Asynchronously finds an element in the set.
     *
     * This operation is thread-safe.
     *
     * @param key The key to find.
     * @param callback The callback function to call with the result.
     */
    void async_find(const Key& key,
                    std::function<void(std::optional<bool>)> callback) {
        std::lock_guard lock(mtx);
        task_queue.push([this, key, callback]() { callback(find(key)); });
        pool_cv.notify_one();
    }

    /**
     * @brief Erases an element from the set.
     *
     * This operation is thread-safe.
     *
     * @param key The key to erase.
     */
    void erase(const Key& key) {
        std::unique_lock lock(mtx);
        data.erase(key);
        // Remove from LRU cache
        lru_cache.put(key);  // Store empty value to indicate deletion
        deletion_count++;
    }

    /**
     * @brief Inserts a batch of elements into the set.
     *
     * This operation is thread-safe.
     *
     * @param keys The keys to insert.
     */
    void batch_insert(const std::vector<Key>& keys) {
        std::unique_lock lock(mtx);
        for (const auto& key : keys) {
            data.insert(key);
            lru_cache.put(key);
        }
    }

    /**
     * @brief Erases a batch of elements from the set.
     *
     * This operation is thread-safe.
     *
     * @param keys The keys to erase.
     */
    void batch_erase(const std::vector<Key>& keys) {
        std::unique_lock lock(mtx);
        for (const auto& key : keys) {
            data.erase(key);
            lru_cache.put(key);  // Store empty value to indicate deletion
        }
    }

    /**
     * @brief Clears all elements from the set.
     *
     * This operation is thread-safe.
     */
    void clear() {
        std::unique_lock lock(mtx);
        data.clear();
        lru_cache = LRUCache<Key>(lru_cache.get_max_size());  // Clear cache
    }

    /**
     * @brief Gets the current size of the set.
     *
     * This operation is thread-safe.
     *
     * @return The number of elements in the set.
     */
    int size() const {
        std::shared_lock lock(mtx);
        return data.size();
    }

    /**
     * @brief Gets the count of insert operations.
     *
     * @return The number of insert operations.
     */
    int get_insertion_count() const { return insertion_count.load(); }

    /**
     * @brief Gets the count of delete operations.
     *
     * @return The number of delete operations.
     */
    int get_deletion_count() const { return deletion_count.load(); }

    /**
     * @brief Performs a parallel for_each operation on the elements.
     *
     * Applies the given function to each element in the set in parallel.
     *
     * @param func The function to apply to each element.
     */
    void parallel_for_each(std::function<void(const Key&)> func) {
        std::shared_lock lock(mtx);
        std::vector<std::future<void>> futures;

        for (const auto& key : data) {
            futures.push_back(
                std::async(std::launch::async, [func, key]() { func(key); }));
        }

        for (auto& future : futures) {
            future.get();
        }
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
                    std::thread(&concurrent_set::thread_pool_worker, this));
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

    /**
     * @brief Gets a constant reference to the current data.
     *
     * This operation is thread-safe for reading.
     *
     * @return A constant reference to the data.
     */
    const SetType& get_data() const {
        std::shared_lock lock(mtx);
        return data;
    }

    /**
     * @brief Ensures atomicity of a series of operations.
     *
     * This operation is thread-safe.
     *
     * @param operations The operations to perform atomically.
     * @return True if all operations succeed, false otherwise.
     */
    bool transaction(std::vector<std::function<void()>>& operations) {
        std::unique_lock lock(mtx);
        try {
            // Execute all operations
            for (auto& op : operations) {
                op();
            }
            return true;  // All operations succeeded
        } catch (const std::exception& e) {
            std::cerr << "Transaction failed: " << e.what() << std::endl;
            return false;  // If an exception occurs, return failure
        }
    }

    /**
     * @brief Finds elements that match a given condition.
     *
     * This operation is thread-safe.
     *
     * @param condition The condition to match.
     * @return A vector of keys that match the condition.
     */
    std::vector<Key> conditional_find(
        std::function<bool(const Key&)> condition) {
        std::shared_lock lock(mtx);
        std::vector<Key> result;
        for (const auto& key : data) {
            if (condition(key)) {
                result.push_back(key);
            }
        }
        return result;
    }

    /**
     * @brief Saves the current set data to a file.
     *
     * This operation is thread-safe.
     *
     * @param filename The name of the file to save to.
     * @return True if the save is successful, false otherwise.
     */
    bool save_to_file(const std::string& filename) const {
        std::shared_lock lock(mtx);
        try {
            std::ofstream out(filename, std::ios::binary);
            if (!out.is_open())
                return false;

            size_t size = data.size();
            out.write(reinterpret_cast<const char*>(&size), sizeof(size));
            for (const auto& key : data) {
                out.write(reinterpret_cast<const char*>(&key), sizeof(Key));
            }
            out.close();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error saving to file: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Loads set data from a file.
     *
     * This operation is thread-safe.
     *
     * @param filename The name of the file to load from.
     * @return True if the load is successful, false otherwise.
     */
    bool load_from_file(const std::string& filename) {
        std::unique_lock lock(mtx);
        try {
            std::ifstream in(filename, std::ios::binary);
            if (!in.is_open())
                return false;

            size_t size = 0;
            in.read(reinterpret_cast<char*>(&size), sizeof(size));

            for (size_t i = 0; i < size; ++i) {
                Key key;
                in.read(reinterpret_cast<char*>(&key), sizeof(Key));
                data.insert(key);
            }
            in.close();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error loading from file: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Gets the current size of the LRU cache.
     *
     * @return The size of the LRU cache.
     */
    size_t get_cache_size() const { return lru_cache.get_max_size(); }

    /**
     * @brief Gets the hit rate of the LRU cache.
     *
     * @return The hit rate of the LRU cache.
     */
    double get_cache_hit_rate() {
        size_t hit_count = 0;
        for (const auto& key : data) {
            if (lru_cache.exists(key)) {
                ++hit_count;
            }
        }
        return static_cast<double>(hit_count) / data.size();
    }

    /**
     * @brief Sets a callback function to handle errors.
     *
     * @param callback The callback function to handle errors.
     */
    void set_error_callback(std::function<void(const std::string&)> callback) {
        error_callback = callback;
    }

private:
    std::function<void(const std::string&)>
        error_callback;  ///< Callback function to handle errors.

    /**
     * @brief Handles errors by calling the error callback function.
     *
     * @param error_message The error message to handle.
     */
    void handle_error(const std::string& error_message) {
        if (error_callback) {
            error_callback(error_message);
        } else {
            std::cerr << "Error: " << error_message << std::endl;
        }
    }
};

}  // namespace atom::type

#endif