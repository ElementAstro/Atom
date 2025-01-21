#ifndef ATOM_TYPE_CONCURRENT_VECTOR_HPP
#define ATOM_TYPE_CONCURRENT_VECTOR_HPP

#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <future>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace atom::type {

/**
 * @brief A thread-safe vector that supports concurrent operations.
 *
 * This class provides a vector-like container that supports concurrent
 * read and write operations. It also includes a thread pool for executing
 * tasks in parallel.
 *
 * @tparam T The type of elements stored in the vector.
 */
template <typename T>
class concurrent_vector {
private:
    std::vector<T> data;       ///< The underlying data storage.
    std::atomic<size_t> size;  ///< The current number of valid elements.
    mutable std::shared_mutex
        mtx;  ///< Mutex for protecting read and write operations.
    std::queue<std::function<void()>>
        task_queue;  ///< Queue of tasks to be executed by the thread pool.
    std::vector<std::thread>
        thread_pool;         ///< The thread pool for executing tasks.
    bool stop_pool = false;  ///< Flag to stop the thread pool.

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
     * @brief Constructs a concurrent_vector with a specified number of threads.
     *
     * @param num_threads The number of threads in the thread pool. Defaults to
     * the number of hardware threads.
     */
    concurrent_vector(size_t num_threads = std::thread::hardware_concurrency())
        : size(0) {
        // Start the thread pool
        for (size_t i = 0; i < num_threads; ++i) {
            thread_pool.push_back(
                std::thread(&concurrent_vector::thread_pool_worker, this));
        }
    }

