#ifndef ATOM_TYPE_CONCURRENT_MAP_HPP
#define ATOM_TYPE_CONCURRENT_MAP_HPP

#include <algorithm>           // For std::for_each
#include <atomic>              // For std::atomic
#include <cassert>             // For assert
#include <condition_variable>  // For std::condition_variable
#include <exception>           // For std::exception
#include <functional>          // For std::function
#include <future>              // For std::future, std::packaged_task
#include <memory>              // For std::shared_ptr, std::unique_ptr
#include <mutex>               // For std::mutex
#include <optional>            // For std::optional
#include <queue>               // For std::queue
#include <shared_mutex>        // For std::shared_mutex
#include <stdexcept>           // For std::runtime_error, std::invalid_argument
#include <thread>              // For std::thread
#include <type_traits>  // For std::is_copy_constructible, std::is_move_constructible
#include <unordered_map>  // For std::unordered_map
#include <vector>         // For std::vector

#ifdef __SSE2__
#include <emmintrin.h>  // For SSE2 intrinsics
#endif

#include "atom/search/lru.hpp"

namespace atom::type {

/**
 * @brief Exception class for concurrent_map operations
 */
class concurrent_map_error : public std::runtime_error {
public:
    explicit concurrent_map_error(const std::string& message)
        : std::runtime_error(message) {}
};

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
public:
    // Type aliases for better readability
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    using task_type = std::function<void()>;
    using result_type = std::optional<T>;

private:
    MapType data;  ///< The underlying data storage.
    mutable std::shared_mutex
        mtx;  ///< Mutex for protecting read and write operations.

    std::queue<std::packaged_task<void()>>
        task_queue;  ///< Queue of tasks to be executed by the thread pool.

    std::vector<std::unique_ptr<std::thread>>
        thread_pool;  ///< The thread pool for executing tasks.

    std::atomic<bool> stop_pool{false};  ///< Flag to stop the thread pool.

    std::unique_ptr<atom::search::ThreadSafeLRUCache<Key, T>>
        lru_cache;  ///< Optional LRU cache.

    static constexpr size_t DEFAULT_BATCH_SIZE =
        100;  ///< Default batch size for operations.

    /**
     * @brief Worker function for the thread pool.
     *
     * This function runs in each thread of the thread pool and executes tasks
     * from the task queue.
     */
    void thread_pool_worker() noexcept {
        try {
            while (true) {
                std::packaged_task<void()> task;
                {
                    std::unique_lock lock(pool_mutex);
                    pool_cv.wait(lock, [this] {
                        return !task_queue.empty() || stop_pool;
                    });

                    if (stop_pool && task_queue.empty()) {
                        return;
                    }

                    if (!task_queue.empty()) {
                        task = std::move(task_queue.front());
                        task_queue.pop();
                    }
                }

                // Execute the task if it's valid
                if (task.valid()) {
                    try {
                        task();
                    } catch (const std::exception& e) {
                        // Log the exception but continue processing tasks
                        // In a real application, use a proper logging mechanism
                        // std::cerr << "Exception in thread pool task: " <<
                        // e.what() << std::endl;
                    } catch (...) {
                        // std::cerr << "Unknown exception in thread pool task"
                        // << std::endl;
                    }
                }
            }
        } catch (const std::exception& e) {
            // Log worker thread exception
            // std::cerr << "Exception in thread pool worker: " << e.what() <<
            // std::endl;
        } catch (...) {
            // std::cerr << "Unknown exception in thread pool worker" <<
            // std::endl;
        }
    }

