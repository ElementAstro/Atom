#ifndef ATOM_TYPE_CONCURRENT_VECTOR_HPP
#define ATOM_TYPE_CONCURRENT_VECTOR_HPP

#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#if defined(__cpp_lib_execution)
#include <execution>
#endif

namespace atom::type {

/**
 * @brief Custom exceptions for concurrent_vector operations
 */
class concurrent_vector_error : public std::runtime_error {
public:
    explicit concurrent_vector_error(const std::string& message)
        : std::runtime_error(message) {}
};

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
    std::vector<T> data;             ///< The underlying data storage
    std::atomic<size_t> valid_size;  ///< The current number of valid elements
    mutable std::shared_mutex mtx;   ///< Mutex for read/write operations

    // Thread pool related members
    std::queue<std::packaged_task<void()>> task_queue;
    std::vector<std::thread> thread_pool;
    std::atomic<bool> stop_pool{false};
    mutable std::mutex pool_mutex;
    std::condition_variable pool_cv;
    std::atomic<size_t> active_tasks{0};
    std::condition_variable_any tasks_done_cv;

    // Error handling related
    std::vector<std::exception_ptr> thread_exceptions;
    mutable std::mutex exception_mutex;

    /**
     * @brief Worker function for the thread pool.
     */
    void thread_pool_worker() {
        try {
            while (true) {
                std::packaged_task<void()> task;
                {
                    std::unique_lock lock(pool_mutex);
                    pool_cv.wait(lock, [this] {
                        return !task_queue.empty() ||
                               stop_pool.load(std::memory_order_acquire);
                    });

                    if (stop_pool.load(std::memory_order_acquire) &&
                        task_queue.empty()) {
                        return;
                    }

                    if (!task_queue.empty()) {
                        task = std::move(task_queue.front());
                        task_queue.pop();
                    }
                }

                if (task.valid()) {
                    active_tasks.fetch_add(1, std::memory_order_acq_rel);

                    try {
                        // Execute the task
                        task();
                    } catch (...) {
                        // Capture any exception that occurs during task
                        // execution
                        std::lock_guard<std::mutex> lock(exception_mutex);
                        thread_exceptions.push_back(std::current_exception());
                    }

                    size_t remaining =
                        active_tasks.fetch_sub(1, std::memory_order_acq_rel) -
                        1;
                    if (remaining == 0) {
                        tasks_done_cv.notify_all();
                    }
                }
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(exception_mutex);
            thread_exceptions.push_back(std::current_exception());
        }
    }

    /**
     * @brief Checks if the provided index is within valid bounds
     *
     * @param index The index to check
     * @param operation_name Name of the operation for error message
     * @throws concurrent_vector_error if index is out of bounds
     */
    void check_bounds(size_t index, const std::string& operation_name) const {
        if (index >= valid_size.load(std::memory_order_acquire)) {
            throw concurrent_vector_error(
                operation_name + ": Index " + std::to_string(index) +
                " out of bounds (size: " + std::to_string(valid_size.load()) +
                ")");
        }
    }

    /**
     * @brief Rethrow any stored exceptions from worker threads
     */
    void check_for_exceptions() {
        std::lock_guard<std::mutex> lock(exception_mutex);
        if (!thread_exceptions.empty()) {
            auto exception = thread_exceptions.front();
            thread_exceptions.clear();
            std::rethrow_exception(exception);
        }
    }

public:
    using value_type = T;
    using size_type = size_t;
    using reference = T&;
    using const_reference = const T&;

    /**
     * @brief Constructs a concurrent_vector with a specified number of threads.
     *
     * @param initial_capacity Initial capacity for the vector
     * @param num_threads The number of threads in the thread pool
     * @throws std::invalid_argument If num_threads is 0
     */
    explicit concurrent_vector(
        size_t initial_capacity = 0,
        size_t num_threads = std::thread::hardware_concurrency())
        : valid_size(0) {
        try {
            if (num_threads == 0) {
                throw std::invalid_argument(
                    "Thread count must be greater than 0");
            }

            // Reserve initial capacity if specified
            if (initial_capacity > 0) {
                data.reserve(initial_capacity);
            }

            // Start the thread pool
            thread_pool.reserve(num_threads);
            for (size_t i = 0; i < num_threads; ++i) {
                thread_pool.emplace_back(&concurrent_vector::thread_pool_worker,
                                         this);
            }
        } catch (...) {
            // Clean up if constructor fails
            stop_pool.store(true, std::memory_order_release);
            pool_cv.notify_all();
            for (auto& t : thread_pool) {
                if (t.joinable()) {
                    t.join();
                }
            }
            throw;
        }
    }

    /**
     * @brief Destructor for concurrent_vector.
     *
     * Stops the thread pool and joins all threads.
     */
    ~concurrent_vector() noexcept {
        try {
            // Stop the thread pool and join all threads
            stop_pool.store(true, std::memory_order_release);
            pool_cv.notify_all();
            for (auto& t : thread_pool) {
                if (t.joinable()) {
                    t.join();
                }
            }
        } catch (...) {
            // Suppress exceptions in destructor
        }
    }

    // Delete copy constructor and assignment operator
    concurrent_vector(const concurrent_vector&) = delete;
    concurrent_vector& operator=(const concurrent_vector&) = delete;

    /**
     * @brief Move constructor
     */
    concurrent_vector(concurrent_vector&& other) noexcept {
        std::unique_lock lock_other(other.mtx);
        std::unique_lock lock_this(mtx);

        data = std::move(other.data);
        valid_size.store(other.valid_size.load(std::memory_order_acquire),
                         std::memory_order_release);

        // We can't easily move the thread pool, so we'll create a new one
        size_t num_threads = other.thread_pool.size();
        other.stop_pool.store(true, std::memory_order_release);
        other.pool_cv.notify_all();

        // Start new thread pool
        thread_pool.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            thread_pool.emplace_back(&concurrent_vector::thread_pool_worker,
                                     this);
        }

        // Join other's threads
        for (auto& t : other.thread_pool) {
            if (t.joinable()) {
                t.join();
            }
        }
        other.thread_pool.clear();
    }

