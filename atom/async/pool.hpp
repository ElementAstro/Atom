#ifndef ATOM_ASYNC_THREADPOOL_HPP
#define ATOM_ASYNC_THREADPOOL_HPP

#include <atomic>
#include <concepts>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

// Platform-specific optimizations
#if defined(_WIN32) || defined(_WIN64)
#define ATOM_PLATFORM_WINDOWS
// clang-format off
#include <windows.h>
#include <processthreadsapi.h>
// clang-format on
#elif defined(__APPLE__)
#define ATOM_PLATFORM_MACOS
#include <dispatch/dispatch.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#elif defined(__linux__)
#define ATOM_PLATFORM_LINUX
#include <pthread.h>
#include <sched.h>
#include <sys/sysinfo.h>
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/stack.hpp>
#endif

#include "atom/async/future.hpp"
#include "atom/async/promise.hpp"

namespace atom::async {

/**
 * @brief Exception class for thread pool errors
 */
class ThreadPoolError : public std::runtime_error {
public:
    explicit ThreadPoolError(const std::string& msg)
        : std::runtime_error(msg) {}
    explicit ThreadPoolError(const char* msg) : std::runtime_error(msg) {}
};

/**
 * @brief Concept for defining lockable types
 * @details Based on Lockable and BasicLockable concepts from C++ standard
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
     */
    ThreadSafeQueue(ThreadSafeQueue&& other) noexcept {
        try {
            std::scoped_lock lock(other.mutex_);
            data_ = std::move(other.data_);
        } catch (...) {
            // Maintain strong exception safety
        }
    }