    /**
     * @brief Validates if a key exists in the map.
     *
     * @param key The key to check.
     * @return true if the key exists, false otherwise.
     */
    [[nodiscard]] bool key_exists(const Key& key) const noexcept {
        std::shared_lock lock(mtx);
        return data.find(key) != data.end();
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
     * @throws std::invalid_argument if num_threads is 0.
     */
    explicit concurrent_map(
        size_t num_threads = std::thread::hardware_concurrency(),
        size_t cache_size = 0) {
        if (num_threads == 0) {
            throw std::invalid_argument(
                "Number of threads must be greater than 0");
        }

        // Initialize the LRU cache if needed
        if (cache_size > 0) {
            lru_cache =
                std::make_unique<atom::search::ThreadSafeLRUCache<Key, T>>(
                    cache_size);
        }

        // Start the thread pool
        try {
            thread_pool.reserve(num_threads);
            for (size_t i = 0; i < num_threads; ++i) {
                thread_pool.push_back(std::make_unique<std::thread>(
                    &concurrent_map::thread_pool_worker, this));
            }
        } catch (const std::exception& e) {
            // Clean up any created threads
            stop_pool = true;
            pool_cv.notify_all();
            for (auto& t : thread_pool) {
                if (t && t->joinable()) {
                    t->join();
                }
            }
            throw concurrent_map_error(
                std::string("Failed to create thread pool: ") + e.what());
        }
    }

    /**
     * @brief Move constructor
     *
     * @param other The concurrent_map to move from
     */
    concurrent_map(concurrent_map&& other) noexcept {
        std::unique_lock lock1(mtx, std::defer_lock);
        std::unique_lock lock2(other.mtx, std::defer_lock);
        std::lock(lock1, lock2);

        data = std::move(other.data);
        lru_cache = std::move(other.lru_cache);

        // Handle thread pool and task queue separately to avoid race conditions
        std::unique_lock pool_lock1(pool_mutex, std::defer_lock);
        std::unique_lock pool_lock2(other.pool_mutex, std::defer_lock);
        std::lock(pool_lock1, pool_lock2);

        stop_pool = other.stop_pool.load();
        task_queue = std::move(other.task_queue);
        thread_pool = std::move(other.thread_pool);
    }

    /**
     * @brief Deleted copy constructor
     */
    concurrent_map(const concurrent_map&) = delete;

    /**
     * @brief Deleted copy assignment operator
     */
    concurrent_map& operator=(const concurrent_map&) = delete;

    /**
     * @brief Move assignment operator
     */
    concurrent_map& operator=(concurrent_map&& other) noexcept {
        if (this != &other) {
            // Stop current thread pool
            {
                std::unique_lock lock(pool_mutex);
                stop_pool = true;
            }
            pool_cv.notify_all();
            for (auto& t : thread_pool) {
                if (t && t->joinable()) {
                    t->join();
                }
            }
            thread_pool.clear();

            // Move other data
            std::unique_lock lock1(mtx, std::defer_lock);
            std::unique_lock lock2(other.mtx, std::defer_lock);
            std::lock(lock1, lock2);

            data = std::move(other.data);
            lru_cache = std::move(other.lru_cache);

            // Handle thread pool and task queue
            std::unique_lock pool_lock1(pool_mutex, std::defer_lock);
            std::unique_lock pool_lock2(other.pool_mutex, std::defer_lock);
            std::lock(pool_lock1, pool_lock2);

            stop_pool = other.stop_pool.load();
            task_queue = std::move(other.task_queue);
            thread_pool = std::move(other.thread_pool);
        }
        return *this;
    }

    /**
     * @brief Destructor for concurrent_map.
     *
     * Stops the thread pool and joins all threads.
     */
    ~concurrent_map() {
        try {
            // Stop the thread pool and join all threads
            {
                std::unique_lock lock(pool_mutex);
                stop_pool = true;
            }
            pool_cv.notify_all();

            for (auto& t : thread_pool) {
                if (t && t->joinable()) {
                    t->join();
                }
            }
        } catch (...) {
            // Destructors should never throw
            // std::cerr << "Exception caught in concurrent_map destructor"
            //          << std::endl;
        }
    }

    /**
     * @brief Inserts or updates an element in the map.
     *
     * This operation is thread-safe.
     *
     * @param key The key to insert or update.
     * @param value The value to insert or update.
     * @throws concurrent_map_error if an error occurs during insertion.
     */
    void insert(const Key& key, const T& value) {
        try {
            std::unique_lock lock(mtx);
            data[key] = value;

            if (lru_cache) {
                lru_cache->put(key, value);
            }
        } catch (const std::exception& e) {
            throw concurrent_map_error(
                std::string("Insert operation failed: ") + e.what());
        }
    }

    /**
     * @brief Inserts or updates an element in the map with move semantics.
     *
     * This operation is thread-safe.
     *
     * @param key The key to insert or update.
     * @param value The value to insert or update (to be moved).
     * @throws concurrent_map_error if an error occurs during insertion.
     */
    void insert(const Key& key, T&& value) {
        try {
            std::unique_lock lock(mtx);
            data[key] = std::move(value);

            if (lru_cache) {
                // Need to copy since we've moved value
                lru_cache->put(key, data[key]);
            }
        } catch (const std::exception& e) {
            throw concurrent_map_error(
                std::string("Insert operation failed: ") + e.what());
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
    [[nodiscard]] result_type find(const Key& key) const noexcept {
        try {
            // First check the cache if available
            if (lru_cache) {
                auto cache_value = lru_cache->get(key);
                if (cache_value.has_value()) {
                    return cache_value;
                }
            }

            // If not in cache, check the main data
            {
                std::shared_lock lock(mtx);
                auto it = data.find(key);
                if (it != data.end()) {
                    // Update cache if it exists
                    if (lru_cache) {
                        lru_cache->put(key, it->second);
                    }
                    return it->second;
                }
            }

            return std::nullopt;
        } catch (...) {
            // Ensure noexcept guarantee
            return std::nullopt;
        }
    }

    /**
     * @brief Inserts an element if it does not exist.
     *
     * This operation is thread-safe.
     *
     * @param key The key to insert.
     * @param value The value to insert.
     * @return true if the element was inserted, false if it already existed.
     * @throws concurrent_map_error if an error occurs during insertion.
     */
    bool find_or_insert(const Key& key, const T& value) {
        try {
            std::unique_lock lock(mtx);
            auto [it, inserted] = data.try_emplace(key, value);

            if (inserted && lru_cache) {
                lru_cache->put(key, value);
            }

            return inserted;
        } catch (const std::exception& e) {
            throw concurrent_map_error(
                std::string("Find or insert operation failed: ") + e.what());
        }
    }

    /**
     * @brief Merges another concurrent_map into this one.
     *
     * This operation is thread-safe.
     *
     * @param other The other concurrent_map to merge.
     * @throws concurrent_map_error if an error occurs during merging.
     */
    void merge(const concurrent_map<Key, T, MapType>& other) {
        try {
            std::unique_lock lock(mtx);
            std::shared_lock other_lock(other.mtx);

            // Reserve space for potential new elements
            if constexpr (std::is_same_v<MapType, std::unordered_map<Key, T>>) {
                data.reserve(data.size() + other.data.size());
            }

            for (const auto& [key, value] : other.data) {
                data[key] = value;
                if (lru_cache) {
                    lru_cache->put(key, value);
                }
            }
        } catch (const std::exception& e) {
            throw concurrent_map_error(std::string("Merge operation failed: ") +
                                       e.what());
        }
    }

    /**
     * @brief Submit a task to the thread pool and return a future to the
     * result.
     *
     * @tparam F Function type
     * @tparam Args Argument types
     * @param f Function to execute
     * @param args Arguments to pass to the function
     * @return std::future with the result of the function
     * @throws concurrent_map_error if the thread pool is stopped or an error
     * occurs.
     */
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using return_type = decltype(f(args...));

        try {
            if (stop_pool) {
                throw concurrent_map_error("Thread pool is stopped");
            }

            // Create a packaged task with the function and its arguments
            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));

            // Get the future before moving the packaged task
            std::future<return_type> result = task->get_future();

            {
                std::unique_lock lock(pool_mutex);
                if (stop_pool) {
                    throw concurrent_map_error("Thread pool is stopped");
                }

                // Wrap the packaged task in a void function
                task_queue.emplace([task]() { (*task)(); });
            }

            pool_cv.notify_one();
            return result;
        } catch (const std::exception& e) {
            throw concurrent_map_error(std::string("Failed to submit task: ") +
                                       e.what());
        }
    }