    /**
     * @brief Move assignment operator
     */
    concurrent_vector& operator=(concurrent_vector&& other) noexcept {
        if (this != &other) {
            // Clean up current resources
            stop_pool.store(true, std::memory_order_release);
            pool_cv.notify_all();
            for (auto& t : thread_pool) {
                if (t.joinable()) {
                    t.join();
                }
            }
            thread_pool.clear();

            // Move other's resources
            std::unique_lock lock_other(other.mtx);
            std::unique_lock lock_this(mtx);

            data = std::move(other.data);
            valid_size.store(other.valid_size.load(std::memory_order_acquire),
                             std::memory_order_release);

            // Reset stop flag and start new thread pool
            stop_pool.store(false, std::memory_order_release);
            size_t num_threads = other.thread_pool.size();

            // Stop other's thread pool
            other.stop_pool.store(true, std::memory_order_release);
            other.pool_cv.notify_all();

            // Start new thread pool
            thread_pool.reserve(num_threads);
            for (size_t i = 0; i < num_threads; ++i) {
                thread_pool.emplace_back(&concurrent_vector::thread_pool_worker,
                                         this);
            }

            // Join other's threads
            for (auto& t : other.thread_pool) {
                if (t.joinable()) {
                    t.join();
                }
            }
            other.thread_pool.clear();
        }
        return *this;
    }

    /**
     * @brief Gets the current size of the vector.
     *
     * @return The number of valid elements in the vector.
     */
    [[nodiscard]] size_t size() const noexcept {
        return valid_size.load(std::memory_order_acquire);
    }

    /**
     * @brief Gets the current capacity of the vector.
     *
     * @return The capacity of the underlying data container.
     */
    [[nodiscard]] size_t capacity() const {
        std::shared_lock lock(mtx);
        return data.capacity();
    }

    /**
     * @brief Checks if the vector is empty.
     *
     * @return True if the vector is empty, false otherwise.
     */
    [[nodiscard]] bool empty() const noexcept {
        return valid_size.load(std::memory_order_acquire) == 0;
    }