    /**
     * @brief Move assignment operator
     * @param other The queue to move from
     * @return Reference to this queue after the move
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
            // Maintain atomicity of the operation
        }
    }

    /**
     * @brief Copies the front element and moves it to the back of the queue
     * @return An optional containing a copy of the front element if the queue
     * is not empty; std::nullopt otherwise
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
     * @brief Push an element to the front of the queue
     * @param value Element to push
     * @throws ThreadPoolError if operation fails
     */
    void pushFront(T&& value) {
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
     * @brief Pop an element from the back of the queue
     * @return The back element if queue is not empty, std::nullopt otherwise
     */
    [[nodiscard]] auto popBack() noexcept -> std::optional<T> {
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
        return popFront();  // For lockfree queue, stealing is the same as
                            // popFront
    }

    /**
     * @brief Rotate specified item to front
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
 * @class ThreadPool
 * @brief High-performance thread pool implementation with modern C++20 features
 * and platform-specific optimizations
 */
class ThreadPool {
public:
    /**
     * @brief Thread pool configuration options
     */
    struct Options {
        enum class ThreadPriority {
            Lowest,
            BelowNormal,
            Normal,
            AboveNormal,
            Highest,
            TimeCritical
        };

        enum class CpuAffinityMode {
            None,        // No CPU affinity settings
            Sequential,  // Threads assigned to cores sequentially
            Spread,      // Threads spread across different cores
            CorePinned,  // Threads pinned to specified cores
            Automatic    // Automatically adjust (requires hardware support)
        };

        size_t initialThreadCount = 0;  // 0 means use hardware thread count
        size_t maxThreadCount = 0;      // 0 means unlimited
        size_t maxQueueSize = 0;        // 0 means unlimited
        std::chrono::milliseconds threadIdleTimeout{
            5000};                      // Idle thread timeout
        bool allowThreadGrowth = true;  // Allow dynamic thread creation
        bool allowThreadShrink = true;  // Allow dynamic thread reduction
        ThreadPriority threadPriority = ThreadPriority::Normal;
        CpuAffinityMode cpuAffinityMode = CpuAffinityMode::None;
        std::vector<int> pinnedCores;  // Used for CorePinned mode
        bool useWorkStealing =
            true;  // Enable work stealing for better performance
        bool setStackSize = false;  // Whether to set custom stack size
        size_t stackSize = 0;       // Custom thread stack size, 0 means default

        static Options createDefault() { return {}; }

        static Options createHighPerformance() {
            Options opts;
            opts.initialThreadCount = std::thread::hardware_concurrency();
            opts.maxThreadCount = opts.initialThreadCount * 2;
            opts.threadPriority = ThreadPriority::AboveNormal;
            opts.cpuAffinityMode = CpuAffinityMode::Spread;
            opts.useWorkStealing = true;
            return opts;
        }

        static Options createLowLatency() {
            Options opts;
            opts.initialThreadCount = std::thread::hardware_concurrency();
            opts.maxThreadCount = opts.initialThreadCount;
            opts.threadPriority = ThreadPriority::TimeCritical;
            opts.cpuAffinityMode = CpuAffinityMode::CorePinned;
            // In a real application, you might need to choose appropriate cores
            // Here we simply use the first half of available cores
            for (unsigned i = 0; i < opts.initialThreadCount / 2; ++i) {
                opts.pinnedCores.push_back(i);
            }
            return opts;
        }

        static Options createEnergyEfficient() {
            Options opts;
            opts.initialThreadCount = std::thread::hardware_concurrency() / 2;
            opts.maxThreadCount = std::thread::hardware_concurrency();
            opts.threadIdleTimeout = std::chrono::milliseconds(1000);
            opts.allowThreadShrink = true;
            opts.threadPriority = ThreadPriority::BelowNormal;
            return opts;
        }
    };

    /**
     * @brief Constructor
     * @param options Thread pool options
     */
    explicit ThreadPool(Options options = Options::createDefault())
        : options_(std::move(options)), stop_(false), activeThreads_(0) {
        // Initialize threads
        size_t numThreads = options_.initialThreadCount;
        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
        }

        // Ensure at least one thread
        numThreads = std::max(size_t(1), numThreads);

        // Create worker threads
        for (size_t i = 0; i < numThreads; ++i) {
            createWorkerThread(i);
        }
    }

    /**
     * @brief Delete copy constructor and assignment
     */
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief Destructor, stops all threads
     */
    ~ThreadPool() { shutdown(); }

    /**
     * @brief Submit a task to the thread pool
     * @tparam F Function type
     * @tparam Args Argument types
     * @param f Function to execute
     * @param args Function arguments
     * @return EnhancedFuture containing the task result
     */
    template <typename F, typename... Args>
        requires std::invocable<F, Args...>
    auto submit(F&& f, Args&&... args) {
        using ResultType = std::invoke_result_t<F, Args...>;
        using TaskType = std::packaged_task<ResultType()>;

        // Create task encapsulating function and arguments
        auto task = std::make_shared<TaskType>(
            [func = std::forward<F>(f),
             ... largs = std::forward<Args>(args)]() mutable {
                return std::invoke(std::forward<F>(func),
                                   std::forward<Args>(largs)...);
            });

        // Get task's future
        auto future = task->get_future();

        // Queue the task
        {
            std::unique_lock lock(queueMutex_);

            // Check if we need to increase thread count
            if (options_.allowThreadGrowth && tasks_.size() >= activeThreads_ &&
                workers_.size() < options_.maxThreadCount) {
                createWorkerThread(workers_.size());
            }

            // Check if queue is full
            if (options_.maxQueueSize > 0 &&
                tasks_.size() >= options_.maxQueueSize) {
                throw std::runtime_error("Thread pool task queue is full");
            }

            // Add task
            tasks_.emplace_back([task]() { (*task)(); });
        }

        // Notify a waiting thread
        condition_.notify_one();

        // Return enhanced future
        return EnhancedFuture<ResultType>(future.share());
    }

    /**
     * @brief Submit multiple tasks and wait for all to complete
     * @tparam InputIt Input iterator type
     * @tparam F Function type
     * @param first Start of input range
     * @param last End of input range
     * @param f Function to execute for each element
     * @return Vector of task results
     */
    template <std::input_iterator InputIt, typename F>
        requires std::invocable<
            F, typename std::iterator_traits<InputIt>::value_type>
    auto submitBatch(InputIt first, InputIt last, F&& f) {
        using InputType = typename std::iterator_traits<InputIt>::value_type;
        using ResultType = std::invoke_result_t<F, InputType>;

        std::vector<EnhancedFuture<ResultType>> futures;
        futures.reserve(std::distance(first, last));

        for (auto it = first; it != last; ++it) {
            futures.push_back(submit(f, *it));
        }

        return futures;
    }

    /**
     * @brief Submit a task with a Promise
     * @tparam F Function type
     * @tparam Args Argument types
     * @param f Function to execute
     * @param args Function arguments
     * @return Promise object
     */
    template <typename F, typename... Args>
        requires std::invocable<F, Args...>
    auto submitWithPromise(F&& f, Args&&... args) {
        using ResultType = std::invoke_result_t<F, Args...>;

        // Create Promise
        Promise<ResultType> promise;

        // Create task
        auto task = [promise, func = std::forward<F>(f),
                     ... largs = std::forward<Args>(args)]() mutable {
            try {
                if constexpr (std::is_void_v<ResultType>) {
                    std::invoke(std::forward<F>(func),
                                std::forward<Args>(largs)...);
                    promise.setValue();
                } else {
                    promise.setValue(std::invoke(std::forward<F>(func),
                                                 std::forward<Args>(largs)...));
                }
            } catch (...) {
                promise.setException(std::current_exception());
            }
        };

        // Queue the task
        {
            std::unique_lock lock(queueMutex_);

            // Check if we need to increase thread count
            if (options_.allowThreadGrowth && tasks_.size() >= activeThreads_ &&
                workers_.size() < options_.maxThreadCount) {
                createWorkerThread(workers_.size());
            }

            // Check if queue is full
            if (options_.maxQueueSize > 0 &&
                tasks_.size() >= options_.maxQueueSize) {
                throw std::runtime_error("Thread pool task queue is full");
            }

            // Add task
            tasks_.emplace_back(std::move(task));
        }

        // Notify a waiting thread
        condition_.notify_one();

        return promise;
    }

    /**
     * @brief Submit a task with ASIO-style execution
     * @tparam F Function type
     * @param f Function to execute
     */
    template <typename F>
        requires std::invocable<F>
    void execute(F&& f) {
        {
            std::unique_lock lock(queueMutex_);
            tasks_.emplace_back(std::forward<F>(f));
        }
        condition_.notify_one();
    }

    /**
     * @brief Submit a task without waiting for result
     * @tparam Function Function type
     * @tparam Args Argument types
     * @param func Function to execute
     * @param args Function arguments
     * @throws ThreadPoolError If task submission fails
     */
    template <typename Function, typename... Args>
        requires std::invocable<Function, Args...>
    void enqueueDetach(Function&& func, Args&&... args) {
        if (stop_.load(std::memory_order_acquire)) {
            throw ThreadPoolError(
                "Cannot enqueue detached task: Thread pool is shutting down");
        }

        try {
            {
                std::unique_lock lock(queueMutex_);

                // Check if queue is full
                if (options_.maxQueueSize > 0 &&
                    tasks_.size() >= options_.maxQueueSize) {
                    throw ThreadPoolError("Thread pool task queue is full");
                }

                // Add task
                tasks_.emplace_back([func = std::forward<Function>(func),
                                     ... largs =
                                         std::forward<Args>(args)]() mutable {
                    try {
                        if constexpr (std::is_same_v<
                                          void, std::invoke_result_t<
                                                    Function&&, Args&&...>>) {
                            std::invoke(func, largs...);
                        } else {
                            std::ignore = std::invoke(func, largs...);
                        }
                    } catch (...) {
                        // Catch and log exception (in production, might log to
                        // a logging system)
                    }
                });
            }
            condition_.notify_one();
        } catch (const std::exception& e) {
            throw ThreadPoolError(
                std::string("Failed to enqueue detached task: ") + e.what());
        }
    }

    /**
     * @brief Get current queue size
     * @return Task queue size
     */
    [[nodiscard]] size_t getQueueSize() const {
        std::unique_lock lock(queueMutex_);
        return tasks_.size();
    }

    /**
     * @brief Get worker thread count
     * @return Thread count
     */
    [[nodiscard]] size_t getThreadCount() const {
        std::unique_lock lock(queueMutex_);
        return workers_.size();
    }

    /**
     * @brief Get active thread count
     * @return Active thread count
     */
    [[nodiscard]] size_t getActiveThreadCount() const { return activeThreads_; }

    /**
     * @brief Resize the thread pool
     * @param newSize New thread count
     */
    void resize(size_t newSize) {
        if (newSize == 0) {
            throw std::invalid_argument("Thread pool size cannot be zero");
        }

        std::unique_lock lock(queueMutex_);

        size_t currentSize = workers_.size();

        if (newSize > currentSize) {
            // Increase threads
            if (!options_.allowThreadGrowth) {
                throw std::runtime_error(
                    "Thread growth is disabled in this pool");
            }

            if (options_.maxThreadCount > 0 &&
                newSize > options_.maxThreadCount) {
                newSize = options_.maxThreadCount;
            }

            for (size_t i = currentSize; i < newSize; ++i) {
                createWorkerThread(i);
            }
        } else if (newSize < currentSize) {
            // Decrease threads
            if (!options_.allowThreadShrink) {
                throw std::runtime_error(
                    "Thread shrinking is disabled in this pool");
            }

            // Mark excess threads for termination
            for (size_t i = newSize; i < currentSize; ++i) {
                terminationFlags_[i] = true;
            }

            // Unlock mutex to avoid deadlock
            lock.unlock();

            // Wake up all threads to check termination flags
            condition_.notify_all();
        }
    }

    /**
     * @brief Shutdown the thread pool, wait for all tasks to complete
     */
    void shutdown() {
        {
            std::unique_lock lock(queueMutex_);
            stop_ = true;
        }

        // Notify all threads
        condition_.notify_all();

        // Wait for all threads to finish
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    /**
     * @brief Immediately stop the thread pool, discard unfinished tasks
     */
    void shutdownNow() {
        {
            std::unique_lock lock(queueMutex_);
            stop_ = true;
            tasks_.clear();
        }

        // Notify all threads
        condition_.notify_all();

        // Wait for all threads to finish
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    /**
     * @brief Wait for all current tasks to complete
     */
    void waitForTasks() {
        std::unique_lock lock(queueMutex_);
        waitEmpty_.wait(
            lock, [this] { return tasks_.empty() && activeThreads_ == 0; });
    }

    /**
     * @brief Wait for an available thread
     */
    void waitForAvailableThread() {
        std::unique_lock lock(queueMutex_);
        waitAvailable_.wait(
            lock, [this] { return activeThreads_ < workers_.size() || stop_; });
    }

    /**
     * @brief Get thread pool options
     * @return Const reference to options
     */
    [[nodiscard]] const Options& getOptions() const { return options_; }

    [[nodiscard]] bool isShutdown() const {
        return stop_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool isThreadGrowthAllowed() const {
        return options_.allowThreadGrowth;
    }

    [[nodiscard]] bool isThreadShrinkAllowed() const {
        return options_.allowThreadShrink;
    }

    [[nodiscard]] bool isWorkStealingEnabled() const {
        return options_.useWorkStealing;
    }

private:
    /**
     * @brief Create a worker thread
     * @param id Thread ID
     */
    void createWorkerThread(size_t id) {
        // Don't create if we've reached max thread count
        if (options_.maxThreadCount > 0 &&
            workers_.size() >= options_.maxThreadCount) {
            return;
        }

        // Initialize termination flag
        if (id >= terminationFlags_.size()) {
            terminationFlags_.resize(id + 1, false);
        }

        // Create worker thread
        workers_.emplace_back([this, id]() {
        // Set thread name (if platform supports it)
#if defined(ATOM_PLATFORM_LINUX) || defined(ATOM_PLATFORM_MACOS)
            {
                char threadName[16];
                snprintf(threadName, sizeof(threadName), "Worker-%zu", id);
                pthread_setname_np(pthread_self(), threadName);
            }
#elif defined(ATOM_PLATFORM_WINDOWS) && \
    _WIN32_WINNT >= 0x0602  // Windows 8 and higher
            {
                wchar_t threadName[16];
                swprintf(threadName, sizeof(threadName) / sizeof(wchar_t),
                         L"Worker-%zu", id);
                SetThreadDescription(GetCurrentThread(), threadName);
            }
#endif

            // Set thread priority
            setPriority(options_.threadPriority);

            // Set CPU affinity
            setCpuAffinity(id);

            // Thread main loop
            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock lock(queueMutex_);

                    // Wait for task or stop signal
                    auto waitResult = condition_.wait_for(
                        lock, options_.threadIdleTimeout, [this, id] {
                            return stop_ || !tasks_.empty() ||
                                   terminationFlags_[id];
                        });

                    // If timeout and thread shrinking allowed, check if we
                    // should terminate
                    if (!waitResult && options_.allowThreadShrink &&
                        workers_.size() > options_.initialThreadCount) {
                        // If idle time exceeds threshold and current thread
                        // count exceeds initial count
                        terminationFlags_[id] = true;
                    }

                    // Check if thread should terminate
                    if ((stop_ || terminationFlags_[id]) && tasks_.empty()) {
                        // Clear termination flag
                        if (id < terminationFlags_.size()) {
                            terminationFlags_[id] = false;
                        }
                        return;
                    }

                    // If no tasks, continue waiting
                    if (tasks_.empty()) {
                        continue;
                    }

                    // Get task
                    task = std::move(tasks_.front());
                    tasks_.pop_front();

                    // Notify potential waiting submitters
                    waitAvailable_.notify_one();
                }

                // Execute task
                activeThreads_++;

                try {
                    task();
                } catch (...) {
                    // Ignore exceptions in task execution
                }

                // Decrease active thread count
                activeThreads_--;

                // If no active threads and task queue is empty, notify waiters
                {
                    std::unique_lock lock(queueMutex_);
                    if (activeThreads_ == 0 && tasks_.empty()) {
                        waitEmpty_.notify_all();
                    }
                }

                // Work stealing implementation - if local queue is empty, try
                // to steal tasks from other threads
                if (options_.useWorkStealing) {
                    tryStealTasks();
                }
            }
        });

        // Set custom stack size if needed
#ifdef ATOM_PLATFORM_WINDOWS
        if (options_.setStackSize && options_.stackSize > 0) {
            // In Windows, can't directly change stack size of already created
            // thread This would only log a message in a real implementation
        }
#endif
    }

    /**
     * @brief Try to steal tasks from other threads
     */
    void tryStealTasks() {
        // Simple implementation: each thread checks global queue when idle
        std::unique_lock lock(queueMutex_, std::try_to_lock);
        if (lock.owns_lock() && !tasks_.empty()) {
            std::function<void()> task = std::move(tasks_.front());
            tasks_.pop_front();

            // Release lock before executing task
            lock.unlock();

            activeThreads_++;
            try {
                task();
            } catch (...) {
                // Ignore exceptions in task execution
            }
            activeThreads_--;
        }
    }

    /**
     * @brief Set thread priority
     * @param priority Priority level
     */
    void setPriority(Options::ThreadPriority priority) {
#if defined(ATOM_PLATFORM_WINDOWS)
        int winPriority;
        switch (priority) {
            case Options::ThreadPriority::Lowest:
                winPriority = THREAD_PRIORITY_LOWEST;
                break;
            case Options::ThreadPriority::BelowNormal:
                winPriority = THREAD_PRIORITY_BELOW_NORMAL;
                break;
            case Options::ThreadPriority::Normal:
                winPriority = THREAD_PRIORITY_NORMAL;
                break;
            case Options::ThreadPriority::AboveNormal:
                winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
                break;
            case Options::ThreadPriority::Highest:
                winPriority = THREAD_PRIORITY_HIGHEST;
                break;
            case Options::ThreadPriority::TimeCritical:
                winPriority = THREAD_PRIORITY_TIME_CRITICAL;
                break;
            default:
                winPriority = THREAD_PRIORITY_NORMAL;
        }
        SetThreadPriority(GetCurrentThread(), winPriority);
#elif defined(ATOM_PLATFORM_LINUX) || defined(ATOM_PLATFORM_MACOS)
        int policy;
        struct sched_param param;
        pthread_getschedparam(pthread_self(), &policy, &param);

        switch (priority) {
            case Options::ThreadPriority::Lowest:
                param.sched_priority = sched_get_priority_min(policy);
                break;
            case Options::ThreadPriority::BelowNormal:
                param.sched_priority = sched_get_priority_min(policy) +
                                       (sched_get_priority_max(policy) -
                                        sched_get_priority_min(policy)) /
                                           4;
                break;
            case Options::ThreadPriority::Normal:
                param.sched_priority = sched_get_priority_min(policy) +
                                       (sched_get_priority_max(policy) -
                                        sched_get_priority_min(policy)) /
                                           2;
                break;
            case Options::ThreadPriority::AboveNormal:
                param.sched_priority = sched_get_priority_max(policy) -
                                       (sched_get_priority_max(policy) -
                                        sched_get_priority_min(policy)) /
                                           4;
                break;
            case Options::ThreadPriority::Highest:
            case Options::ThreadPriority::TimeCritical:
                param.sched_priority = sched_get_priority_max(policy);
                break;
            default:
                param.sched_priority = sched_get_priority_min(policy) +
                                       (sched_get_priority_max(policy) -
                                        sched_get_priority_min(policy)) /
                                           2;
        }

        pthread_setschedparam(pthread_self(), policy, &param);
#endif
    }

    /**
     * @brief Set CPU affinity
     * @param threadId Thread ID
     */
    void setCpuAffinity(size_t threadId) {
        if (options_.cpuAffinityMode == Options::CpuAffinityMode::None) {
            return;
        }

        const unsigned int numCores = std::thread::hardware_concurrency();
        if (numCores <= 1) {
            return;  // No need for affinity on single-core systems
        }

        unsigned int coreId = 0;

        switch (options_.cpuAffinityMode) {
            case Options::CpuAffinityMode::Sequential:
                coreId = threadId % numCores;
                break;

            case Options::CpuAffinityMode::Spread:
                // Try to spread threads across different physical cores
                coreId = (threadId * 2) % numCores;
                break;

            case Options::CpuAffinityMode::CorePinned:
                if (!options_.pinnedCores.empty()) {
                    coreId = options_.pinnedCores[threadId %
                                                  options_.pinnedCores.size()];
                } else {
                    coreId = threadId % numCores;
                }
                break;

            case Options::CpuAffinityMode::Automatic:
                // Automatic mode relies on OS scheduling
                return;

            default:
                return;
        }

            // Set CPU affinity
#if defined(ATOM_PLATFORM_WINDOWS)
        DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << coreId);
        SetThreadAffinityMask(GetCurrentThread(), mask);
#elif defined(ATOM_PLATFORM_LINUX)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(coreId, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#elif defined(ATOM_PLATFORM_MACOS)
        // macOS only supports soft affinity through thread policy
        thread_affinity_policy_data_t policy = {static_cast<integer_t>(coreId)};
        thread_policy_set(pthread_mach_thread_np(pthread_self()),
                          THREAD_AFFINITY_POLICY, (thread_policy_t)&policy,
                          THREAD_AFFINITY_POLICY_COUNT);
#endif
    }

private:
    Options options_;                          // Thread pool configuration
    std::atomic<bool> stop_;                   // Stop flag
    std::vector<std::thread> workers_;         // Worker threads
    std::deque<std::function<void()>> tasks_;  // Task queue
    std::vector<bool> terminationFlags_;       // Thread termination flags

    mutable std::mutex queueMutex_;  // Mutex protecting task queue
    std::condition_variable
        condition_;  // Condition variable for thread waiting
    std::condition_variable
        waitEmpty_;  // Condition variable for waiting for empty queue
    std::condition_variable
        waitAvailable_;  // Condition variable for waiting for available thread

    std::atomic<size_t> activeThreads_;  // Current active thread count
};

// Global thread pool singleton
inline ThreadPool& globalThreadPool() {
    static ThreadPool instance(ThreadPool::Options::createDefault());
    return instance;
}

// High performance thread pool singleton
inline ThreadPool& highPerformanceThreadPool() {
    static ThreadPool instance(ThreadPool::Options::createHighPerformance());
    return instance;
}

// Low latency thread pool singleton
inline ThreadPool& lowLatencyThreadPool() {
    static ThreadPool instance(ThreadPool::Options::createLowLatency());
    return instance;
}

// Energy efficient thread pool singleton
inline ThreadPool& energyEfficientThreadPool() {
    static ThreadPool instance(ThreadPool::Options::createEnergyEfficient());
    return instance;
}

/**
 * @brief Asynchronously execute a task in the global thread pool
 * @tparam F Function type
 * @tparam Args Argument types
 * @param f Function to execute
 * @param args Function arguments
 * @return EnhancedFuture containing the task result
 */
template <typename F, typename... Args>
    requires std::invocable<F, Args...>
auto async(F&& f, Args&&... args) {
    return globalThreadPool().submit(std::forward<F>(f),
                                     std::forward<Args>(args)...);
}

/**
 * @brief Asynchronously execute a task in the high performance thread pool
 * @tparam F Function type
 * @tparam Args Argument types
 * @param f Function to execute
 * @param args Function arguments
 * @return EnhancedFuture containing the task result
 */
template <typename F, typename... Args>
    requires std::invocable<F, Args...>
auto asyncHighPerformance(F&& f, Args&&... args) {
    return highPerformanceThreadPool().submit(std::forward<F>(f),
                                              std::forward<Args>(args)...);
}

/**
 * @brief Asynchronously execute a task in the low latency thread pool
 * @tparam F Function type
 * @tparam Args Argument types
 * @param f Function to execute
 * @param args Function arguments
 * @return EnhancedFuture containing the task result
 */
template <typename F, typename... Args>
    requires std::invocable<F, Args...>
auto asyncLowLatency(F&& f, Args&&... args) {
    return lowLatencyThreadPool().submit(std::forward<F>(f),
                                         std::forward<Args>(args)...);
}

/**
 * @brief Asynchronously execute a task in the energy efficient thread pool
 * @tparam F Function type
 * @tparam Args Argument types
 * @param f Function to execute
 * @param args Function arguments
 * @return EnhancedFuture containing the task result
 */
template <typename F, typename... Args>
    requires std::invocable<F, Args...>
auto asyncEnergyEfficient(F&& f, Args&&... args) {
    return energyEfficientThreadPool().submit(std::forward<F>(f),
                                              std::forward<Args>(args)...);
}

}  // namespace atom::async

#endif  // ATOM_ASYNC_THREADPOOL_HPP