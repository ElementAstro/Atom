/*
 * pool.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-13

Description: A very simple thread pool for preload

**************************************************/

#ifndef ATOM_ASYNC_POOL_HPP
#define ATOM_ASYNC_POOL_HPP

#include <algorithm>
#include <atomic>
#include <concepts>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <limits>
#include <mutex>
#include <optional>
#include <semaphore>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include "atom/macro.hpp"
#ifdef __has_include
#if __has_include(<version>)
#include <version>
#endif
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/stack.hpp>
#endif

namespace atom::async {

/// @brief 异常类：线程池错误
class ThreadPoolError : public std::runtime_error {
public:
    explicit ThreadPoolError(const std::string& msg)
        : std::runtime_error(msg) {}
    explicit ThreadPoolError(const char* msg) : std::runtime_error(msg) {}
};

/**
 * @brief Improved concept for defining lockable types
 * @details Based on the Lockable and BasicLockable concepts from the C++
 * standard
 * @see https://en.cppreference.com/w/cpp/named_req/Lockable
 */
template <typename Lock>
concept is_lockable = requires(Lock lock) {
    { lock.lock() } -> std::same_as<void>;
    { lock.unlock() } -> std::same_as<void>;
    { lock.try_lock() } -> std::same_as<bool>;
};

/**
 * @brief Thread-safe queue for managing data access in multi-threaded
 * environments
 * @tparam T Type of elements stored in the queue
 * @tparam Lock Lock type, defaults to std::mutex
 *
 * @details This class provides a thread-safe wrapper around std::deque with
 * comprehensive exception handling and support for various queue operations.
 * All operations are protected by mutex locks to ensure thread safety.
 */
template <typename T, typename Lock = std::mutex>
    requires is_lockable<Lock>
class ThreadSafeQueue {
public:
    /** @brief Type of elements stored in the queue */
    using value_type = T;

    /** @brief Type used for size operations */
    using size_type = typename std::deque<T>::size_type;

    /** @brief Maximum theoretical size of the queue */
    static constexpr size_type max_size = std::numeric_limits<size_type>::max();

    /**
     * @brief Default constructor
     */
    ThreadSafeQueue() = default;

    /**
     * @brief Copy constructor
     * @param other The queue to copy from
     * @throws ThreadPoolError If copying fails due to any exception
     *
     * @details Creates a deep copy of the other queue while maintaining thread
     * safety by locking the source queue during the copy operation.
     */
    ThreadSafeQueue(const ThreadSafeQueue& other) {
        try {
            std::scoped_lock lock(other.mutex_);
            data_ = other.data_;
        } catch (const std::exception& e) {
            throw ThreadPoolError(std::string("Copy constructor failed: ") +
                                  e.what());
        }
    }

    /**
     * @brief Copy assignment operator
     * @param other The queue to copy from
     * @return Reference to this queue after the copy
     * @throws ThreadPoolError If copying fails due to any exception
     *
     * @details Performs a deep copy of the other queue while maintaining thread
     * safety by locking both queues during the copy operation to prevent
     * deadlocks.
     */
    auto operator=(const ThreadSafeQueue& other) -> ThreadSafeQueue& {
        if (this != &other) {
            try {
                std::scoped_lock lockThis(mutex_, std::defer_lock);
                std::scoped_lock lockOther(other.mutex_, std::defer_lock);
                std::lock(lockThis, lockOther);
                data_ = other.data_;
            } catch (const std::exception& e) {
                throw ThreadPoolError(std::string("Copy assignment failed: ") +
                                      e.what());
            }
        }
        return *this;
    }

    /**
     * @brief Move constructor
     * @param other The queue to move from
     *
     * @details Moves the contents of the other queue while maintaining thread
     * safety by locking the source queue during the move operation. Provides
     * strong exception guarantee to ensure the object remains valid even in
     * case of exceptions.
     */
    ThreadSafeQueue(ThreadSafeQueue&& other) noexcept {
        try {
            std::scoped_lock lock(other.mutex_);
            data_ = std::move(other.data_);
        } catch (...) {
            // Maintain strong exception safety, ensuring the object is valid
            // even in case of exceptions
        }
    }