    /**
     * @brief Reserves space for at least the specified number of elements.
     *
     * @param new_capacity The minimum capacity to reserve.
     * @throws std::length_error If the requested capacity exceeds maximum size
     */
    void reserve(size_t new_capacity) {
        std::unique_lock lock(mtx);
        try {
            data.reserve(new_capacity);
        } catch (const std::length_error& e) {
            throw concurrent_vector_error(std::string("reserve: ") + e.what());
        }
    }

    /**
     * @brief Thread-safe push_back operation.
     *
     * @param value The value to be added.
     * @throws Any exception thrown by T's copy constructor
     */
    void push_back(const T& value) {
        std::unique_lock lock(mtx);
        try {
            size_t current_size = valid_size.load(std::memory_order_relaxed);
            if (current_size >= data.size()) {
                data.push_back(value);
            } else {
                data[current_size] = value;
            }
            valid_size.store(current_size + 1, std::memory_order_release);
        } catch (...) {
            throw concurrent_vector_error(
                "push_back: Failed to add element due to " +
                std::string(std::current_exception() ? "exception"
                                                     : "unknown reason"));
        }
    }

    /**
     * @brief Thread-safe push_back operation (rvalue version).
     *
     * @param value The value to be moved.
     * @throws Any exception thrown by T's move constructor
     */
    void push_back(T&& value) {
        std::unique_lock lock(mtx);
        try {
            size_t current_size = valid_size.load(std::memory_order_relaxed);
            if (current_size >= data.size()) {
                data.push_back(std::move(value));
            } else {
                data[current_size] = std::move(value);
            }
            valid_size.store(current_size + 1, std::memory_order_release);
        } catch (...) {
            throw concurrent_vector_error(
                "push_back: Failed to add element due to " +
                std::string(std::current_exception() ? "exception"
                                                     : "unknown reason"));
        }
    }

    /**
     * @brief Emplaces an element at the end of the vector.
     *
     * @tparam Args Types of arguments to forward to the constructor
     * @param args Arguments to forward to the constructor
     * @throws Any exception thrown by T's constructor
     */
    template <typename... Args>
    void emplace_back(Args&&... args) {
        std::unique_lock lock(mtx);
        try {
            size_t current_size = valid_size.load(std::memory_order_relaxed);
            if (current_size >= data.size()) {
                data.emplace_back(std::forward<Args>(args)...);
            } else {
                data[current_size] = T(std::forward<Args>(args)...);
            }
            valid_size.store(current_size + 1, std::memory_order_release);
        } catch (...) {
            throw concurrent_vector_error(
                "emplace_back: Failed to construct element due to " +
                std::string(std::current_exception() ? "exception"
                                                     : "unknown reason"));
        }
    }

    /**
     * @brief Thread-safe pop_back operation.
     *
     * @return Optional containing the popped value
     * @throws concurrent_vector_error If the vector is empty
     */
    std::optional<T> pop_back() {
        std::unique_lock lock(mtx);
        size_t current_size = valid_size.load(std::memory_order_relaxed);

        if (current_size == 0) {
            throw concurrent_vector_error(
                "pop_back: Cannot remove from an empty vector");
        }

        try {
            // Store the value before decreasing size
            std::optional<T> result =
                std::make_optional<T>(std::move(data[current_size - 1]));
            valid_size.store(current_size - 1, std::memory_order_release);
            return result;
        } catch (...) {
            throw concurrent_vector_error(
                "pop_back: Failed to pop element due to " +
                std::string(std::current_exception() ? "exception"
                                                     : "unknown reason"));
        }
    }

    /**
     * @brief Gets an element at the specified index.
     *
     * @param index The index of the element to retrieve.
     * @return A reference to the element at the specified index.
     * @throws concurrent_vector_error If index is out of bounds
     */
    T& at(size_t index) {
        check_bounds(index, "at");
        std::shared_lock lock(mtx);
        return data[index];
    }

    /**
     * @brief Gets a constant reference to an element at the specified index.
     *
     * @param index The index of the element to retrieve.
     * @return A constant reference to the element at the specified index.
     * @throws concurrent_vector_error If index is out of bounds
     */
    const T& at(size_t index) const {
        check_bounds(index, "at");
        std::shared_lock lock(mtx);
        return data[index];
    }