    /**
     * @brief Performs a batch find operation using the thread pool for parallel
     * execution.
     *
     * This operation is thread-safe.
     *
     * @param keys The keys to find.
     * @return A vector of optionals containing the values if found,
     * std::nullopt otherwise.
     * @throws concurrent_map_error if an error occurs during the operation.
     */
    [[nodiscard]] std::vector<result_type> batch_find(
        const std::vector<Key>& keys) {
        try {
            if (keys.empty()) {
                return {};
            }

            std::vector<result_type> results(keys.size());

            // Use a higher level of parallelism for batched operations
            const size_t num_threads = thread_pool.size();
            if (num_threads <= 1 || keys.size() <= DEFAULT_BATCH_SIZE) {
                // Sequential processing for small batches or single-threaded
                // mode
                for (size_t i = 0; i < keys.size(); ++i) {
                    results[i] = find(keys[i]);
                }
            } else {
                // Parallel processing using the thread pool
                const size_t chunk_size =
                    std::max(size_t(1), keys.size() / num_threads);
                std::vector<std::future<void>> futures;

                for (size_t i = 0; i < keys.size(); i += chunk_size) {
                    const size_t end = std::min(i + chunk_size, keys.size());

                    futures.push_back(submit([this, &keys, &results, i, end]() {
                        for (size_t j = i; j < end; ++j) {
                            results[j] = this->find(keys[j]);
                        }
                    }));
                }

                // Wait for all tasks to complete
                for (auto& future : futures) {
                    future.get();
                }
            }

            return results;
        } catch (const std::exception& e) {
            throw concurrent_map_error(
                std::string("Batch find operation failed: ") + e.what());
        }
    }