    /**
     * @brief Move assignment operator
     * @param other The queue to move from
     * @return Reference to this queue after the move
     *
     * @details Moves the contents of the other queue while maintaining thread
     * safety by locking both queues during the move operation to prevent
     * deadlocks. Provides strong exception guarantee.
     */
    auto operator=(ThreadSafeQueue&& other) noexcept -> ThreadSafeQueue& {
        if (this != &other) {
            try {
                std::scoped_lock lockThis(mutex_, std::defer_lock);
                std::scoped_lock lockOther(other.mutex_, std::defer_lock);
                std::lock(lockThis, lockOther);
                data_ = std::move(other.data_);
            } catch (...) {
                // Maintain strong exception safety
            }
        }
        return *this;
    }

    /**
     * @brief Adds an element to the back of the queue
     * @param value The element to add (rvalue reference)
     * @throws ThreadPoolError If the queue is full or if the add operation
     * fails
     *
     * @details Locks the queue, checks if there's space available, and adds the
     * element to the back of the underlying container using perfect forwarding.
     */
    void pushBack(T&& value) {
        std::scoped_lock lock(mutex_);
        if (data_.size() >= max_size) {
            throw ThreadPoolError("Queue is full");
        }
        try {
            data_.push_back(std::forward<T>(value));
        } catch (const std::exception& e) {
            throw ThreadPoolError(std::string("Push back failed: ") + e.what());
        }
    }

    /**
     * @brief Adds an element to the front of the queue
     * @param value The element to add (rvalue reference)
     * @throws ThreadPoolError If the queue is full or if the add operation
     * fails
     *
     * @details Locks the queue, checks if there's space available, and adds the
     * element to the front of the underlying container using perfect
     * forwarding.
     */
    void pushFront(T&& value) {
        std::scoped_lock lock(mutex_);
        if (data_.size() >= max_size) {
            throw ThreadPoolError("Queue is full");
        }
        try {
            data_.push_front(std::forward<T>(value));
        } catch (const std::exception& e) {
            throw ThreadPoolError(std::string("Push front failed: ") +
                                  e.what());
        }
    }

    /**
     * @brief Checks if the queue is empty
     * @return true if the queue is empty, false otherwise
     *
     * @details Thread-safe check for queue emptiness. Returns true in case of
     * any exceptions as a conservative approach.
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        try {
            std::scoped_lock lock(mutex_);
            return data_.empty();
        } catch (...) {
            return true;  // Conservative approach: return empty on exceptions
        }
    }

    /**
     * @brief Gets the number of elements in the queue
     * @return The number of elements in the queue
     *
     * @details Thread-safe method to get the current size. Returns 0 in case of
     * any exceptions as a conservative approach.
     */
    [[nodiscard]] auto size() const noexcept -> size_type {
        try {
            std::scoped_lock lock(mutex_);
            return data_.size();
        } catch (...) {
            return 0;  // Conservative approach: return 0 on exceptions
        }
    }