    /**
     * @brief Gets an element at the specified index (no bounds checking).
     *
     * @param index The index of the element to retrieve.
     * @return A reference to the element at the specified index.
     */
    T& operator[](size_t index) {
        assert(index < valid_size.load(std::memory_order_acquire) &&
               "Index out of range");
        std::shared_lock lock(mtx);
        return data[index];
    }

    /**
     * @brief Gets a constant reference to an element at the specified index (no
     * bounds checking).
     *
     * @param index The index of the element to retrieve.
     * @return A constant reference to the element at the specified index.
     */
    const T& operator[](size_t index) const {
        assert(index < valid_size.load(std::memory_order_acquire) &&
               "Index out of range");
        std::shared_lock lock(mtx);
        return data[index];
    }

    /**
     * @brief Performs a parallel for_each operation on the elements.
     *
     * @param func The function to apply to each element.
     * @throws Any exceptions thrown by the function
     */
    void parallel_for_each(std::function<void(T&)> func) {
        check_for_exceptions();

        size_t current_size = valid_size.load(std::memory_order_acquire);
        if (current_size == 0)
            return;

        try {
            std::vector<std::future<void>> futures;
            futures.reserve(current_size);

            // Process data in parallel
            for (size_t i = 0; i < current_size; ++i) {
                std::promise<void> promise;
                futures.push_back(promise.get_future());

                auto task = [this, i, func,
                             promise = std::move(promise)]() mutable {
                    try {
                        std::shared_lock lock(mtx);
                        func(data[i]);
                        promise.set_value();
                    } catch (...) {
                        promise.set_exception(std::current_exception());
                    }
                };

                submit_task(std::move(task));
            }

            // Wait for all tasks to complete and propagate exceptions
            for (auto& future : futures) {
                future.get();  // This will rethrow any exception
            }

        } catch (...) {
            throw concurrent_vector_error(
                "parallel_for_each: Operation failed due to exception in "
                "worker task");
        }

        check_for_exceptions();
    }

    /**
     * @brief Performs a parallel for_each operation on the elements (const
     * version).
     *
     * @param func The function to apply to each element.
     * @throws Any exceptions thrown by the function
     */
    void parallel_for_each(std::function<void(const T&)> func) const {
        check_for_exceptions();

        size_t current_size = valid_size.load(std::memory_order_acquire);
        if (current_size == 0)
            return;

        try {
            std::vector<std::future<void>> futures;
            futures.reserve(current_size);

            // Process data in parallel
            for (size_t i = 0; i < current_size; ++i) {
                std::promise<void> promise;
                futures.push_back(promise.get_future());

                auto task = [this, i, func,
                             promise = std::move(promise)]() mutable {
                    try {
                        std::shared_lock lock(mtx);
                        func(data[i]);
                        promise.set_value();
                    } catch (...) {
                        promise.set_exception(std::current_exception());
                    }
                };

                // Need const_cast because we're submitting to a non-const
                // object
                const_cast<concurrent_vector*>(this)->submit_task(
                    std::move(task));
            }

            // Wait for all tasks to complete and propagate exceptions
            for (auto& future : futures) {
                future.get();  // This will rethrow any exception
            }

        } catch (...) {
            throw concurrent_vector_error(
                "parallel_for_each: Operation failed due to exception in "
                "worker task");
        }

        check_for_exceptions();
    }

    /**
     * @brief Inserts a batch of elements into the vector.
     *
     * @param values The values to be inserted.
     * @throws Any exceptions thrown during insert
     */
    void batch_insert(const std::vector<T>& values) {
        if (values.empty())
            return;

        std::unique_lock lock(mtx);
        try {
            size_t current_size = valid_size.load(std::memory_order_relaxed);
            size_t new_size = current_size + values.size();

            // Expand the container if needed
            if (new_size > data.size()) {
                // Use growth factor for efficient resizing
                size_t new_capacity = std::max(
                    new_size,
                    data.size() *
                        2  // Double the size for better amortized complexity
                );
                data.resize(new_capacity);
            }

            // Copy the data
            std::copy(values.begin(), values.end(),
                      data.begin() + current_size);
            valid_size.store(new_size, std::memory_order_release);
        } catch (...) {
            throw concurrent_vector_error(
                "batch_insert: Failed to insert batch due to " +
                std::string(std::current_exception() ? "exception"
                                                     : "unknown reason"));
        }
    }