    /**
     * @brief Destructor for concurrent_vector.
     *
     * Stops the thread pool and joins all threads.
     */
    ~concurrent_vector() {
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
     * @brief Gets the current size of the vector.
     *
     * @return The number of valid elements in the vector.
     */
    size_t get_size() const { return size.load(std::memory_order_relaxed); }

    /**
     * @brief Thread-safe push_back operation.
     *
     * Adds an element to the end of the vector.
     *
     * @param value The value to be added.
     */
    void push_back(const T& value) {
        std::unique_lock lock(mtx);
        size_t current_size = size.load(std::memory_order_relaxed);
        if (current_size >= data.size()) {
            data.push_back(value);  // Expand data
        } else {
            data[current_size] = value;  // Overwrite if no expansion is needed
        }
        size.store(current_size + 1, std::memory_order_release);  // Update size
    }

    /**
     * @brief Thread-safe push_back operation (rvalue version).
     *
     * Adds an element to the end of the vector.
     *
     * @param value The value to be added.
     */
    void push_back(T&& value) {
        std::unique_lock lock(mtx);
        size_t current_size = size.load(std::memory_order_relaxed);
        if (current_size >= data.size()) {
            data.push_back(std::move(value));  // Expand data
        } else {
            data[current_size] =
                std::move(value);  // Overwrite if no expansion is needed
        }
        size.store(current_size + 1, std::memory_order_release);  // Update size
    }

    /**
     * @brief Thread-safe pop_back operation.
     *
     * Removes the last element from the vector.
     */
    void pop_back() {
        std::unique_lock lock(mtx);
        size_t current_size = size.load(std::memory_order_relaxed);
        if (current_size > 0) {
            size.store(current_size - 1, std::memory_order_release);
        }
    }

    /**
     * @brief Gets an element at the specified index.
     *
     * This operation is thread-safe for reading.
     *
     * @param index The index of the element to retrieve.
     * @return A reference to the element at the specified index.
     */
    T& operator[](size_t index) {
        assert(index < size.load(std::memory_order_relaxed));
        std::shared_lock lock(mtx);
        return data[index];
    }

    /**
     * @brief Gets a constant reference to an element at the specified index.
     *
     * This operation is thread-safe for reading.
     *
     * @param index The index of the element to retrieve.
     * @return A constant reference to the element at the specified index.
     */
    const T& operator[](size_t index) const {
        assert(index < size.load(std::memory_order_relaxed));
        std::shared_lock lock(mtx);
        return data[index];
    }

    /**
     * @brief Performs a parallel for_each operation on the elements.
     *
     * Applies the given function to each element in the vector in parallel.
     *
     * @param func The function to apply to each element.
     */
    void parallel_for_each(std::function<void(T&)> func) {
        size_t current_size = size.load(std::memory_order_relaxed);
        std::vector<std::future<void>> futures;

        // Process data in parallel
        for (size_t i = 0; i < current_size; ++i) {
            futures.push_back(std::async(std::launch::async,
                                         [this, i, func] { func(data[i]); }));
        }

        // Wait for all threads to complete
        for (auto& future : futures) {
            future.get();
        }
    }

    /**
     * @brief Inserts a batch of elements into the vector.
     *
     * This operation is thread-safe.
     *
     * @param values The values to be inserted.
     */
    void batch_insert(const std::vector<T>& values) {
        std::unique_lock lock(mtx);
        size_t current_size = size.load(std::memory_order_relaxed);
        size_t new_size = current_size + values.size();

        // Expand the container
        if (new_size > data.size()) {
            data.resize(new_size);
        }

        // Insert data
        std::copy(values.begin(), values.end(), data.begin() + current_size);
        size.store(new_size, std::memory_order_release);  // Update size
    }

    /**
     * @brief Inserts a batch of elements into the vector in parallel.
     *
     * This operation is thread-safe.
     *
     * @param values The values to be inserted.
     */
    void parallel_batch_insert(const std::vector<T>& values) {
        std::unique_lock lock(mtx);
        size_t current_size = size.load(std::memory_order_relaxed);
        size_t new_size = current_size + values.size();

        // Expand the container
        if (new_size > data.size()) {
            data.resize(new_size);
        }

        // Insert data in parallel
        std::vector<std::future<void>> futures;
        for (size_t i = 0; i < values.size(); ++i) {
            futures.push_back(std::async(std::launch::async,
                                         [this, current_size, i, &values] {
                                             data[current_size + i] = values[i];
                                         }));
        }

        // Wait for all insert operations to complete
        for (auto& future : futures) {
            future.get();
        }

        size.store(new_size, std::memory_order_release);  // Update size
    }

    /**
     * @brief Clears all data from the vector.
     *
     * This operation is thread-safe.
     */
    void clear() {
        std::unique_lock lock(mtx);
        data.clear();
        size.store(0, std::memory_order_release);
    }

    /**
     * @brief Clears data in the specified range.
     *
     * This operation is thread-safe.
     *
     * @param start The start index of the range to clear.
     * @param end The end index of the range to clear.
     */
    void clear_range(size_t start, size_t end) {
        if (start >= end || end > size.load(std::memory_order_relaxed)) {
            return;  // Invalid range
        }

        std::unique_lock lock(mtx);
        std::fill(data.begin() + start, data.begin() + end,
                  T());  // Fill with default constructor
        size.store(end, std::memory_order_release);  // Update valid size
    }

    /**
     * @brief Finds an element in the vector in parallel.
     *
     * This operation is thread-safe.
     *
     * @param value The value to find.
     * @return True if the value is found, false otherwise.
     */
    bool parallel_find(const T& value) {
        size_t current_size = size.load(std::memory_order_relaxed);
        std::vector<std::future<bool>> futures;

        // Start multiple threads to find the value
        for (size_t i = 0; i < current_size; ++i) {
            futures.push_back(std::async(std::launch::async, [this, i, &value] {
                return data[i] == value;
            }));
        }

        // Wait for the find result
        for (auto& future : futures) {
            if (future.get()) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Submits a task to the thread pool.
     *
     * @param task The task to be executed.
     */
    void submit_task(std::function<void()> task) {
        {
            std::unique_lock lock(pool_mutex);
            task_queue.push(task);
        }
        pool_cv.notify_one();
    }

    /**
     * @brief Gets a constant reference to the current data.
     *
     * This operation is thread-safe for reading.
     *
     * @return A constant reference to the data.
     */
    const std::vector<T>& get_data() const {
        std::shared_lock lock(mtx);
        return data;
    }
};

}  // namespace atom::type

#endif