    /**
     * @brief Performs a batch update operation with parallel execution where
     * possible.
     *
     * This operation is thread-safe.
     *
     * @param updates The key-value pairs to update.
     * @throws concurrent_map_error if an error occurs during the batch update.
     */
    void batch_update(const std::vector<std::pair<Key, T>>& updates) {
        if (updates.empty()) {
            return;
        }

        try {
            // Use a unique lock for the entire operation to maintain atomicity
            std::unique_lock lock(mtx);

            // Pre-allocate space if using unordered_map
            if constexpr (std::is_same_v<MapType, std::unordered_map<Key, T>>) {
                data.reserve(data.size() + updates.size());
            }

#ifdef __SSE2__
            // Use SIMD for bulk operations if the type supports it
            if constexpr (std::is_arithmetic_v<T> && sizeof(T) == 4) {
                // Process updates in chunks that can benefit from SIMD
                for (size_t i = 0; i < updates.size(); i += 4) {
                    if (i + 4 <= updates.size()) {
                        // Prepare data for SIMD processing
                        T values[4];
                        Key keys[4];

                        for (size_t j = 0; j < 4; ++j) {
                            keys[j] = updates[i + j].first;
                            values[j] = updates[i + j].second;
                        }

                        // Load values into a SIMD register (example for float)
                        if constexpr (std::is_same_v<T, float>) {
                            __m128 simd_values = _mm_loadu_ps(values);

                            // Store each value in the map
                            for (size_t j = 0; j < 4; ++j) {
                                data[keys[j]] = values[j];
                                if (lru_cache) {
                                    lru_cache->put(keys[j], values[j]);
                                }
                            }
                        } else {
                            // Fallback for non-float types
                            for (size_t j = 0; j < 4; ++j) {
                                data[keys[j]] = values[j];
                                if (lru_cache) {
                                    lru_cache->put(keys[j], values[j]);
                                }
                            }
                        }
                    } else {
                        // Handle remaining elements
                        for (size_t j = i; j < updates.size(); ++j) {
                            const auto& [key, value] = updates[j];
                            data[key] = value;
                            if (lru_cache) {
                                lru_cache->put(key, value);
                            }
                        }
                    }
                }
            } else
#endif
            {
                // Standard processing for all other types
                for (const auto& [key, value] : updates) {
                    data[key] = value;
                    if (lru_cache) {
                        lru_cache->put(key, value);
                    }
                }
            }
        } catch (const std::exception& e) {
            throw concurrent_map_error(
                std::string("Batch update operation failed: ") + e.what());
        }
    }