    /**
     * @brief Inserts a batch of elements into the vector.
     *
     * @param values The values to be moved.
     * @throws Any exceptions thrown during insert
     */
    void batch_insert(std::vector<T>&& values) {
        if (values.empty())
            return;

        std::unique_lock lock(mtx);
        try {
            size_t current_size = valid_size.load(std::memory_order_relaxed);
            size_t new_size = current_size + values.size();

            // Expand the container if needed
            if (new_size > data.size()) {
                // Use growth factor for efficient resizing
                size_t new_capacity = std::max(
                    new_size,
                    data.size() *
                        2  // Double the size for better amortized complexity
                );
                data.resize(new_capacity);
            }

            // Move the data
            std::move(values.begin(), values.end(),
                      data.begin() + current_size);
            valid_size.store(new_size, std::memory_order_release);
        } catch (...) {
            throw concurrent_vector_error(
                "batch_insert: Failed to insert batch due to " +
                std::string(std::current_exception() ? "exception"
                                                     : "unknown reason"));
        }
    }

    /**
     * @brief Inserts a batch of elements into the vector in parallel.
     *
     * @param values The values to be inserted.
     * @throws Any exceptions thrown during parallel insert
     */
    void parallel_batch_insert(const std::vector<T>& values) {
        if (values.empty())
            return;
        check_for_exceptions();

        std::unique_lock lock(mtx);
        try {
            size_t current_size = valid_size.load(std::memory_order_relaxed);
            size_t new_size = current_size + values.size();
            size_t values_size = values.size();

            // Expand the container if needed
            if (new_size > data.size()) {
                // Use growth factor for efficient resizing
                size_t new_capacity = std::max(
                    new_size,
                    data.size() *
                        2  // Double the size for better amortized complexity
                );
                data.resize(new_capacity);
            }

            // Release the lock before starting parallel insertion
            lock.unlock();

            // Break the work into chunks for better performance
            const size_t chunk_size =
                std::max<size_t>(1, values_size / (thread_pool.size() * 4));

            std::vector<std::future<void>> futures;
            futures.reserve((values_size + chunk_size - 1) / chunk_size);

            for (size_t chunk_start = 0; chunk_start < values_size;
                 chunk_start += chunk_size) {
                size_t chunk_end =
                    std::min(chunk_start + chunk_size, values_size);

                std::promise<void> promise;
                futures.push_back(promise.get_future());

                auto task = [this, current_size, chunk_start, chunk_end,
                             &values, promise = std::move(promise)]() mutable {
                    try {
                        for (size_t i = chunk_start; i < chunk_end; ++i) {
                            // Lock for each item
                            std::unique_lock item_lock(mtx);
                            data[current_size + i] = values[i];
                        }
                        promise.set_value();
                    } catch (...) {
                        promise.set_exception(std::current_exception());
                    }
                };

                submit_task(std::move(task));
            }

            // Wait for all tasks to complete
            for (auto& future : futures) {
                future.get();  // Will rethrow exceptions
            }

            // Update the size atomically
            valid_size.store(new_size, std::memory_order_release);

        } catch (...) {
            throw concurrent_vector_error(
                "parallel_batch_insert: Failed due to " +
                std::string(std::current_exception() ? "exception"
                                                     : "unknown reason"));
        }

        check_for_exceptions();
    }

    /**
     * @brief Clears all data from the vector.
     */
    void clear() noexcept {
        std::unique_lock lock(mtx);
        data.clear();
        valid_size.store(0, std::memory_order_release);
    }

    /**
     * @brief Shrinks the capacity to fit the current size.
     *
     * @throws Any exceptions thrown by the underlying vector
     */
    void shrink_to_fit() {
        std::unique_lock lock(mtx);
        try {
            size_t current_size = valid_size.load(std::memory_order_relaxed);

            // Create a temporary vector with the exact size
            std::vector<T> temp;
            temp.reserve(current_size);

            // Move elements to the temporary vector
            for (size_t i = 0; i < current_size; ++i) {
                temp.push_back(std::move(data[i]));
            }

            // Swap with the original vector
            data.swap(temp);
            data.shrink_to_fit();
        } catch (...) {
            throw concurrent_vector_error("shrink_to_fit: Failed due to " +
                                          std::string(std::current_exception()
                                                          ? "exception"
                                                          : "unknown reason"));
        }
    }

