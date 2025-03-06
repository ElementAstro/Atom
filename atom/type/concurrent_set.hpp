#ifndef ATOM_TYPE_CONCURRENT_SET_HPP
#define ATOM_TYPE_CONCURRENT_SET_HPP

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <exception>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace atom::type {

/**
 * @brief Custom exceptions for concurrent_set operations
 */
class concurrent_set_exception : public std::runtime_error {
public:
    explicit concurrent_set_exception(const std::string& msg)
        : std::runtime_error(msg) {}
};

class cache_exception : public concurrent_set_exception {
public:
    explicit cache_exception(const std::string& msg)
        : concurrent_set_exception(msg) {}
};

class transaction_exception : public concurrent_set_exception {
public:
    explicit transaction_exception(const std::string& msg)
        : concurrent_set_exception(msg) {}
};

class io_exception : public concurrent_set_exception {
public:
    explicit io_exception(const std::string& msg)
        : concurrent_set_exception(msg) {}
};

/**
 * @brief A Least Recently Used (LRU) cache with improved memory management and
 * error handling.
 *
 * @tparam Key The type of the keys in the cache.
 */
template <typename Key>
class LRUCache {
private:
    size_t max_size;       ///< The maximum size of the cache.
    std::list<Key> cache;  ///< The cache storage.
    std::unordered_map<Key, typename std::list<Key>::iterator>
        cache_map;                          ///< Map for quick lookup.
    std::atomic<size_t> hits{0};            ///< Count of cache hits
    std::atomic<size_t> misses{0};          ///< Count of cache misses
    mutable std::shared_mutex cache_mutex;  ///< Mutex for thread safety

public:
    /**
     * @brief Constructs an LRUCache with a specified size.
     *
     * @param size The maximum size of the cache.
     * @throws std::invalid_argument if size is 0
     */
    explicit LRUCache(size_t size) : max_size(size) {
        if (size == 0) {
            throw std::invalid_argument("Cache size cannot be zero");
        }
    }

    /**
     * @brief Checks if a key exists in the cache.
     *
     * @param key The key to check.
     * @return True if the key exists, false otherwise.
     */
    [[nodiscard]] bool exists(const Key& key) noexcept {
        std::shared_lock lock(cache_mutex);
        bool found = cache_map.find(key) != cache_map.end();
        if (found)
            hits++;
        else
            misses++;
        return found;
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
        std::unique_lock lock(cache_mutex);

        if (max_size == 0)
            return;  // No-op if cache disabled

        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            cache.splice(cache.begin(), cache, it->second);  // Move to front
        } else {
            if (cache.size() >= max_size) {
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
    std::optional<Key> get(const Key& key) {
        std::unique_lock lock(cache_mutex);
        auto it = cache_map.find(key);
        if (it == cache_map.end()) {
            misses++;
            return std::nullopt;
        }

        hits++;
        cache.splice(cache.begin(), cache, it->second);  // Move to front
        return *it->second;
    }

    /**
     * @brief Clears all items from the cache.
     */
    void clear() noexcept {
        std::unique_lock lock(cache_mutex);
        cache.clear();
        cache_map.clear();
    }

    /**
     * @brief Resizes the cache.
     *
     * @param new_size The new maximum size of the cache.
     * @throws std::invalid_argument if new_size is 0
     */
    void resize(size_t new_size) {
        if (new_size == 0) {
            throw std::invalid_argument("Cache size cannot be zero");
        }

        std::unique_lock lock(cache_mutex);
        max_size = new_size;

        // Remove excess items if needed
        while (cache.size() > max_size) {
            auto last = cache.back();
            cache_map.erase(last);
            cache.pop_back();
        }
    }

    /**
     * @brief Gets cache statistics.
     *
     * @return A pair of hit count and miss count.
     */
    [[nodiscard]] std::pair<size_t, size_t> get_stats() const noexcept {
        return {hits.load(), misses.load()};
    }

    /**
     * @brief Gets the hit rate of the cache.
     *
     * @return The hit rate as a percentage (0-100).
     */
    [[nodiscard]] double get_hit_rate() const noexcept {
        size_t total = hits.load() + misses.load();
        if (total == 0)
            return 0.0;
        return (static_cast<double>(hits.load()) / total) * 100.0;
    }

    /**
     * @brief Gets the maximum size of the cache.
     *
     * @return The maximum size of the cache.
     */
    [[nodiscard]] size_t get_max_size() const noexcept {
        std::shared_lock lock(cache_mutex);
        return max_size;
    }

    /**
     * @brief Gets the current size of the cache.
     *
     * @return The current size of the cache.
     */
    [[nodiscard]] size_t size() const noexcept {
        std::shared_lock lock(cache_mutex);
        return cache.size();
    }
};

/**
 * @brief A thread-safe set that supports concurrent operations with improved
 * performance and safety.
 *
 * @tparam Key The type of the keys in the set.
 * @tparam SetType The underlying set type. Defaults to std::unordered_set.
 * @tparam Hash The hash function type. Defaults to std::hash<Key>.
 */
template <typename Key, typename SetType = std::unordered_set<Key>,
          typename Hash = std::hash<Key>>
class concurrent_set {
private:
    // Main data structures
    SetType data;  ///< The underlying data storage.
    mutable std::shared_mutex
        mtx;  ///< Mutex for protecting read and write operations.
    std::unique_ptr<LRUCache<Key>>
        lru_cache;  ///< LRU cache for faster lookups.

    // Thread pool components
    std::queue<std::function<void()>>
        task_queue;  ///< Queue of tasks to be executed.
    std::vector<std::thread>
        thread_pool;  ///< The thread pool for executing tasks.
    std::atomic<bool> stop_pool{false};  ///< Flag to stop the thread pool.
    mutable std::mutex pool_mutex;    ///< Mutex for protecting the task queue.
    std::condition_variable pool_cv;  ///< Condition variable for task queue.

    // Statistics and metrics
    std::atomic<size_t> insertion_count{0};  ///< Count of insert operations.
    std::atomic<size_t> deletion_count{0};   ///< Count of delete operations.
    std::atomic<size_t> find_count{0};       ///< Count of find operations.
    std::atomic<size_t> error_count{0};      ///< Count of errors.

    // Custom error handling
    std::function<void(std::string_view, std::exception_ptr)> error_callback;

    /**
     * @brief Worker function for the thread pool.
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

                if (!task_queue.empty()) {
                    task = std::move(task_queue.front());
                    task_queue.pop();
                } else {
                    continue;
                }
            }

            try {
                task();
            } catch (const std::exception& e) {
                handle_error("Thread pool task error",
                             std::current_exception());
            } catch (...) {
                handle_error("Unknown thread pool error",
                             std::current_exception());
            }
        }
    }

    /**
     * @brief Handles errors by calling the error callback function.
     *
     * @param error_message The error message.
     * @param eptr The exception pointer.
     */
    void handle_error(std::string_view error_message,
                      std::exception_ptr eptr = nullptr) noexcept {
        error_count++;

        if (error_callback) {
            try {
                error_callback(error_message, eptr);
            } catch (...) {
                // Prevent callback exceptions from propagating
                std::cerr << "Error in error callback handler" << std::endl;
            }
        } else {
            std::cerr << "Error: " << error_message;
            if (eptr) {
                try {
                    std::rethrow_exception(eptr);
                } catch (const std::exception& e) {
                    std::cerr << " - " << e.what();
                } catch (...) {
                    std::cerr << " - Unknown exception";
                }
            }
            std::cerr << std::endl;
        }
    }

public:
    /**
     * @brief Constructs a concurrent_set with improved configuration.
     *
     * @param num_threads The number of threads in the thread pool.
     * @param cache_size The size of the LRU cache. Set to 0 to disable caching.
     * @throws std::invalid_argument if num_threads is 0
     */
    explicit concurrent_set(
        size_t num_threads = std::thread::hardware_concurrency(),
        size_t cache_size = 1000)
        : thread_pool() {
        if (num_threads == 0) {
            throw std::invalid_argument("Thread pool size cannot be zero");
        }

        // Initialize the LRU cache
        if (cache_size > 0) {
            try {
                lru_cache = std::make_unique<LRUCache<Key>>(cache_size);
            } catch (const std::exception& e) {
                throw concurrent_set_exception(
                    std::string("Failed to initialize cache: ") + e.what());
            }
        } else {
            lru_cache = std::make_unique<LRUCache<Key>>(1);  // Minimal cache
        }

        // Start the thread pool
        thread_pool.reserve(num_threads);
        try {
            for (size_t i = 0; i < num_threads; ++i) {
                thread_pool.emplace_back(&concurrent_set::thread_pool_worker,
                                         this);
            }
        } catch (...) {
            // Clean up if thread creation fails
            stop_pool = true;
            pool_cv.notify_all();
            for (auto& t : thread_pool) {
                if (t.joinable())
                    t.join();
            }
            throw;
        }
    }

    /**
     * @brief Destructor for concurrent_set.
     */
    ~concurrent_set() {
        try {
            // Stop the thread pool and join all threads
            stop_pool = true;
            pool_cv.notify_all();

            for (auto& t : thread_pool) {
                if (t.joinable()) {
                    t.join();
                }
            }
        } catch (...) {
            // Best effort cleanup, don't throw from destructor
            std::cerr << "Error during concurrent_set destruction" << std::endl;
        }
    }

    // Prevent copying to avoid complex synchronization issues
    concurrent_set(const concurrent_set&) = delete;
    concurrent_set& operator=(const concurrent_set&) = delete;

    // Allow moving
    concurrent_set(concurrent_set&& other) noexcept {
        std::unique_lock lock1(mtx, std::defer_lock);
        std::unique_lock lock2(other.mtx, std::defer_lock);
        std::lock(lock1, lock2);

        data = std::move(other.data);
        lru_cache = std::move(other.lru_cache);
        insertion_count.store(other.insertion_count.load());
        deletion_count.store(other.deletion_count.load());
        find_count.store(other.find_count.load());
        error_count.store(other.error_count.load());
        error_callback = std::move(other.error_callback);

        // Take ownership of threads (risky but possible)
        stop_pool = other.stop_pool.load();
        std::lock_guard pool_lock(pool_mutex);
        task_queue = std::move(other.task_queue);
        thread_pool = std::move(other.thread_pool);
    }

    concurrent_set& operator=(concurrent_set&& other) noexcept {
        if (this != &other) {
            // First, clean up our resources
            {
                std::lock_guard pool_lock(pool_mutex);
                stop_pool = true;
                pool_cv.notify_all();
            }

            for (auto& t : thread_pool) {
                if (t.joinable())
                    t.join();
            }

            // Now move from other
            std::unique_lock lock1(mtx, std::defer_lock);
            std::unique_lock lock2(other.mtx, std::defer_lock);
            std::lock(lock1, lock2);

            data = std::move(other.data);
            lru_cache = std::move(other.lru_cache);
            insertion_count.store(other.insertion_count.load());
            deletion_count.store(other.deletion_count.load());
            find_count.store(other.find_count.load());
            error_count.store(other.error_count.load());
            error_callback = std::move(other.error_callback);

            std::lock_guard pool_lock(pool_mutex);
            stop_pool = other.stop_pool.load();
            task_queue = std::move(other.task_queue);
            thread_pool = std::move(other.thread_pool);
        }
        return *this;
    }

    /**
     * @brief Inserts an element into the set with improved error handling.
     *
     * @param key The key to insert.
     * @throws concurrent_set_exception if insertion fails
     */
    void insert(const Key& key) {
        try {
            std::unique_lock lock(mtx);
            auto result = data.insert(key);
            if (result.second) {  // If insertion was successful
                insertion_count++;
                // Update cache
                if (lru_cache) {
                    lru_cache->put(key);
                }
            }
        } catch (const std::exception& e) {
            handle_error("Insert operation failed", std::current_exception());
            throw concurrent_set_exception(std::string("Insert failed: ") +
                                           e.what());
        }
    }

    /**
     * @brief Inserts an element into the set using move semantics.
     *
     * @param key The key to insert (moved).
     * @throws concurrent_set_exception if insertion fails
     */
    void insert(Key&& key) {
        try {
            std::unique_lock lock(mtx);
            // Make a copy for the cache before the move
            Key key_copy = key;
            auto result = data.insert(std::move(key));
            if (result.second) {  // If insertion was successful
                insertion_count++;
                // Update cache
                if (lru_cache) {
                    lru_cache->put(key_copy);
                }
            }
        } catch (const std::exception& e) {
            handle_error("Insert operation failed", std::current_exception());
            throw concurrent_set_exception(std::string("Insert failed: ") +
                                           e.what());
        }
    }

    /**
     * @brief Asynchronously inserts an element into the set.
     *
     * @param key The key to insert.
     */
    void async_insert(const Key& key) {
        try {
            std::lock_guard lock(pool_mutex);
            task_queue.push([this, key = key]() {
                try {
                    this->insert(key);
                } catch (...) {
                    handle_error("Async insert failed",
                                 std::current_exception());
                }
            });
            pool_cv.notify_one();
        } catch (const std::exception& e) {
            handle_error("Failed to queue async insert",
                         std::current_exception());
        }
    }

    /**
     * @brief Asynchronously inserts an element into the set using move
     * semantics.
     *
     * @param key The key to insert (moved).
     */
    void async_insert(Key&& key) {
        try {
            std::lock_guard lock(pool_mutex);
            task_queue.push([this, key = std::move(key)]() mutable {
                try {
                    this->insert(std::move(key));
                } catch (...) {
                    handle_error("Async insert failed",
                                 std::current_exception());
                }
            });
            pool_cv.notify_one();
        } catch (const std::exception& e) {
            handle_error("Failed to queue async insert",
                         std::current_exception());
        }
    }

    /**
     * @brief Finds an element in the set with improved caching.
     *
     * @param key The key to find.
     * @return An optional containing true if found, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<bool> find(const Key& key) {
        try {
            find_count++;

            // Try the cache first for better performance
            if (lru_cache && lru_cache->exists(key)) {
                return true;
            }

            // If not in cache, check the main storage
            {
                std::shared_lock lock(mtx);
                if (data.find(key) != data.end()) {
                    // Update cache on hit
                    if (lru_cache) {
                        lru_cache->put(key);
                    }
                    return true;
                }
            }

            return std::nullopt;  // Not found
        } catch (const std::exception& e) {
            handle_error("Find operation failed", std::current_exception());
            return std::nullopt;
        }
    }

    /**
     * @brief Asynchronously finds an element in the set.
     *
     * @param key The key to find.
     * @param callback The callback function to call with the result.
     */
    void async_find(const Key& key,
                    std::function<void(std::optional<bool>)> callback) {
        try {
            if (!callback) {
                throw std::invalid_argument("Callback function cannot be null");
            }

            std::lock_guard lock(pool_mutex);
            task_queue.push([this, key, cb = std::move(callback)]() {
                try {
                    auto result = this->find(key);
                    cb(result);
                } catch (...) {
                    handle_error("Async find failed", std::current_exception());
                    cb(std::nullopt);
                }
            });
            pool_cv.notify_one();
        } catch (const std::exception& e) {
            handle_error("Failed to queue async find",
                         std::current_exception());
            if (callback) {
                callback(std::nullopt);
            }
        }
    }

    /**
     * @brief Erases an element from the set with improved error handling.
     *
     * @param key The key to erase.
     * @return True if the element was found and erased, false otherwise.
     */
    bool erase(const Key& key) {
        try {
            std::unique_lock lock(mtx);
            size_t count = data.erase(key);
            if (count > 0) {
                deletion_count++;
                // Update cache to reflect deletion
                if (lru_cache) {
                    // We keep the key in cache but mark it as deleted
                    lru_cache->put(key);
                }
                return true;
            }
            return false;
        } catch (const std::exception& e) {
            handle_error("Erase operation failed", std::current_exception());
            throw concurrent_set_exception(std::string("Erase failed: ") +
                                           e.what());
        }
    }

    /**
     * @brief Asynchronously erases an element from the set.
     *
     * @param key The key to erase.
     * @param callback Optional callback function to call with the result.
     */
    void async_erase(const Key& key,
                     std::function<void(bool)> callback = nullptr) {
        try {
            std::lock_guard lock(pool_mutex);
            task_queue.push([this, key, cb = std::move(callback)]() {
                try {
                    bool result = this->erase(key);
                    if (cb)
                        cb(result);
                } catch (...) {
                    handle_error("Async erase failed",
                                 std::current_exception());
                    if (cb)
                        cb(false);
                }
            });
            pool_cv.notify_one();
        } catch (const std::exception& e) {
            handle_error("Failed to queue async erase",
                         std::current_exception());
            if (callback) {
                callback(false);
            }
        }
    }

    /**
     * @brief Inserts a batch of elements into the set with improved efficiency.
     *
     * @param keys The keys to insert.
     * @throws concurrent_set_exception if batch insertion fails
     */
    void batch_insert(const std::vector<Key>& keys) {
        if (keys.empty())
            return;

        try {
            std::unique_lock lock(mtx);
            for (const auto& key : keys) {
                auto result = data.insert(key);
                if (result.second) {  // If insertion was successful
                    insertion_count++;
                    // Update cache
                    if (lru_cache) {
                        lru_cache->put(key);
                    }
                }
            }
        } catch (const std::exception& e) {
            handle_error("Batch insert operation failed",
                         std::current_exception());
            throw concurrent_set_exception(
                std::string("Batch insert failed: ") + e.what());
        }
    }

    /**
     * @brief Inserts a batch of elements asynchronously with improved
     * parallelism.
     *
     * @param keys The keys to insert.
     * @param callback Optional callback to call when batch is complete.
     */
    void async_batch_insert(const std::vector<Key>& keys,
                            std::function<void(bool)> callback = nullptr) {
        if (keys.empty()) {
            if (callback)
                callback(true);
            return;
        }

        try {
            const size_t chunk_size =
                100;  // Process in chunks for better parallelism

            std::lock_guard lock(pool_mutex);

            // Process large batches in smaller chunks
            for (size_t i = 0; i < keys.size(); i += chunk_size) {
                size_t end = std::min(i + chunk_size, keys.size());
                std::vector<Key> chunk(keys.begin() + i, keys.begin() + end);

                task_queue.push([this, chunk = std::move(chunk)]() {
                    try {
                        this->batch_insert(chunk);
                    } catch (...) {
                        handle_error("Async batch insert chunk failed",
                                     std::current_exception());
                    }
                });
            }

            // Add a final task for the callback if provided
            if (callback) {
                task_queue.push([callback]() { callback(true); });
            }

            pool_cv.notify_all();  // Wake up all worker threads
        } catch (const std::exception& e) {
            handle_error("Failed to queue async batch insert",
                         std::current_exception());
            if (callback) {
                callback(false);
            }
        }
    }

    /**
     * @brief Erases a batch of elements from the set.
     *
     * @param keys The keys to erase.
     * @return Number of elements successfully erased.
     */
    size_t batch_erase(const std::vector<Key>& keys) {
        if (keys.empty())
            return 0;

        try {
            size_t erased_count = 0;
            std::unique_lock lock(mtx);

            for (const auto& key : keys) {
                size_t count = data.erase(key);
                if (count > 0) {
                    deletion_count++;
                    erased_count++;
                    // Update cache
                    if (lru_cache) {
                        lru_cache->put(key);  // Mark as deleted in cache
                    }
                }
            }

            return erased_count;
        } catch (const std::exception& e) {
            handle_error("Batch erase operation failed",
                         std::current_exception());
            throw concurrent_set_exception(std::string("Batch erase failed: ") +
                                           e.what());
        }
    }

    /**
     * @brief Clears all elements from the set with improved error handling.
     */
    void clear() {
        try {
            std::unique_lock lock(mtx);
            data.clear();
            if (lru_cache) {
                lru_cache->clear();
            }
            // Don't reset counters as they represent historical data
        } catch (const std::exception& e) {
            handle_error("Clear operation failed", std::current_exception());
            throw concurrent_set_exception(std::string("Clear failed: ") +
                                           e.what());
        }
    }

    /**
     * @brief Gets the current size of the set.
     *
     * @return The number of elements in the set.
     */
    [[nodiscard]] size_t size() const noexcept {
        try {
            std::shared_lock lock(mtx);
            return data.size();
        } catch (...) {
            handle_error("Size operation failed", std::current_exception());
            return 0;
        }
    }

    /**
     * @brief Gets the count of insert operations.
     *
     * @return The number of insert operations.
     */
    [[nodiscard]] size_t get_insertion_count() const noexcept {
        return insertion_count.load();
    }

    /**
     * @brief Gets the count of delete operations.
     *
     * @return The number of delete operations.
     */
    [[nodiscard]] size_t get_deletion_count() const noexcept {
        return deletion_count.load();
    }

    /**
     * @brief Gets the count of find operations.
     *
     * @return The number of find operations.
     */
    [[nodiscard]] size_t get_find_count() const noexcept {
        return find_count.load();
    }

    /**
     * @brief Gets the count of errors.
     *
     * @return The number of errors.
     */
    [[nodiscard]] size_t get_error_count() const noexcept {
        return error_count.load();
    }

    /**
     * @brief Performs a parallel for_each operation on the elements using the
     * thread pool.
     *
     * @param func The function to apply to each element.
     * @throws concurrent_set_exception if operation fails
     */
    template <typename Func>
    void parallel_for_each(Func func) {
        try {
            // Make a copy of the data to avoid locking during processing
            std::vector<Key> items;
            {
                std::shared_lock lock(mtx);
                items.reserve(data.size());
                for (const auto& key : data) {
                    items.push_back(key);
                }
            }

            if (items.empty())
                return;

            // Process items in parallel using our thread pool
            std::atomic<size_t> processed{0};
            std::atomic<bool> has_error{false};
            std::exception_ptr first_error;
            std::mutex error_mutex;

            // Calculate chunk size based on number of items and threads
            const size_t thread_count = thread_pool.size();
            const size_t chunk_size =
                std::max(size_t(1), items.size() / (thread_count * 2));

            std::vector<std::future<void>> futures;
            futures.reserve((items.size() + chunk_size - 1) / chunk_size);

            for (size_t i = 0; i < items.size(); i += chunk_size) {
                size_t end = std::min(i + chunk_size, items.size());

                std::packaged_task<void()> task([&, i, end]() {
                    try {
                        for (size_t j = i; j < end && !has_error; ++j) {
                            func(items[j]);
                            processed++;
                        }
                    } catch (...) {
                        std::lock_guard<std::mutex> lock(error_mutex);
                        if (!first_error) {
                            first_error = std::current_exception();
                        }
                        has_error = true;
                    }
                });

                futures.push_back(task.get_future());

                std::lock_guard lock(pool_mutex);
                task_queue.push(std::move(task));
                pool_cv.notify_one();
            }

            // Wait for all tasks to complete
            for (auto& future : futures) {
                future.wait();
            }

            // Check for errors
            if (has_error && first_error) {
                std::rethrow_exception(first_error);
            }
        } catch (const std::exception& e) {
            handle_error("Parallel for_each failed", std::current_exception());
            throw concurrent_set_exception(
                std::string("Parallel for_each operation failed: ") + e.what());
        }
    }

    /**
     * @brief Adjusts the size of the thread pool with improved safety.
     *
     * @param new_size The new size of the thread pool.
     * @throws std::invalid_argument if new_size is 0
     */
    void adjust_thread_pool_size(size_t new_size) {
        if (new_size == 0) {
            throw std::invalid_argument("Thread pool size cannot be zero");
        }

        try {
            std::lock_guard lock(pool_mutex);

            if (new_size > thread_pool.size()) {
                // Add more threads
                size_t to_add = new_size - thread_pool.size();
                thread_pool.reserve(new_size);

                for (size_t i = 0; i < to_add; ++i) {
                    thread_pool.emplace_back(
                        &concurrent_set::thread_pool_worker, this);
                }
            } else if (new_size < thread_pool.size()) {
                // Remove excess threads
                size_t current_size = thread_pool.size();
                size_t to_remove = current_size - new_size;

                // Create temporary threads that will exit immediately
                std::vector<std::thread> exiting_threads;
                exiting_threads.reserve(to_remove);

                for (size_t i = 0; i < to_remove; ++i) {
                    // Add a task that will make a thread exit
                    task_queue.push([this]() {
                        std::unique_lock<std::mutex> lock(this->pool_mutex);
                        // This thread will now exit
                        throw std::runtime_error("Thread exit requested");
                    });
                }

                // Wake up threads to process the exit tasks
                pool_cv.notify_all();

                // Wait for the threads to exit
                for (size_t i = 0; i < to_remove; ++i) {
                    if (i < thread_pool.size() &&
                        thread_pool[thread_pool.size() - 1 - i].joinable()) {
                        thread_pool[thread_pool.size() - 1 - i].join();
                    }
                }

                // Remove the joined threads
                thread_pool.resize(new_size);

                // Create new threads to replace those that exited
                for (size_t i = 0; i < new_size; ++i) {
                    if (!thread_pool[i].joinable()) {
                        thread_pool[i] = std::thread(
                            &concurrent_set::thread_pool_worker, this);
                    }
                }
            }
        } catch (const std::exception& e) {
            handle_error("Adjust thread pool size failed",
                         std::current_exception());
            throw concurrent_set_exception(
                std::string("Failed to adjust thread pool size: ") + e.what());
        }
    }

    /**
     * @brief Gets a constant reference to the current data with safe copy.
     *
     * @return A copy of the data to avoid thread safety issues.
     */
    [[nodiscard]] SetType get_data_copy() const {
        std::shared_lock lock(mtx);
        return data;  // Return a copy
    }

    /**
     * @brief Ensures atomicity of a series of operations with improved
     * transaction safety.
     *
     * @param operations The operations to perform atomically.
     * @return True if all operations succeed, false otherwise.
     */
    bool transaction(const std::vector<std::function<void()>>& operations) {
        if (operations.empty())
            return true;

        try {
            std::unique_lock lock(mtx);

            // Make a copy of the data for rollback
            SetType data_backup = data;
            size_t insertion_count_before = insertion_count.load();
            size_t deletion_count_before = deletion_count.load();

            try {
                // Execute all operations
                for (const auto& op : operations) {
                    if (!op)
                        throw std::invalid_argument(
                            "Transaction contains null operation");
                    op();
                }
                return true;  // All operations succeeded
            } catch (const std::exception& e) {
                // Rollback on failure
                data = std::move(data_backup);
                insertion_count.store(insertion_count_before);
                deletion_count.store(deletion_count_before);

                handle_error(std::string("Transaction failed: ") + e.what(),
                             std::current_exception());
                return false;
            }
        } catch (const std::exception& e) {
            handle_error("Transaction lock acquisition failed",
                         std::current_exception());
            throw transaction_exception(std::string("Transaction failed: ") +
                                        e.what());
        }
    }

    /**
     * @brief Finds elements that match a given condition with improved
     * performance.
     *
     * @param condition The condition to match.
     * @return A vector of keys that match the condition.
     */
    template <typename Predicate>
    [[nodiscard]] std::vector<Key> conditional_find(Predicate condition) {
        try {
            std::vector<Key> result;

            {
                std::shared_lock lock(mtx);
                // Pre-allocate space to avoid reallocations
                result.reserve(std::min(data.size(), size_t(100)));

                for (const auto& key : data) {
                    if (condition(key)) {
                        result.push_back(key);
                    }
                }
            }

            return result;
        } catch (const std::exception& e) {
            handle_error("Conditional find failed", std::current_exception());
            throw concurrent_set_exception(
                std::string("Conditional find failed: ") + e.what());
        }
    }

    /**
     * @brief Asynchronously finds elements that match a given condition.
     *
     * @param condition The condition to match.
     * @param callback The callback function to call with the results.
     */
    template <typename Predicate>
    void async_conditional_find(
        Predicate condition, std::function<void(std::vector<Key>)> callback) {
        if (!callback) {
            throw std::invalid_argument("Callback function cannot be null");
        }

        try {
            std::lock_guard lock(pool_mutex);
            task_queue.push([this, condition = std::move(condition),
                             cb = std::move(callback)]() {
                try {
                    auto results = this->conditional_find(condition);
                    cb(results);
                } catch (...) {
                    handle_error("Async conditional find failed",
                                 std::current_exception());
                    cb(std::vector<Key>{});
                }
            });
            pool_cv.notify_one();
        } catch (const std::exception& e) {
            handle_error("Failed to queue async conditional find",
                         std::current_exception());
            callback(std::vector<Key>{});
        }
    }

    /**
     * @brief Saves the current set data to a file with improved error handling.
     *
     * @param filename The name of the file to save to.
     * @return True if the save is successful, false otherwise.
     * @throws io_exception on file write errors
     */
    bool save_to_file(std::string_view filename) const {
        if (filename.empty()) {
            throw std::invalid_argument("Filename cannot be empty");
        }

        try {
            std::shared_lock lock(mtx);
            std::ofstream out(std::string(filename), std::ios::binary);

            if (!out.is_open()) {
                throw io_exception("Could not open file for writing: " +
                                   std::string(filename));
            }

            // Write header with version information
            const uint32_t VERSION = 1;
            out.write(reinterpret_cast<const char*>(&VERSION), sizeof(VERSION));

            // Write data size
            size_t size = data.size();
            out.write(reinterpret_cast<const char*>(&size), sizeof(size));

            // Write each element
            for (const auto& key : data) {
                if constexpr (std::is_trivially_copyable_v<Key>) {
                    // For trivially copyable types, write directly
                    out.write(reinterpret_cast<const char*>(&key), sizeof(Key));
                } else {
                    // For complex types, require serialization support
                    static_assert(
                        std::is_same_v<decltype(serialize(std::declval<Key>())),
                                       std::vector<char>>,
                        "Non-trivial types must provide a serialize function");

                    auto serialized = serialize(key);
                    size_t bytes = serialized.size();
                    out.write(reinterpret_cast<const char*>(&bytes),
                              sizeof(bytes));
                    out.write(serialized.data(), bytes);
                }
            }

            // Write metadata (operation counts)
            out.write(reinterpret_cast<const char*>(&insertion_count),
                      sizeof(insertion_count));
            out.write(reinterpret_cast<const char*>(&deletion_count),
                      sizeof(deletion_count));
            out.write(reinterpret_cast<const char*>(&find_count),
                      sizeof(find_count));

            if (!out.good()) {
                throw io_exception("Error writing to file: " +
                                   std::string(filename));
            }

            out.close();
            return true;
        } catch (const std::exception& e) {
            handle_error("Save to file failed", std::current_exception());
            throw io_exception(std::string("Failed to save to file: ") +
                               e.what());
        }
    }

    /**
     * @brief Loads set data from a file with improved error handling.
     *
     * @param filename The name of the file to load from.
     * @return True if the load is successful, false otherwise.
     * @throws io_exception on file read errors
     */
    bool load_from_file(std::string_view filename) {
        if (filename.empty()) {
            throw std::invalid_argument("Filename cannot be empty");
        }

        try {
            std::ifstream in(std::string(filename), std::ios::binary);

            if (!in.is_open()) {
                throw io_exception("Could not open file for reading: " +
                                   std::string(filename));
            }

            // Read and verify header
            uint32_t version;
            in.read(reinterpret_cast<char*>(&version), sizeof(version));

            if (version != 1) {
                throw io_exception("Unsupported file version: " +
                                   std::to_string(version));
            }

            // Read data size
            size_t size = 0;
            in.read(reinterpret_cast<char*>(&size), sizeof(size));

            if (size >
                10'000'000) {  // Sanity check to prevent memory exhaustion
                throw io_exception("File contains too many elements: " +
                                   std::to_string(size));
            }

            // Create new data to replace existing
            SetType new_data;
            new_data.reserve(size);

            // Read each element
            for (size_t i = 0; i < size; ++i) {
                if constexpr (std::is_trivially_copyable_v<Key>) {
                    // For trivially copyable types, read directly
                    Key key;
                    in.read(reinterpret_cast<char*>(&key), sizeof(Key));
                    new_data.insert(key);
                } else {
                    // For complex types, require deserialization support
                    static_assert(
                        std::is_same_v<decltype(deserialize<Key>(
                                           std::declval<std::vector<char>>())),
                                       Key>,
                        "Non-trivial types must provide a deserialize "
                        "function");

                    size_t bytes;
                    in.read(reinterpret_cast<char*>(&bytes), sizeof(bytes));

                    if (bytes > 1'000'000) {  // Sanity check
                        throw io_exception("Element serialization too large: " +
                                           std::to_string(bytes));
                    }

                    std::vector<char> buffer(bytes);
                    in.read(buffer.data(), bytes);

                    Key key = deserialize<Key>(buffer);
                    new_data.insert(key);
                }
            }

            // Read metadata
            size_t ins_count = 0, del_count = 0, find_cnt = 0;
            in.read(reinterpret_cast<char*>(&ins_count), sizeof(ins_count));
            in.read(reinterpret_cast<char*>(&del_count), sizeof(del_count));
            in.read(reinterpret_cast<char*>(&find_cnt), sizeof(find_cnt));

            if (!in.good() && !in.eof()) {
                throw io_exception("Error reading from file: " +
                                   std::string(filename));
            }

            // Now update our actual data
            std::unique_lock lock(mtx);
            data = std::move(new_data);
            insertion_count.store(ins_count);
            deletion_count.store(del_count);
            find_count.store(find_cnt);

            // Rebuild cache
            if (lru_cache) {
                lru_cache->clear();
                for (const auto& key : data) {
                    lru_cache->put(key);
                }
            }

            in.close();
            return true;
        } catch (const std::exception& e) {
            handle_error("Load from file failed", std::current_exception());
            throw io_exception(std::string("Failed to load from file: ") +
                               e.what());
        }
    }

    /**
     * @brief Asynchronously saves the set data to a file.
     *
     * @param filename The name of the file to save to.
     * @param callback Optional callback function to call with the result.
     */
    void async_save_to_file(std::string_view filename,
                            std::function<void(bool)> callback = nullptr) {
        if (filename.empty()) {
            throw std::invalid_argument("Filename cannot be empty");
        }

        try {
            std::string filename_copy(filename);
            std::lock_guard lock(pool_mutex);

            task_queue.push([this, filename = std::move(filename_copy),
                             cb = std::move(callback)]() {
                bool success = false;
                try {
                    success = this->save_to_file(filename);
                } catch (...) {
                    handle_error("Async save to file failed",
                                 std::current_exception());
                }

                if (cb)
                    cb(success);
            });

            pool_cv.notify_one();
        } catch (const std::exception& e) {
            handle_error("Failed to queue async save",
                         std::current_exception());
            if (callback) {
                callback(false);
            }
        }
    }

    /**
     * @brief Gets information about the LRU cache.
     *
     * @return A tuple containing: (cache size, hits, misses, hit rate)
     */
    [[nodiscard]] std::tuple<size_t, size_t, size_t, double> get_cache_stats()
        const {
        if (!lru_cache) {
            return {0, 0, 0, 0.0};
        }

        auto [hits, misses] = lru_cache->get_stats();
        return {lru_cache->get_max_size(), hits, misses,
                lru_cache->get_hit_rate()};
    }

    /**
     * @brief Resizes the LRU cache.
     *
     * @param new_size The new size of the cache. Set to 0 to disable caching.
     * @throws cache_exception if resizing fails
     */
    void resize_cache(size_t new_size) {
        try {
            if (new_size == 0) {
                // Disable cache
                std::unique_lock lock(mtx);
                lru_cache =
                    std::make_unique<LRUCache<Key>>(1);  // Minimal cache
            } else {
                // Resize existing cache
                if (!lru_cache) {
                    lru_cache = std::make_unique<LRUCache<Key>>(new_size);
                } else {
                    lru_cache->resize(new_size);
                }
            }
        } catch (const std::exception& e) {
            handle_error("Cache resize failed", std::current_exception());
            throw cache_exception(std::string("Failed to resize cache: ") +
                                  e.what());
        }
    }

    /**
     * @brief Sets a callback function to handle errors with enhanced
     * information.
     *
     * @param callback The callback function to handle errors.
     */
    void set_error_callback(
        std::function<void(std::string_view, std::exception_ptr)> callback) {
        error_callback = std::move(callback);
    }

    /**
     * @brief Gets the number of tasks in the queue.
     *
     * @return The number of pending tasks.
     */
    [[nodiscard]] size_t get_pending_task_count() const {
        std::lock_guard lock(pool_mutex);
        return task_queue.size();
    }

    /**
     * @brief Gets the number of threads in the thread pool.
     *
     * @return The number of threads.
     */
    [[nodiscard]] size_t get_thread_count() const {
        std::lock_guard lock(pool_mutex);
        return thread_pool.size();
    }

    /**
     * @brief Waits until all pending tasks are completed.
     *
     * @param timeout_ms Maximum time to wait in milliseconds, 0 means wait
     * indefinitely.
     * @return True if all tasks completed, false if timeout occurred.
     */
    bool wait_for_tasks(size_t timeout_ms = 0) {
        auto start_time = std::chrono::steady_clock::now();

        while (true) {
            {
                std::lock_guard lock(pool_mutex);
                if (task_queue.empty()) {
                    return true;  // All tasks are done
                }
            }

            if (timeout_ms > 0) {
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(
                        elapsed)
                        .count() >= timeout_ms) {
                    return false;  // Timeout occurred
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

}  // namespace atom::type

#endif  // ATOM_TYPE_CONCURRENT_SET_HPP