    /**
     * @brief Performs a batch erase operation.
     *
     * This operation is thread-safe.
     *
     * @param keys The keys to erase.
     * @return Number of elements actually erased.
     * @throws concurrent_map_error if an error occurs during the batch erase.
     */
    size_t batch_erase(const std::vector<Key>& keys) {
        if (keys.empty()) {
            return 0;
        }

        try {
            size_t erased_count = 0;

            // Take a unique lock for the entire operation
            std::unique_lock lock(mtx);

            for (const auto& key : keys) {
                auto it = data.find(key);
                if (it != data.end()) {
                    data.erase(it);
                    erased_count++;

                    if (lru_cache) {
                        lru_cache->erase(key);
                    }
                }
            }

            return erased_count;
        } catch (const std::exception& e) {
            throw concurrent_map_error(
                std::string("Batch erase operation failed: ") + e.what());
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
     * @throws concurrent_map_error if an error occurs during the range query.
     */
    [[nodiscard]] std::vector<std::pair<Key, T>> range_query(const Key& start,
                                                             const Key& end) {
        try {
            if (!(start <= end)) {
                throw std::invalid_argument(
                    "Start key must be less than or equal to end key");
            }

            std::shared_lock lock(mtx);
            std::vector<std::pair<Key, T>> result;

            // Reserve a reasonable initial capacity
            result.reserve(std::min<size_t>(100, data.size() / 10));

            for (const auto& [key, value] : data) {
                if (key >= start && key <= end) {
                    result.emplace_back(key, value);
                } else if (key > end) {
                    // If we find a key greater than the end, we can stop if
                    // using an ordered map This optimization applies only to
                    // ordered maps
                    if constexpr (!std::is_same_v<MapType,
                                                  std::unordered_map<Key, T>>) {
                        break;
                    }
                }
            }

            return result;
        } catch (const std::exception& e) {
            throw concurrent_map_error(
                std::string("Range query operation failed: ") + e.what());
        }
    }

    /**
     * @brief Gets a constant reference to the current data.
     *
     * This operation is thread-safe for reading.
     *
     * @return A copy of the data to prevent race conditions.
     * @throws concurrent_map_error if an error occurs during the operation.
     */
    [[nodiscard]] MapType get_data() const {
        try {
            std::shared_lock lock(mtx);
            return data;  // Return a copy to prevent race conditions
        } catch (const std::exception& e) {
            throw concurrent_map_error(
                std::string("Get data operation failed: ") + e.what());
        }
    }

    /**
     * @brief Returns the size of the map.
     *
     * @return size_t The number of elements in the map.
     */
    [[nodiscard]] size_t size() const noexcept {
        try {
            std::shared_lock lock(mtx);
            return data.size();
        } catch (...) {
            // Ensure noexcept guarantee
            return 0;
        }
    }

    /**
     * @brief Checks if the map is empty.
     *
     * @return bool True if the map is empty, false otherwise.
     */
    [[nodiscard]] bool empty() const noexcept {
        try {
            std::shared_lock lock(mtx);
            return data.empty();
        } catch (...) {
            // Ensure noexcept guarantee
            return true;
        }
    }

    /**
     * @brief Clears all elements from the map.
     *
     * This operation is thread-safe.
     */
    void clear() {
        try {
            std::unique_lock lock(mtx);
            data.clear();
            if (lru_cache) {
                lru_cache->clear();
            }
        } catch (const std::exception& e) {
            throw concurrent_map_error(std::string("Clear operation failed: ") +
                                       e.what());
        }
    }

    /**
     * @brief Adjusts the size of the thread pool.
     *
     * This operation is thread-safe.
     *
     * @param new_size The new size of the thread pool.
     * @throws std::invalid_argument if new_size is 0.
     * @throws concurrent_map_error if an error occurs while adjusting the
     * thread pool.
     */
    void adjust_thread_pool_size(size_t new_size) {
        if (new_size == 0) {
            throw std::invalid_argument(
                "Thread pool size must be greater than 0");
        }

        try {
            std::unique_lock lock(pool_mutex);

            if (new_size > thread_pool.size()) {
                // Add more threads
                const size_t threads_to_add = new_size - thread_pool.size();
                thread_pool.reserve(new_size);

                for (size_t i = 0; i < threads_to_add; ++i) {
                    thread_pool.push_back(std::make_unique<std::thread>(
                        &concurrent_map::thread_pool_worker, this));
                }
            } else if (new_size < thread_pool.size()) {
                // Remove threads
                const size_t current_size = thread_pool.size();
                const size_t threads_to_remove = current_size - new_size;

                // Set stop flag for threads we want to remove
                stop_pool = true;
                pool_cv.notify_all();

                // Join the threads that will be removed
                for (size_t i = current_size - threads_to_remove;
                     i < current_size; ++i) {
                    if (thread_pool[i] && thread_pool[i]->joinable()) {
                        thread_pool[i]->join();
                    }
                }

                // Resize the thread pool
                thread_pool.resize(new_size);

                // Reset the stop flag
                stop_pool = false;

                // Start new threads to replace the joined ones
                for (size_t i = 0; i < new_size; ++i) {
                    if (!thread_pool[i] || !thread_pool[i]->joinable()) {
                        thread_pool[i] = std::make_unique<std::thread>(
                            &concurrent_map::thread_pool_worker, this);
                    }
                }
            }
        } catch (const std::exception& e) {
            throw concurrent_map_error(
                std::string("Failed to adjust thread pool size: ") + e.what());
        }
    }

    /**
     * @brief Sets the cache size.
     *
     * @param cache_size The new cache size. 0 to disable caching.
     * @throws concurrent_map_error if an error occurs while adjusting the
     * cache.
     */
    void set_cache_size(size_t cache_size) {
        try {
            std::unique_lock lock(mtx);
            if (cache_size > 0) {
                if (lru_cache) {
                    // If cache already exists, create a new one with the
                    // desired size and copy over existing entries
                    auto new_cache = std::make_unique<
                        atom::search::ThreadSafeLRUCache<Key, T>>(cache_size);

                    // Copy the most recent entries from the old cache if
                    // possible This is a simplification - in a real
                    // implementation you might want a more sophisticated
                    // approach to preserve the most important entries
                    for (const auto& [key, value] : data) {
                        auto cache_value = lru_cache->get(key);
                        if (cache_value.has_value()) {
                            new_cache->put(key, *cache_value);
                        }
                    }

                    lru_cache = std::move(new_cache);
                } else {
                    // Create a new cache
                    lru_cache = std::make_unique<
                        atom::search::ThreadSafeLRUCache<Key, T>>(cache_size);

                    // Populate with existing entries (up to cache size)
                    size_t count = 0;
                    for (const auto& [key, value] : data) {
                        lru_cache->put(key, value);
                        if (++count >= cache_size)
                            break;
                    }
                }
            } else {
                // Disable cache
                lru_cache.reset();
            }
        } catch (const std::exception& e) {
            throw concurrent_map_error(
                std::string("Failed to set cache size: ") + e.what());
        }
    }

    /**
     * @brief Checks if the cache is enabled.
     *
     * @return bool True if the cache is enabled, false otherwise.
     */
    [[nodiscard]] bool has_cache() const noexcept {
        return lru_cache != nullptr;
    }

    /**
     * @brief Gets the number of threads in the thread pool.
     *
     * @return size_t The number of threads.
     */
    [[nodiscard]] size_t get_thread_count() const noexcept {
        try {
            std::unique_lock lock(pool_mutex);
            return thread_pool.size();
        } catch (...) {
            return 0;
        }
    }
};

}  // namespace atom::type

#endif  // ATOM_TYPE_CONCURRENT_MAP_HPP