    /**
     * @brief Clears data in the specified range.
     *
     * @param start The start index of the range to clear.
     * @param end The end index of the range to clear (exclusive).
     * @throws concurrent_vector_error If range is invalid
     */
    void clear_range(size_t start, size_t end) {
        if (start >= end) {
            throw concurrent_vector_error(
                "clear_range: Invalid range (start >= end)");
        }

        size_t current_size = valid_size.load(std::memory_order_acquire);
        if (end > current_size) {
            throw concurrent_vector_error(
                "clear_range: End index " + std::to_string(end) +
                " exceeds vector size " + std::to_string(current_size));
        }

        std::unique_lock lock(mtx);
        try {
            // Move elements after the range
            for (size_t i = end, j = start; i < current_size; ++i, ++j) {
                data[j] = std::move(data[i]);
            }

            // Reset moved-from objects
            for (size_t i = current_size - (end - start); i < current_size;
                 ++i) {
                data[i] = T();
            }

            // Update size
            valid_size.store(current_size - (end - start),
                             std::memory_order_release);
        } catch (...) {
            throw concurrent_vector_error("clear_range: Failed due to " +
                                          std::string(std::current_exception()
                                                          ? "exception"
                                                          : "unknown reason"));
        }
    }

    /**
     * @brief Finds an element in the vector in parallel.
     *
     * @param value The value to find.
     * @return Optional containing the index if found, empty otherwise.
     * @throws Any exceptions from worker tasks
     */
    std::optional<size_t> parallel_find(const T& value) {
        check_for_exceptions();

        size_t current_size = valid_size.load(std::memory_order_acquire);
        if (current_size == 0)
            return std::nullopt;

        try {
            std::atomic<bool> found{false};
            std::atomic<size_t> found_index{0};

            // Break the work into chunks for better performance
            const size_t chunk_size =
                std::max<size_t>(1, current_size / (thread_pool.size() * 4));

            std::vector<std::future<void>> futures;
            futures.reserve((current_size + chunk_size - 1) / chunk_size);

            for (size_t chunk_start = 0; chunk_start < current_size &&
                                         !found.load(std::memory_order_acquire);
                 chunk_start += chunk_size) {
                size_t chunk_end =
                    std::min(chunk_start + chunk_size, current_size);

                std::promise<void> promise;
                futures.push_back(promise.get_future());

                auto task = [this, chunk_start, chunk_end, &value, &found,
                             &found_index,
                             promise = std::move(promise)]() mutable {
                    try {
                        std::shared_lock lock(mtx);
                        for (size_t i = chunk_start; i < chunk_end; ++i) {
                            // Check if another thread already found the value
                            if (found.load(std::memory_order_acquire))
                                break;

                            if (data[i] == value) {
                                found_index.store(i, std::memory_order_release);
                                found.store(true, std::memory_order_release);
                                break;
                            }
                        }
                        promise.set_value();
                    } catch (...) {
                        promise.set_exception(std::current_exception());
                    }
                };

                submit_task(std::move(task));
            }

            // Wait for all tasks to complete
            for (auto& future : futures) {
                future.get();
            }

            if (found.load(std::memory_order_acquire)) {
                return found_index.load(std::memory_order_acquire);
            }

            return std::nullopt;

        } catch (...) {
            throw concurrent_vector_error("parallel_find: Failed due to " +
                                          std::string(std::current_exception()
                                                          ? "exception"
                                                          : "unknown reason"));
        }

        check_for_exceptions();
        return std::nullopt;
    }