    /**
     * @brief Removes and returns the front element from the queue
     * @return An optional containing the front element if the queue is not
     * empty; std::nullopt otherwise
     *
     * @details Thread-safe method that removes the front element from the
     * queue. Returns std::nullopt if the queue is empty or if an exception
     * occurs.
     */
    [[nodiscard]] auto popFront() noexcept -> std::optional<T> {
        try {
            std::scoped_lock lock(mutex_);
            if (data_.empty()) {
                return std::nullopt;
            }

            auto front = std::move(data_.front());
            data_.pop_front();
            return front;
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Removes and returns the back element from the queue
     * @return An optional containing the back element if the queue is not
     * empty; std::nullopt otherwise
     *
     * @details Thread-safe method that removes the back element from the queue.
     * Returns std::nullopt if the queue is empty or if an exception occurs.
     */
    [[nodiscard]] auto popBack() noexcept -> std::optional<T> {
        try {
            std::scoped_lock lock(mutex_);
            if (data_.empty()) {
                return std::nullopt;
            }

            auto back = std::move(data_.back());
            data_.pop_back();
            return back;
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Steals an element from the back of the queue (typically used for
     * work-stealing schedulers)
     * @return An optional containing the back element if the queue is not
     * empty; std::nullopt otherwise
     *
     * @details Thread-safe method that removes and returns the back element
     * from the queue. This is semantically identical to popBack() but named
     * differently to indicate its intended use in work-stealing scenarios.
     */
    [[nodiscard]] auto steal() noexcept -> std::optional<T> {
        try {
            std::scoped_lock lock(mutex_);
            if (data_.empty()) {
                return std::nullopt;
            }

            auto back = std::move(data_.back());
            data_.pop_back();
            return back;
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Moves a specified item to the front of the queue
     * @param item The item to be moved to the front
     *
     * @details Thread-safe method that searches for the item in the queue using
     * C++20 ranges. If found, it removes the item from its current position and
     * adds it to the front of the queue. If not found, it simply adds the item
     * to the front.
     */
    void rotateToFront(const T& item) noexcept {
        try {
            std::scoped_lock lock(mutex_);
            // Use C++20 ranges to find the element
            auto iter = std::ranges::find(data_, item);

            if (iter != data_.end()) {
                std::ignore = data_.erase(iter);
            }

            data_.push_front(item);
        } catch (...) {
            // Maintain atomicity of the operation, ensuring no data corruption
        }
    }

    /**
     * @brief Copies the front element and moves it to the back of the queue
     * @return An optional containing a copy of the front element if the queue
     * is not empty; std::nullopt otherwise
     *
     * @details Thread-safe method that removes the front element, adds a copy
     * to the back of the queue, and returns a copy of the element. Returns
     * std::nullopt if the queue is empty or if an exception occurs.
     */
    [[nodiscard]] auto copyFrontAndRotateToBack() noexcept -> std::optional<T> {
        try {
            std::scoped_lock lock(mutex_);

            if (data_.empty()) {
                return std::nullopt;
            }

            auto front = data_.front();
            data_.pop_front();

            data_.push_back(front);

            return front;
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Clears all elements from the queue
     *
     * @details Thread-safe method that removes all elements from the underlying
     * container. Any exceptions that occur during the clear operation are
     * caught and ignored to maintain the noexcept guarantee.
     */
    void clear() noexcept {
        try {
            std::scoped_lock lock(mutex_);
            data_.clear();
        } catch (...) {
            // Ignore exceptions during clear attempt
        }
    }

private:
    /** @brief The underlying container storing the queue elements */
    std::deque<T> data_;

    /** @brief Mutex for thread synchronization, mutable to allow locking in
     * const methods */
    mutable Lock mutex_;
};

#ifdef ATOM_USE_BOOST_LOCKFREE
/**
 * @brief Thread-safe queue implementation using Boost.lockfree
 * @tparam T Element type in the queue
 * @tparam Capacity Fixed capacity for the lockfree queue
 */
template <typename T, size_t Capacity = 1024>
class BoostLockFreeQueue {
public:
    using value_type = T;
    using size_type = typename std::deque<T>::size_type;
    static constexpr size_type max_size = Capacity;

    BoostLockFreeQueue() = default;
    ~BoostLockFreeQueue() = default;

    // Deleted copy operations as Boost.lockfree containers are not copyable
    BoostLockFreeQueue(const BoostLockFreeQueue&) = delete;
    auto operator=(const BoostLockFreeQueue&) -> BoostLockFreeQueue& = delete;

    // Move operations
    BoostLockFreeQueue(BoostLockFreeQueue&& other) noexcept {
        // Can't move construct lockfree queue directly
        // Instead, move elements individually
        T value;
        while (other.queue_.pop(value)) {
            queue_.push(std::move(value));
        }
    }

    auto operator=(BoostLockFreeQueue&& other) noexcept -> BoostLockFreeQueue& {
        if (this != &other) {
            // Clear current queue and move elements from other
            T value;
            while (queue_.pop(value))
                ;  // Clear current queue

            while (other.queue_.pop(value)) {
                queue_.push(std::move(value));
            }
        }
        return *this;
    }

    /**
     * @brief Push an element to the back of the queue
     * @param value Element to push
     * @throws ThreadPoolError if the queue is full or push fails
     */
    void pushBack(T&& value) {
        if (!queue_.push(std::forward<T>(value))) {
            throw ThreadPoolError(
                "Boost lockfree queue is full or push failed");
        }
    }

    /**
     * @brief Push an element to the front of the queue (not efficient in
     * lockfree queue)
     * @param value Element to push
     * @throws ThreadPoolError Always throws as front operations aren't
     * efficient in lockfree queue
     */
    void pushFront(T&& value) {
        // For lockfree queue, pushing to front isn't efficient
        // We use a stack as temporary storage and re-push everything
        try {
            boost::lockfree::stack<T, boost::lockfree::capacity<Capacity>>
                temp_stack;
            T temp_value;

            // Pop all existing items and push to temp stack
            while (queue_.pop(temp_value)) {
                if (!temp_stack.push(std::move(temp_value))) {
                    throw std::runtime_error(
                        "Failed to push to temporary stack");
                }
            }

            // Push the new value first
            if (!queue_.push(std::forward<T>(value))) {
                throw std::runtime_error("Failed to push new value");
            }

            // Push back original items
            while (temp_stack.pop(temp_value)) {
                if (!queue_.push(std::move(temp_value))) {
                    throw std::runtime_error("Failed to restore queue items");
                }
            }
        } catch (const std::exception& e) {
            throw ThreadPoolError(std::string("Push front operation failed: ") +
                                  e.what());
        }
    }

    /**
     * @brief Check if the queue is empty
     * @return true if queue is empty, false otherwise
     */
    [[nodiscard]] auto empty() const noexcept -> bool { return queue_.empty(); }

    /**
     * @brief Get approximate size of the queue
     * @return Approximate number of elements in queue
     */
    [[nodiscard]] auto size() const noexcept -> size_type {
        return queue_.read_available();
    }

    /**
     * @brief Pop an element from the front of the queue
     * @return The front element if queue is not empty, std::nullopt otherwise
     */
    [[nodiscard]] auto popFront() noexcept -> std::optional<T> {
        T value;
        if (queue_.pop(value)) {
            return std::optional<T>(std::move(value));
        }
        return std::nullopt;
    }

    /**
     * @brief Pop an element from the back of the queue (not efficient in
     * lockfree queue)
     * @return The back element if queue is not empty, std::nullopt otherwise
     */
    [[nodiscard]] auto popBack() noexcept -> std::optional<T> {
        // This operation is expensive with a lockfree queue
        // as we need to pop everything and push back all but the last item
        try {
            if (queue_.empty()) {
                return std::nullopt;
            }

            std::vector<T> temp_storage;
            T value;

            // Pop all items to a vector
            while (queue_.pop(value)) {
                temp_storage.push_back(std::move(value));
            }

            if (temp_storage.empty()) {
                return std::nullopt;
            }

            // Get the back item
            auto back_item = std::move(temp_storage.back());
            temp_storage.pop_back();

            // Push back the remaining items in original order
            for (auto it = temp_storage.rbegin(); it != temp_storage.rend();
                 ++it) {
                queue_.push(std::move(*it));
            }

            return std::optional<T>(std::move(back_item));
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Steal an element from the queue (same as popBack for consistency)
     * @return An element if queue is not empty, std::nullopt otherwise
     */
    [[nodiscard]] auto steal() noexcept -> std::optional<T> {
        // In lockfree queue, stealing is the same as popFront
        return popFront();
    }

    /**
     * @brief Rotate specified item to front (not efficient)
     * @param item Item to rotate
     */
    void rotateToFront(const T& item) noexcept {
        try {
            std::vector<T> temp_storage;
            T value;
            bool found = false;

            // Extract all items
            while (queue_.pop(value)) {
                if (value == item) {
                    found = true;
                } else {
                    temp_storage.push_back(std::move(value));
                }
            }

            // Push the target item first if found
            if (found) {
                queue_.push(item);
            }

            // Push back all other items
            for (auto& stored_item : temp_storage) {
                queue_.push(std::move(stored_item));
            }

            // If item wasn't found, push it to front
            if (!found) {
                T temp_value;
                std::vector<T> rebuild;

                while (queue_.pop(temp_value)) {
                    rebuild.push_back(std::move(temp_value));
                }

                queue_.push(item);

                for (auto& stored_item : rebuild) {
                    queue_.push(std::move(stored_item));
                }
            }
        } catch (...) {
            // Maintain strong exception safety
        }
    }

    /**
     * @brief Copy front element and rotate to back
     * @return Front element if queue is not empty, std::nullopt otherwise
     */
    [[nodiscard]] auto copyFrontAndRotateToBack() noexcept -> std::optional<T> {
        try {
            if (queue_.empty()) {
                return std::nullopt;
            }

            std::vector<T> temp_storage;
            T value;

            // Pop all items to a vector
            while (queue_.pop(value)) {
                temp_storage.push_back(value);  // Copy, not move
            }

            if (temp_storage.empty()) {
                return std::nullopt;
            }

            // Get the front item
            auto front_item = temp_storage.front();

            // Push back all items including the front item at the end
            for (size_t i = 1; i < temp_storage.size(); ++i) {
                queue_.push(std::move(temp_storage[i]));
            }
            queue_.push(front_item);  // Push front item to back

            return std::optional<T>(front_item);
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Clear the queue
     */
    void clear() noexcept {
        T value;
        while (queue_.pop(value)) {
            // Just discard all elements
        }
    }

private:
    boost::lockfree::queue<T, boost::lockfree::capacity<Capacity>> queue_;
};
#endif  // ATOM_USE_BOOST_LOCKFREE

namespace details {
#ifdef __cpp_lib_move_only_function
using default_function_type = std::move_only_function<void()>;
#else
using default_function_type = std::function<void()>;
#endif
}  // namespace details

#ifdef ATOM_USE_BOOST_LOCKFREE
#ifdef ATOM_LOCKFREE_FIXED_CAPACITY
template <typename T>
using DefaultQueueType = BoostLockFreeQueue<T, ATOM_LOCKFREE_FIXED_CAPACITY>;
#else
template <typename T>
using DefaultQueueType = BoostLockFreeQueue<T>;
#endif
#else
template <typename T>
using DefaultQueueType = ThreadSafeQueue<T>;
#endif

/**
 * @brief Enhanced thread pool implementation with work stealing and priority
 * scheduling
 * @tparam FunctionType Task function type
 * @tparam ThreadType Thread type, defaults to std::jthread
 */
template <typename FunctionType = details::default_function_type,
          typename ThreadType = std::jthread,
          template <typename> typename QueueType = DefaultQueueType>
    requires std::invocable<FunctionType> &&
             std::is_same_v<void, std::invoke_result_t<FunctionType>>
class ThreadPool {
public:
    /**
     * @brief Constructor
     * @param number_of_threads Number of threads, defaults to system hardware
     * concurrency
     * @param init Thread initialization function, executed when each thread
     * starts
     * @throws ThreadPoolError If initialization fails
     */
    template <
        typename InitializationFunction = std::function<void(std::size_t)>>
        requires std::invocable<InitializationFunction, std::size_t> &&
                 std::is_same_v<void, std::invoke_result_t<
                                          InitializationFunction, std::size_t>>
    explicit ThreadPool(
        const unsigned int& number_of_threads =
            std::thread::hardware_concurrency(),
        InitializationFunction init = [](std::size_t) {})
        : tasks_(validateThreadCount(number_of_threads)) {
        try {
            std::size_t currentId = 0;
            for (std::size_t i = 0; i < tasks_.size(); ++i) {
                priority_queue_.pushBack(std::move(currentId));
                try {
                    threads_.emplace_back(
                        [this, threadId = currentId, init = std::move(init)](
                            const std::stop_token& stop_tok) {
                            threadFunction(threadId, init, stop_tok);
                        });
                    ++currentId;
                } catch (const std::exception& e) {
                    tasks_.pop_back();
                    std::ignore = priority_queue_.popBack();
                    throw ThreadPoolError(
                        std::string("Failed to create thread: ") + e.what());
                }
            }
        } catch (const std::exception& e) {
            // 清理已创建的资源
            shutdown();
            throw ThreadPoolError(
                std::string("Thread pool initialization failed: ") + e.what());
        }
    }

    /**
     * @brief 析构函数，等待所有任务完成并停止所有线程
     */
    ~ThreadPool() noexcept { shutdown(); }

    // 删除复制构造函数和复制赋值运算符
    ThreadPool(const ThreadPool&) = delete;
    auto operator=(const ThreadPool&) -> ThreadPool& = delete;

    // 定义移动构造函数和移动赋值运算符
    ThreadPool(ThreadPool&& other) noexcept = default;
    auto operator=(ThreadPool&& other) noexcept -> ThreadPool& = default;

    /**
     * @brief 向线程池提交任务并返回future以获取结果
     * @tparam Function 函数类型
     * @tparam Args 函数参数类型
     * @tparam ReturnType 函数返回类型
     * @param func 要执行的函数
     * @param args 函数参数
     * @return std::future，用于获取任务结果
     * @throws ThreadPoolError 如果任务提交失败
     */
    template <typename Function, typename... Args,
              typename ReturnType = std::invoke_result_t<Function&&, Args&&...>>
        requires std::invocable<Function, Args...>
    [[nodiscard]] auto enqueue(Function func, Args... args)
        -> std::future<ReturnType> {
        if (is_shutdown_.load(std::memory_order_acquire)) {
            throw ThreadPoolError(
                "Cannot enqueue task: Thread pool is shutting down");
        }

#ifdef __cpp_lib_move_only_function
        std::promise<ReturnType> promise;
        auto future = promise.get_future();
        auto task = [func = std::move(func), ... largs = std::move(args),
                     promise = std::move(promise)]() mutable {
            try {
                if constexpr (std::is_same_v<ReturnType, void>) {
                    std::invoke(func, largs...);
                    promise.set_value();
                } else {
                    promise.set_value(std::invoke(func, largs...));
                }
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        };
        try {
            enqueueTask(std::move(task));
        } catch (const std::exception& e) {
            throw ThreadPoolError(std::string("Failed to enqueue task: ") +
                                  e.what());
        }
        return future;
#else
        auto shared_promise = std::make_shared<std::promise<ReturnType>>();
        auto task = [func = std::move(func), ... largs = std::move(args),
                     promise = shared_promise]() {
            try {
                if constexpr (std::is_same_v<ReturnType, void>) {
                    std::invoke(func, largs...);
                    promise->set_value();
                } else {
                    promise->set_value(std::invoke(func, largs...));
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        };

        auto future = shared_promise->get_future();
        try {
            enqueueTask(std::move(task));
        } catch (const std::exception& e) {
            throw ThreadPoolError(std::string("Failed to enqueue task: ") +
                                  e.what());
        }
        return future;
#endif
    }

    /**
     * @brief 提交任务但不返回future（不关心结果）
     * @tparam Function 函数类型
     * @tparam Args 函数参数类型
     * @param func 要执行的函数
     * @param args 函数参数
     * @throws ThreadPoolError 如果任务提交失败
     */
    template <typename Function, typename... Args>
        requires std::invocable<Function, Args...>
    void enqueueDetach(Function&& func, Args&&... args) {
        if (is_shutdown_.load(std::memory_order_acquire)) {
            throw ThreadPoolError(
                "Cannot enqueue detached task: Thread pool is shutting down");
        }

        try {
            enqueueTask([func = std::forward<Function>(func),
                         ... largs = std::forward<Args>(args)]() mutable {
                try {
                    if constexpr (std::is_same_v<
                                      void, std::invoke_result_t<Function&&,
                                                                 Args&&...>>) {
                        std::invoke(func, largs...);
                    } else {
                        std::ignore = std::invoke(func, largs...);
                    }
                } catch (...) {
                    // 捕获并记录异常（在生产环境中可能会记录到日志）
                }
            });
        } catch (const std::exception& e) {
            throw ThreadPoolError(
                std::string("Failed to enqueue detached task: ") + e.what());
        }
    }

    /**
     * @brief 获取线程池中的线程数
     * @return 线程数
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return threads_.size();
    }

    /**
     * @brief 等待所有任务完成
     * @param timeout_ms 等待超时时间（毫秒），0表示一直等待
     * @return 如果所有任务完成返回true，如果超时返回false
     */
    bool waitForTasks(std::size_t timeout_ms = 0) noexcept {
        try {
            if (in_flight_tasks_.load(std::memory_order_acquire) > 0) {
                if (timeout_ms == 0) {
                    threads_complete_signal_.wait(false);
                    return true;
                } else {
                    // 使用超时版本的wait
                    auto deadline = std::chrono::steady_clock::now() +
                                    std::chrono::milliseconds(timeout_ms);
                    while (!threads_complete_signal_.load(
                               std::memory_order_acquire) &&
                           std::chrono::steady_clock::now() < deadline) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(1));
                    }
                    return threads_complete_signal_.load(
                        std::memory_order_acquire);
                }
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    /**
     * @brief 批量提交任务并等待所有任务完成
     * @tparam Iterator 迭代器类型
     * @param begin 任务范围起始迭代器
     * @param end 任务范围结束迭代器
     * @return 是否所有任务都成功提交并完成
     */
    template <typename Iterator>
    bool submitBatch(Iterator begin, Iterator end) {
        try {
            std::vector<std::future<void>> futures;
            futures.reserve(std::distance(begin, end));

            for (auto it = begin; it != end; ++it) {
                futures.push_back(enqueue(*it));
            }

            for (auto& future : futures) {
                future.wait();
            }

            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    /**
     * @brief 获取当前活跃的任务数
     * @return 活跃任务数
     */
    [[nodiscard]] auto activeTaskCount() const noexcept -> std::size_t {
        return in_flight_tasks_.load(std::memory_order_acquire);
    }

    /**
     * @brief 检查线程池是否正在关闭
     * @return 如果线程池正在关闭返回true
     */
    [[nodiscard]] bool isShuttingDown() const noexcept {
        return is_shutdown_.load(std::memory_order_acquire);
    }

private:
    /**
     * @brief Validate and return valid thread count
     * @param thread_count Requested thread count
     * @return Valid thread count
     */
    static unsigned int validateThreadCount(unsigned int thread_count) {
        const unsigned int min_threads = 1;
        const unsigned int max_threads = 256;  // 设置一个合理的上限
        const unsigned int default_threads =
            std::thread::hardware_concurrency();

        if (thread_count < min_threads) {
            return min_threads;
        } else if (thread_count > max_threads) {
            return max_threads;
        } else if (thread_count == 0) {
            return default_threads > 0 ? default_threads : min_threads;
        }

        return thread_count;
    }

    /**
     * @brief 关闭线程池，等待任务完成并停止所有线程
     */
    void shutdown() noexcept {
        is_shutdown_.store(true, std::memory_order_release);

        try {
            waitForTasks();

            for (auto& thread : threads_) {
                if (thread.joinable()) {
                    thread.request_stop();
                }
            }

            for (auto& task : tasks_) {
                task.signal.release();
            }

            for (auto& thread : threads_) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
        } catch (...) {
            // 处理关闭过程中的异常
        }
    }

    /**
     * @brief 线程工作函数
     * @param threadId 线程ID
     * @param init 初始化函数
     * @param stop_tok 停止令牌
     */
    template <typename InitFunc>
    void threadFunction(std::size_t threadId, InitFunc& init,
                        const std::stop_token& stop_tok) noexcept {
        try {
            std::invoke(init, threadId);
        } catch (...) {
            // 初始化失败但继续执行
        }

        do {
            tasks_[threadId].signal.acquire();

            do {
                processTasksFromQueue(threadId);
                stealAndProcessTasks(threadId);
            } while (unassigned_tasks_.load(std::memory_order_acquire) > 0);

            priority_queue_.rotateToFront(threadId);

            if (in_flight_tasks_.load(std::memory_order_acquire) == 0) {
                threads_complete_signal_.store(true, std::memory_order_release);
                threads_complete_signal_.notify_one();
            }

        } while (!stop_tok.stop_requested() &&
                 !is_shutdown_.load(std::memory_order_acquire));
    }

    /**
     * @brief 处理线程自己队列中的任务
     * @param threadId 线程ID
     */
    void processTasksFromQueue(std::size_t threadId) noexcept {
        while (auto task = tasks_[threadId].tasks.popFront()) {
            unassigned_tasks_.fetch_sub(1, std::memory_order_release);
            try {
                std::invoke(std::move(task.value()));
            } catch (...) {
                // 捕获任务执行异常
            }
            in_flight_tasks_.fetch_sub(1, std::memory_order_release);
        }
    }

    /**
     * @brief 从其他线程队列窃取并执行任务
     * @param threadId 当前线程ID
     */
    void stealAndProcessTasks(std::size_t threadId) noexcept {
        for (std::size_t j = 1; j < tasks_.size(); ++j) {
            const std::size_t INDEX = (threadId + j) % tasks_.size();
            if (auto task = tasks_[INDEX].tasks.steal()) {
                unassigned_tasks_.fetch_sub(1, std::memory_order_release);
                try {
                    std::invoke(std::move(task.value()));
                } catch (...) {
                    // 捕获任务执行异常
                }
                in_flight_tasks_.fetch_sub(1, std::memory_order_release);
                break;
            }
        }
    }

    /**
     * @brief 将任务放入队列中
     * @tparam Function 任务函数类型
     * @param func 任务函数
     * @throws ThreadPoolError 如果无法提交任务
     */
    template <typename Function>
    void enqueueTask(Function&& func) {
        if (is_shutdown_.load(std::memory_order_acquire)) {
            throw ThreadPoolError("Thread pool is shutting down");
        }

        auto iOpt = priority_queue_.copyFrontAndRotateToBack();
        if (!iOpt.has_value()) {
            throw ThreadPoolError(
                "Failed to get thread index from priority queue");
        }
        auto index = *(iOpt);

        unassigned_tasks_.fetch_add(1, std::memory_order_release);
        const auto PREV_IN_FLIGHT =
            in_flight_tasks_.fetch_add(1, std::memory_order_release);

        if (PREV_IN_FLIGHT == 0) {
            threads_complete_signal_.store(false, std::memory_order_release);
        }

        try {
            tasks_[index].tasks.pushBack(std::forward<Function>(func));
            tasks_[index].signal.release();
        } catch (...) {
            unassigned_tasks_.fetch_sub(1, std::memory_order_release);
            in_flight_tasks_.fetch_sub(1, std::memory_order_release);
            throw ThreadPoolError("Failed to enqueue task");
        }
    }

#ifdef ATOM_USE_BOOST_LOCKFREE
    // Use lockfree queue when the macro is defined
    struct TaskItem {
#ifdef ATOM_LOCKFREE_FIXED_CAPACITY
        // Use a fixed capacity if specified
        atom::async::BoostLockFreeQueue<FunctionType,
                                        ATOM_LOCKFREE_FIXED_CAPACITY>
            tasks{};
#else
        // Default capacity
        atom::async::BoostLockFreeQueue<FunctionType> tasks{};
#endif
        std::binary_semaphore signal{0};
    } ATOM_ALIGNAS(128);
#else
    // Use the original implementation
    struct TaskItem {
        atom::async::ThreadSafeQueue<FunctionType> tasks{};
        std::binary_semaphore signal{0};
    } ATOM_ALIGNAS(128);
#endif

    std::vector<ThreadType> threads_;
    std::deque<TaskItem> tasks_;
    atom::async::ThreadSafeQueue<std::size_t> priority_queue_;
    std::atomic_int_fast64_t unassigned_tasks_{0}, in_flight_tasks_{0};
    std::atomic_bool threads_complete_signal_{false};
    std::atomic_bool is_shutdown_{false};
};
}  // namespace atom::async

#endif  // ATOM_ASYNC_POOL_HPP