    /**
     * @brief Applies a transformation to each element in parallel.
     *
     * @param transform Function that transforms each element
     * @throws Any exceptions from worker tasks
     */
    template <typename Func>
    void parallel_transform(Func transform) {
        check_for_exceptions();

        size_t current_size = valid_size.load(std::memory_order_acquire);
        if (current_size == 0)
            return;

        try {
            // Break the work into chunks for better performance
            const size_t chunk_size =
                std::max<size_t>(1, current_size / (thread_pool.size() * 4));

            std::vector<std::future<void>> futures;
            futures.reserve((current_size + chunk_size - 1) / chunk_size);

            for (size_t chunk_start = 0; chunk_start < current_size;
                 chunk_start += chunk_size) {
                size_t chunk_end =
                    std::min(chunk_start + chunk_size, current_size);

                std::promise<void> promise;
                futures.push_back(promise.get_future());

                auto task = [this, chunk_start, chunk_end, transform,
                             promise = std::move(promise)]() mutable {
                    try {
                        for (size_t i = chunk_start; i < chunk_end; ++i) {
                            std::unique_lock lock(mtx);
                            transform(data[i]);
                        }
                        promise.set_value();
                    } catch (...) {
                        promise.set_exception(std::current_exception());
                    }
                };

                submit_task(std::move(task));
            }

            // Wait for all tasks to complete
            for (auto& future : futures) {
                future.get();
            }

        } catch (...) {
            throw concurrent_vector_error("parallel_transform: Failed due to " +
                                          std::string(std::current_exception()
                                                          ? "exception"
                                                          : "unknown reason"));
        }

        check_for_exceptions();
    }

    /**
     * @brief Submits a task to the thread pool.
     *
     * @param task The task to be executed.
     */
    void submit_task(std::function<void()> task) {
        if (!task) {
            throw std::invalid_argument("submit_task: Task cannot be null");
        }

        std::packaged_task<void()> packaged(std::move(task));
        {
            std::unique_lock lock(pool_mutex);
            task_queue.emplace(std::move(packaged));
        }
        pool_cv.notify_one();
    }

    /**
     * @brief Waits for all submitted tasks to complete.
     */
    void wait_for_tasks() {
        std::shared_lock lock(mtx);
        if (active_tasks.load(std::memory_order_acquire) > 0) {
            tasks_done_cv.wait(lock, [this] {
                return active_tasks.load(std::memory_order_acquire) == 0;
            });
        }
        check_for_exceptions();
    }

    /**
     * @brief Gets a constant reference to the current data.
     *
     * @return A constant reference to the data.
     */
    [[nodiscard]] const std::vector<T>& get_data() const {
        std::shared_lock lock(mtx);
        return data;
    }

    /**
     * @brief Gets the first element in the vector.
     *
     * @return Reference to the first element
     * @throws concurrent_vector_error If the vector is empty
     */
    T& front() {
        std::shared_lock lock(mtx);
        if (valid_size.load(std::memory_order_acquire) == 0) {
            throw concurrent_vector_error("front: Vector is empty");
        }
        return data[0];
    }

    /**
     * @brief Gets the first element in the vector (const version).
     *
     * @return Const reference to the first element
     * @throws concurrent_vector_error If the vector is empty
     */
    const T& front() const {
        std::shared_lock lock(mtx);
        if (valid_size.load(std::memory_order_acquire) == 0) {
            throw concurrent_vector_error("front: Vector is empty");
        }
        return data[0];
    }

    /**
     * @brief Gets the last element in the vector.
     *
     * @return Reference to the last element
     * @throws concurrent_vector_error If the vector is empty
     */
    T& back() {
        std::shared_lock lock(mtx);
        size_t current_size = valid_size.load(std::memory_order_acquire);
        if (current_size == 0) {
            throw concurrent_vector_error("back: Vector is empty");
        }
        return data[current_size - 1];
    }

    /**
     * @brief Gets the last element in the vector (const version).
     *
     * @return Const reference to the last element
     * @throws concurrent_vector_error If the vector is empty
     */
    const T& back() const {
        std::shared_lock lock(mtx);
        size_t current_size = valid_size.load(std::memory_order_acquire);
        if (current_size == 0) {
            throw concurrent_vector_error("back: Vector is empty");
        }
        return data[current_size - 1];
    }

    /**
     * @brief Gets the number of active worker threads.
     *
     * @return Number of threads in the thread pool
     */
    [[nodiscard]] size_t thread_count() const noexcept {
        return thread_pool.size();
    }
};

}  // namespace atom::type

#endif  // ATOM_TYPE_CONCURRENT_VECTOR_HPP
