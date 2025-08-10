#ifndef ATOM_ASYNC_THREADPOOL_HPP
#define ATOM_ASYNC_THREADPOOL_HPP

#include <spdlog/spdlog.h>  // Added for logging
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

#ifdef ATOM_USE_ASIO
#include <asio.hpp>
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
            spdlog::error("ThreadSafeQueue copy constructor failed: {}",
                          e.what());
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
                spdlog::error("ThreadSafeQueue copy assignment failed: {}",
                              e.what());
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
            spdlog::error("ThreadSafeQueue move constructor failed.");
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
                spdlog::error("ThreadSafeQueue move assignment failed.");
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
            spdlog::error("ThreadSafeQueue is full, cannot pushBack.");
            throw ThreadPoolError("Queue is full");
        }
        try {
            data_.push_back(std::forward<T>(value));
        } catch (const std::exception& e) {
            spdlog::error("ThreadSafeQueue pushBack failed: {}", e.what());
            throw ThreadPoolError(std::string("Push back failed: ") + e.what());
        }
    }

    /**
     * @brief Adds an element to the back of the queue
     * @param value The element to add (const reference)
     * @throws ThreadPoolError If the queue is full or if the add operation
     * fails
     */
    void pushBack(const T& value) {
        std::scoped_lock lock(mutex_);
        if (data_.size() >= max_size) {
            spdlog::error("ThreadSafeQueue is full, cannot pushBack.");
            throw ThreadPoolError("Queue is full");
        }
        try {
            data_.push_back(value);
        } catch (const std::exception& e) {
            spdlog::error("Failed to pushBack to ThreadSafeQueue: {}", e.what());
            throw ThreadPoolError("Failed to pushBack to ThreadSafeQueue");
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
            spdlog::error("ThreadSafeQueue is full, cannot pushFront.");
            throw ThreadPoolError("Queue is full");
        }
        try {
            data_.push_front(std::forward<T>(value));
        } catch (const std::exception& e) {
            spdlog::error("ThreadSafeQueue pushFront failed: {}", e.what());
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
            spdlog::error(
                "Exception in ThreadSafeQueue::empty, returning true.");
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
            spdlog::error("Exception in ThreadSafeQueue::size, returning 0.");
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
            spdlog::error(
                "Exception in ThreadSafeQueue::popFront, returning "
                "std::nullopt.");
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
            spdlog::error(
                "Exception in ThreadSafeQueue::popBack, returning "
                "std::nullopt.");
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
            spdlog::error(
                "Exception in ThreadSafeQueue::steal, returning std::nullopt.");
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
            spdlog::error("Exception in ThreadSafeQueue::rotateToFront.");
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
            spdlog::error(
                "Exception in ThreadSafeQueue::copyFrontAndRotateToBack, "
                "returning std::nullopt.");
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
            spdlog::error("Exception in ThreadSafeQueue::clear.");
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
    using size_type =
        typename std::deque<T>::size_type;  // Using deque's size_type for
                                            // consistency
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
            if (!queue_.push(std::move(value))) {
                spdlog::warn(
                    "BoostLockFreeQueue move constructor: Failed to push "
                    "element.");
            }
        }
    }

    auto operator=(BoostLockFreeQueue&& other) noexcept -> BoostLockFreeQueue& {
        if (this != &other) {
            // Clear current queue and move elements from other
            T value;
            while (queue_.pop(value))
                ;  // Clear current queue

            while (other.queue_.pop(value)) {
                if (!queue_.push(std::move(value))) {
                    spdlog::warn(
                        "BoostLockFreeQueue move assignment: Failed to push "
                        "element.");
                }
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
            spdlog::error("Boost lockfree queue is full or push failed.");
            throw ThreadPoolError(
                "Boost lockfree queue is full or push failed");
        }
    }

    /**
     * @brief Push an element to the back of the queue
     * @param value Element to push (const reference)
     * @throws ThreadPoolError if the queue is full or push fails
     */
    void pushBack(const T& value) {
        if (!queue_.push(value)) {
            spdlog::error("Boost lockfree queue is full or push failed.");
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
                    spdlog::error(
                        "Failed to push to temporary stack in "
                        "BoostLockFreeQueue::pushFront.");
                    throw std::runtime_error(
                        "Failed to push to temporary stack");
                }
            }

            // Push the new value first
            if (!queue_.push(std::forward<T>(value))) {
                spdlog::error(
                    "Failed to push new value to queue in "
                    "BoostLockFreeQueue::pushFront.");
                throw std::runtime_error("Failed to push new value");
            }

            // Push back original items
            while (temp_stack.pop(temp_value)) {
                if (!queue_.push(std::move(temp_value))) {
                    spdlog::error(
                        "Failed to restore queue items in "
                        "BoostLockFreeQueue::pushFront.");
                    throw std::runtime_error("Failed to restore queue items");
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("BoostLockFreeQueue pushFront operation failed: {}",
                          e.what());
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
                if (!queue_.push(std::move(*it))) {
                    spdlog::error(
                        "Failed to push back remaining items in "
                        "BoostLockFreeQueue::popBack.");
                    // This indicates a serious issue, as we just popped them.
                    // Re-throwing might be an option, but for noexcept, just
                    // log.
                }
            }

            return std::optional<T>(std::move(back_item));
        } catch (...) {
            spdlog::error(
                "Exception in BoostLockFreeQueue::popBack, returning "
                "std::nullopt.");
            return std::nullopt;
        }
    }

    /**
     * @brief Steal an element from the queue (same as popFront for consistency)
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
                if (!queue_.push(item)) {
                    spdlog::error(
                        "Failed to push target item in "
                        "BoostLockFreeQueue::rotateToFront.");
                }
            }

            // Push back all other items
            for (auto& stored_item : temp_storage) {
                if (!queue_.push(std::move(stored_item))) {
                    spdlog::error(
                        "Failed to push back stored item in "
                        "BoostLockFreeQueue::rotateToFront.");
                }
            }

            // If item wasn't found, push it to front
            if (!found) {
                T temp_value;
                std::vector<T> rebuild;

                while (queue_.pop(temp_value)) {
                    rebuild.push_back(std::move(temp_value));
                }

                if (!queue_.push(item)) {
                    spdlog::error(
                        "Failed to push item when not found in "
                        "BoostLockFreeQueue::rotateToFront.");
                }

                for (auto& stored_item : rebuild) {
                    if (!queue_.push(std::move(stored_item))) {
                        spdlog::error(
                            "Failed to push back rebuilt item in "
                            "BoostLockFreeQueue::rotateToFront.");
                    }
                }
            }
        } catch (...) {
            spdlog::error("Exception in BoostLockFreeQueue::rotateToFront.");
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
                if (!queue_.push(std::move(temp_storage[i]))) {
                    spdlog::error(
                        "Failed to push back temp_storage item in "
                        "BoostLockFreeQueue::copyFrontAndRotateToBack.");
                }
            }
            if (!queue_.push(front_item)) {  // Push front item to back
                spdlog::error(
                    "Failed to push front_item to back in "
                    "BoostLockFreeQueue::copyFrontAndRotateToBack.");
            }

            return std::optional<T>(front_item);
        } catch (...) {
            spdlog::error(
                "Exception in BoostLockFreeQueue::copyFrontAndRotateToBack, "
                "returning std::nullopt.");
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

// Forward declaration of IO context wrapper
#ifdef ATOM_USE_ASIO
class AsioContextWrapper;
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

#ifdef ATOM_USE_ASIO
        bool useAsioContext = false;  // Whether to use ASIO context
#endif

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

#ifdef ATOM_USE_ASIO
        static Options createAsioEnabled() {
            Options opts = createDefault();
            opts.useAsioContext = true;
            return opts;
        }
#endif
    };

    /**
     * @brief Constructor
     * @param options Thread pool options
     */
    explicit ThreadPool(Options options = Options::createDefault())
        : options_(std::move(options)), stop_(false), activeThreads_(0) {
        spdlog::info("ThreadPool created with initialThreadCount: {}",
                     options_.initialThreadCount);
#ifdef ATOM_USE_ASIO
        // Initialize ASIO if enabled
        if (options_.useAsioContext) {
            initAsioContext();
        }
#endif

        // Initialize threads
        size_t numThreads = options_.initialThreadCount;
        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
            spdlog::info("Initial thread count set to hardware_concurrency: {}",
                         numThreads);
        }

        // Ensure at least one thread
        numThreads = std::max(size_t(1), numThreads);

        // Initialize local queues for work stealing
        localTaskQueues_.resize(numThreads);

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
    ~ThreadPool() {
        spdlog::info("ThreadPool destructor called, shutting down.");
        shutdown();
#ifdef ATOM_USE_ASIO
        // Clean up ASIO context
        if (asioContext_) {
            asioContext_.reset();
        }
#endif
    }

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

#ifdef ATOM_USE_ASIO
        // If using ASIO and context is available, delegate to ASIO
        // implementation
        if (options_.useAsioContext && asioContext_) {
            spdlog::debug("Submitting task to ASIO context.");
            return submitAsio<ResultType>(std::forward<F>(f),
                                          std::forward<Args>(args)...);
        }
#endif

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
            std::unique_lock lock(queueMutex_);  // Global queue mutex

            // Check if we need to increase thread count
            if (options_.allowThreadGrowth &&
                getTotalQueuedTasks() >= activeThreads_ &&
                workers_.size() < options_.maxThreadCount) {
                spdlog::info(
                    "Growing thread pool: current tasks {} >= active threads "
                    "{}, workers {}",
                    getTotalQueuedTasks(), activeThreads_.load(),
                    workers_.size());
                createWorkerThread(workers_.size());
            }

            // Check if queue is full (global queue + all local queues)
            if (options_.maxQueueSize > 0 &&
                (globalTaskQueue_.size() + getTotalQueuedTasks()) >=
                    options_.maxQueueSize) {
                spdlog::error(
                    "Thread pool task queue is full, maxQueueSize: {}",
                    options_.maxQueueSize);
                throw std::runtime_error("Thread pool task queue is full");
            }

            // Add task to global queue
            globalTaskQueue_.pushBack([task]() { (*task)(); });
            spdlog::debug(
                "Task submitted to global queue. Global queue size: {}",
                globalTaskQueue_.size());
        }

        // Notify a waiting thread
        condition_.notify_one();

        // Return enhanced future
        return EnhancedFuture<ResultType>(future.share());
    }

#ifdef ATOM_USE_ASIO
    /**
     * @brief Submit a task using ASIO
     * @tparam ResultType Type of the result
     * @tparam F Function type
     * @tparam Args Argument types
     * @param f Function to execute
     * @param args Function arguments
     * @return EnhancedFuture containing the task result
     */
    template <typename ResultType, typename F, typename... Args>
        requires std::invocable<F, Args...>
    auto submitAsio(F&& f, Args&&... args) {
        // Create a shared state for promise and future
        auto promise = std::make_shared<std::promise<ResultType>>();
        auto future = promise->get_future();

        // Post the task to ASIO
        asio::post(*asioContext_->getContext(), [promise,
                                                 func = std::forward<F>(f),
                                                 ... largs = std::forward<Args>(
                                                     args)]() mutable {
            try {
                if constexpr (std::is_void_v<ResultType>) {
                    std::invoke(std::forward<F>(func),
                                std::forward<Args>(largs)...);
                    promise->set_value();
                } else {
                    promise->set_value(std::invoke(
                        std::forward<F>(func), std::forward<Args>(largs)...));
                }
            } catch (const std::exception& e) {
                spdlog::error("Exception in ASIO task: {}", e.what());
                promise->set_exception(std::current_exception());
            } catch (...) {
                spdlog::error("Unknown exception in ASIO task.");
                promise->set_exception(std::current_exception());
            }
        });

        // Return enhanced future
        return EnhancedFuture<ResultType>(future.share());
    }

    /**
     * @brief Get the underlying ASIO context
     * @return Pointer to the ASIO context or nullptr if not using ASIO
     */
    auto getAsioContext() -> asio::io_context* {
        if (asioContext_) {
            return asioContext_->getContext();
        }
        return nullptr;
    }
#endif

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
        spdlog::debug("Submitted batch of {} tasks.", futures.size());
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

#ifdef ATOM_USE_ASIO
        // If using ASIO and context is available, use ASIO for execution
        if (options_.useAsioContext && asioContext_) {
            spdlog::debug("Submitting task with promise to ASIO context.");
            asio::post(
                *asioContext_->getContext(),
                [promise, func = std::forward<F>(f),
                 ... largs = std::forward<Args>(args)]() mutable {
                    try {
                        if constexpr (std::is_void_v<ResultType>) {
                            std::invoke(std::forward<F>(func),
                                        std::forward<Args>(largs)...);
                            promise.setValue();
                        } else {
                            promise.setValue(
                                std::invoke(std::forward<F>(func),
                                            std::forward<Args>(largs)...));
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Exception in ASIO promise task: {}",
                                      e.what());
                        promise.setException(std::current_exception());
                    } catch (...) {
                        spdlog::error(
                            "Unknown exception in ASIO promise task.");
                        promise.setException(std::current_exception());
                    }
                });

            return promise;
        }
#endif

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
            } catch (const std::exception& e) {
                spdlog::error("Exception in promise task: {}", e.what());
                promise.setException(std::current_exception());
            } catch (...) {
                spdlog::error("Unknown exception in promise task.");
                promise.setException(std::current_exception());
            }
        };

        // Queue the task
        {
            std::unique_lock lock(queueMutex_);

            // Check if we need to increase thread count
            if (options_.allowThreadGrowth &&
                getTotalQueuedTasks() >= activeThreads_ &&
                workers_.size() < options_.maxThreadCount) {
                spdlog::info(
                    "Growing thread pool for promise task: current tasks {} >= "
                    "active threads {}, workers {}",
                    getTotalQueuedTasks(), activeThreads_.load(),
                    workers_.size());
                createWorkerThread(workers_.size());
            }

            // Check if queue is full
            if (options_.maxQueueSize > 0 &&
                (globalTaskQueue_.size() + getTotalQueuedTasks()) >=
                    options_.maxQueueSize) {
                spdlog::error(
                    "Thread pool task queue is full for promise task, "
                    "maxQueueSize: {}",
                    options_.maxQueueSize);
                throw std::runtime_error("Thread pool task queue is full");
            }

            // Add task
            globalTaskQueue_.pushBack(std::move(task));
            spdlog::debug(
                "Promise task submitted to global queue. Global queue size: {}",
                globalTaskQueue_.size());
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
#ifdef ATOM_USE_ASIO
        // If using ASIO and context is available, use ASIO for execution
        if (options_.useAsioContext && asioContext_) {
            spdlog::debug("Executing task via ASIO context.");
            asio::post(*asioContext_->getContext(), std::forward<F>(f));
            return;
        }
#endif

        {
            std::unique_lock lock(queueMutex_);
            if (options_.maxQueueSize > 0 &&
                (globalTaskQueue_.size() + getTotalQueuedTasks()) >=
                    options_.maxQueueSize) {
                spdlog::error(
                    "Thread pool task queue is full for execute task, "
                    "maxQueueSize: {}",
                    options_.maxQueueSize);
                throw std::runtime_error("Thread pool task queue is full");
            }
            globalTaskQueue_.pushBack(std::forward<F>(f));
            spdlog::debug(
                "Execute task submitted to global queue. Global queue size: {}",
                globalTaskQueue_.size());
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
            spdlog::warn(
                "Cannot enqueue detached task: Thread pool is shutting down.");
            throw ThreadPoolError(
                "Cannot enqueue detached task: Thread pool is shutting down");
        }

#ifdef ATOM_USE_ASIO
        // If using ASIO and context is available, use ASIO for execution
        if (options_.useAsioContext && asioContext_) {
            spdlog::debug("Enqueuing detached task via ASIO context.");
            asio::post(
                *asioContext_->getContext(),
                [func = std::forward<Function>(func),
                 ... largs = std::forward<Args>(args)]() mutable {
                    try {
                        if constexpr (std::is_same_v<
                                          void, std::invoke_result_t<
                                                    Function&&, Args&&...>>) {
                            std::invoke(func, largs...);
                        } else {
                            std::ignore = std::invoke(func, largs...);
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Exception in detached ASIO task: {}",
                                      e.what());
                    } catch (...) {
                        spdlog::error(
                            "Unknown exception in detached ASIO task.");
                    }
                });

            return;
        }
#endif

        try {
            {
                std::unique_lock lock(queueMutex_);

                // Check if queue is full
                if (options_.maxQueueSize > 0 &&
                    (globalTaskQueue_.size() + getTotalQueuedTasks()) >=
                        options_.maxQueueSize) {
                    spdlog::error(
                        "Thread pool task queue is full for detached task, "
                        "maxQueueSize: {}",
                        options_.maxQueueSize);
                    throw ThreadPoolError("Thread pool task queue is full");
                }

                // Add task
                globalTaskQueue_.pushBack([func = std::forward<Function>(func),
                                           ... largs = std::forward<Args>(
                                               args)]() mutable {
                    try {
                        if constexpr (std::is_same_v<
                                          void, std::invoke_result_t<
                                                    Function&&, Args&&...>>) {
                            std::invoke(func, largs...);
                        } else {
                            std::ignore = std::invoke(func, largs...);
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Exception in detached task: {}",
                                      e.what());
                    } catch (...) {
                        spdlog::error("Unknown exception in detached task.");
                    }
                });
                spdlog::debug(
                    "Detached task submitted to global queue. Global queue "
                    "size: {}",
                    globalTaskQueue_.size());
            }
            condition_.notify_one();
        } catch (const std::exception& e) {
            spdlog::error("Failed to enqueue detached task: {}", e.what());
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
        return globalTaskQueue_.size();
    }

    /**
     * @brief Get total queued tasks across all queues (global + local)
     * @return Total task count
     */
    [[nodiscard]] size_t getTotalQueuedTasks() const {
        size_t total = globalTaskQueue_.size();
        for (const auto& localQueue : localTaskQueues_) {
            total += localQueue.size();
        }
        return total;
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
            spdlog::error("Thread pool size cannot be zero.");
            throw std::invalid_argument("Thread pool size cannot be zero");
        }

        std::unique_lock lock(queueMutex_);

        size_t currentSize = workers_.size();
        spdlog::info("Resizing thread pool from {} to {} threads.", currentSize,
                     newSize);

        if (newSize > currentSize) {
            // Increase threads
            if (!options_.allowThreadGrowth) {
                spdlog::warn(
                    "Thread growth is disabled, cannot resize from {} to {}.",
                    currentSize, newSize);
                throw std::runtime_error(
                    "Thread growth is disabled in this pool");
            }

            if (options_.maxThreadCount > 0 &&
                newSize > options_.maxThreadCount) {
                spdlog::warn(
                    "New size {} exceeds maxThreadCount {}, capping to max.",
                    newSize, options_.maxThreadCount);
                newSize = options_.maxThreadCount;
            }

            // Resize local queues vector first
            localTaskQueues_.resize(newSize);

            for (size_t i = currentSize; i < newSize; ++i) {
                createWorkerThread(i);
            }
        } else if (newSize < currentSize) {
            // Decrease threads
            if (!options_.allowThreadShrink) {
                spdlog::warn(
                    "Thread shrinking is disabled, cannot resize from {} to "
                    "{}.",
                    currentSize, newSize);
                throw std::runtime_error(
                    "Thread shrinking is disabled in this pool");
            }

            // Mark excess threads for termination
            for (size_t i = newSize; i < currentSize; ++i) {
                if (i < terminationFlags_.size()) {  // Ensure index is valid
                    terminationFlags_[i] = true;
                }
            }

            // Unlock mutex to avoid deadlock
            lock.unlock();

            // Wake up all threads to check termination flags
            condition_.notify_all();
            spdlog::info("Signaled {} threads for termination.",
                         currentSize - newSize);
        }
    }

    /**
     * @brief Shutdown the thread pool, wait for all tasks to complete
     */
    void shutdown() {
        {
            std::unique_lock lock(queueMutex_);
            stop_ = true;
            spdlog::info("ThreadPool shutdown initiated.");
        }

        // Notify all threads
        condition_.notify_all();

        // Wait for all threads to finish
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
                spdlog::debug("Worker thread joined.");
            }
        }
        workers_.clear();  // Clear worker threads after joining

        // Clear all queues
        globalTaskQueue_.clear();
        for (auto& localQueue : localTaskQueues_) {
            localQueue.clear();
        }
        localTaskQueues_.clear();  // Clear local queues vector

#ifdef ATOM_USE_ASIO
        // Stop ASIO context
        if (asioContext_) {
            asioContext_->stop();
            spdlog::info("ASIO context stopped.");
        }
#endif
        spdlog::info("ThreadPool shutdown complete.");
    }

    /**
     * @brief Immediately stop the thread pool, discard unfinished tasks
     */
    void shutdownNow() {
        {
            std::unique_lock lock(queueMutex_);
            stop_ = true;
            globalTaskQueue_.clear();  // Discard global tasks
            for (auto& localQueue : localTaskQueues_) {
                localQueue.clear();  // Discard local tasks
            }
            spdlog::info(
                "ThreadPool shutdownNow initiated, discarding all tasks.");
        }

        // Notify all threads
        condition_.notify_all();

        // Wait for all threads to finish
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
                spdlog::debug("Worker thread joined during shutdownNow.");
            }
        }
        workers_.clear();

        localTaskQueues_.clear();

#ifdef ATOM_USE_ASIO
        // Stop ASIO context
        if (asioContext_) {
            asioContext_->stop();
            spdlog::info("ASIO context stopped during shutdownNow.");
        }
#endif
        spdlog::info("ThreadPool shutdownNow complete.");
    }

    /**
     * @brief Wait for all current tasks to complete
     */
    void waitForTasks() {
        spdlog::info("Waiting for all tasks to complete.");
        std::unique_lock lock(queueMutex_);
        waitEmpty_.wait(lock, [this] {
            return getTotalQueuedTasks() == 0 && activeThreads_ == 0;
        });
        spdlog::info("All tasks completed.");
    }

    /**
     * @brief Wait for an available thread
     */
    void waitForAvailableThread() {
        spdlog::debug("Waiting for an available thread.");
        std::unique_lock lock(queueMutex_);
        waitAvailable_.wait(
            lock, [this] { return activeThreads_ < workers_.size() || stop_; });
        spdlog::debug("Thread available or pool stopped.");
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

#ifdef ATOM_USE_ASIO
    [[nodiscard]] bool isAsioEnabled() const {
        return options_.useAsioContext && asioContext_ != nullptr;
    }
#endif

private:
#ifdef ATOM_USE_ASIO
    /**
     * @brief Wrapper for ASIO context
     */
    class AsioContextWrapper {
    public:
        AsioContextWrapper() : context_(std::make_unique<asio::io_context>()) {
            spdlog::debug("ASIO context wrapper created.");
            // Start the work guard to prevent io_context from running out of
            // work
            workGuard_ = std::make_unique<asio::io_context::work>(*context_);
        }

        ~AsioContextWrapper() {
            spdlog::debug("ASIO context wrapper destroyed.");
            stop();
        }

        void stop() {
            if (workGuard_) {
                // Reset work guard to allow run() to exit when queue is empty
                workGuard_.reset();
                spdlog::debug("ASIO work guard reset.");

                // Stop the context
                context_->stop();
                spdlog::debug("ASIO context stopped.");
            }
        }

        auto getContext() -> asio::io_context* { return context_.get(); }

    private:
        std::unique_ptr<asio::io_context> context_;
        std::unique_ptr<asio::io_context::work> workGuard_;
    };

    /**
     * @brief Initialize ASIO context
     */
    void initAsioContext() {
        asioContext_ = std::make_unique<AsioContextWrapper>();
        spdlog::info("ASIO context initialized.");
    }
#endif

    /**
     * @brief Create a worker thread
     * @param id Thread ID
     */
    void createWorkerThread(size_t id) {
        // Don't create if we've reached max thread count
        if (options_.maxThreadCount > 0 &&
            workers_.size() >= options_.maxThreadCount) {
            spdlog::warn(
                "Max thread count reached, not creating new worker thread {}.",
                id);
            return;
        }

        // Initialize termination flag
        if (id >= terminationFlags_.size()) {
            terminationFlags_.resize(id + 1, false);
        } else {
            terminationFlags_[id] = false;  // Reset if reusing ID
        }

        // Create worker thread
        workers_.emplace_back([this, id]() {
            spdlog::info("Worker thread {} started.", id);
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
                bool taskFound = false;

                // Try to get a task from local queue first
                if (options_.useWorkStealing) {
                    task = localTaskQueues_[id].popFront().value_or(nullptr);
                    if (task) {
                        taskFound = true;
                        spdlog::debug("Worker {} got task from local queue.",
                                      id);
                    }
                }

                if (!taskFound) {
                    std::unique_lock lock(queueMutex_);

                    // Wait for task or stop signal
                    auto waitResult = condition_.wait_for(
                        lock, options_.threadIdleTimeout, [this, id] {
                            return stop_ || !globalTaskQueue_.empty() ||
                                   terminationFlags_[id];
                        });

                    // If timeout and thread shrinking allowed, check if we
                    // should terminate
                    if (!waitResult && options_.allowThreadShrink &&
                        workers_.size() > options_.initialThreadCount) {
                        // If idle time exceeds threshold and current thread
                        // count exceeds initial count
                        spdlog::info(
                            "Worker {} idle timeout, considering termination.",
                            id);
                        terminationFlags_[id] = true;
                    }

                    // Check if thread should terminate
                    if ((stop_ || terminationFlags_[id]) &&
                        globalTaskQueue_.empty()) {
                        // Clear termination flag
                        if (id < terminationFlags_.size()) {
                            terminationFlags_[id] = false;
                        }
                        spdlog::info("Worker thread {} terminating.", id);
                        return;
                    }

                    // If global queue is empty, continue waiting or try
                    // stealing
                    if (globalTaskQueue_.empty()) {
                        // If work stealing is enabled, try to steal from other
                        // queues
                        if (options_.useWorkStealing) {
                            lock.unlock();  // Unlock global mutex before
                                            // stealing
                            task = tryStealTasks(id).value_or(nullptr);
                            if (task) {
                                taskFound = true;
                                spdlog::debug("Worker {} stole a task.", id);
                            } else {
                                // If no task found after stealing, re-lock and
                                // continue waiting
                                lock.lock();
                                continue;
                            }
                        } else {
                            continue;  // No work stealing, just wait
                        }
                    } else {
                        // Get task from global queue
                        task = globalTaskQueue_.popFront().value_or(nullptr);
                        if (task) {
                            taskFound = true;
                            spdlog::debug(
                                "Worker {} got task from global queue.", id);
                        }
                    }

                    // Notify potential waiting submitters
                    if (taskFound) {
                        waitAvailable_.notify_one();
                    }
                }

                // Execute task if found
                if (taskFound && task) {
                    activeThreads_++;
                    try {
                        task();
                    } catch (const std::exception& e) {
                        spdlog::error(
                            "Exception in worker {} task execution: {}", id,
                            e.what());
                    } catch (...) {
                        spdlog::error(
                            "Unknown exception in worker {} task execution.",
                            id);
                    }
                    activeThreads_--;
                }

                // If no active threads and all task queues are empty, notify
                // waiters
                {
                    std::unique_lock lock(queueMutex_);
                    if (activeThreads_ == 0 && getTotalQueuedTasks() == 0) {
                        waitEmpty_.notify_all();
                    }
                }
            }
        });

        // Set custom stack size if needed
#ifdef ATOM_PLATFORM_WINDOWS
        if (options_.setStackSize && options_.stackSize > 0) {
            // In Windows, can't directly change stack size of already created
            // thread This would only log a message in a real implementation
            spdlog::warn(
                "Cannot set stack size for already created thread on Windows. "
                "Set stackSize before thread creation.");
        }
#endif
    }

    /**
     * @brief Try to steal tasks from other threads
     * @param currentThreadId The ID of the thread attempting to steal
     * @return An optional containing the stolen task, or std::nullopt if no
     * task was stolen
     */
    [[nodiscard]] auto tryStealTasks(size_t currentThreadId) noexcept
        -> std::optional<std::function<void()>> {
        if (!options_.useWorkStealing) {
            return std::nullopt;
        }

        // Iterate through other threads' local queues to steal
        for (size_t i = 0; i < localTaskQueues_.size(); ++i) {
            if (i == currentThreadId) {
                continue;  // Don't steal from self
            }

            // Try to steal from the back of another thread's queue
            auto stolenTask =
                localTaskQueues_[i].popBack();  // Use popBack for work stealing
            if (stolenTask) {
                spdlog::debug(
                    "Worker {} successfully stole a task from worker {}.",
                    currentThreadId, i);
                return stolenTask;
            }
        }
        spdlog::debug("Worker {} failed to steal any tasks.", currentThreadId);
        return std::nullopt;
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
        if (!SetThreadPriority(GetCurrentThread(), winPriority)) {
            spdlog::warn("Failed to set thread priority on Windows.");
        } else {
            spdlog::debug("Thread priority set to {} on Windows.",
                          static_cast<int>(priority));
        }
#elif defined(ATOM_PLATFORM_LINUX) || defined(ATOM_PLATFORM_MACOS)
        int policy;
        struct sched_param param;
        if (pthread_getschedparam(pthread_self(), &policy, &param) != 0) {
            spdlog::warn("Failed to get thread scheduling parameters.");
            return;
        }

        int min_prio = sched_get_priority_min(policy);
        int max_prio = sched_get_priority_max(policy);

        switch (priority) {
            case Options::ThreadPriority::Lowest:
                param.sched_priority = min_prio;
                break;
            case Options::ThreadPriority::BelowNormal:
                param.sched_priority = min_prio + (max_prio - min_prio) / 4;
                break;
            case Options::ThreadPriority::Normal:
                param.sched_priority = min_prio + (max_prio - min_prio) / 2;
                break;
            case Options::ThreadPriority::AboveNormal:
                param.sched_priority = max_prio - (max_prio - min_prio) / 4;
                break;
            case Options::ThreadPriority::Highest:
            case Options::ThreadPriority::TimeCritical:
                param.sched_priority = max_prio;
                break;
            default:
                param.sched_priority = min_prio + (max_prio - min_prio) / 2;
        }

        if (pthread_setschedparam(pthread_self(), policy, &param) != 0) {
            spdlog::warn("Failed to set thread priority on Linux/macOS.");
        } else {
            spdlog::debug("Thread priority set to {} on Linux/macOS.",
                          static_cast<int>(priority));
        }
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
            spdlog::debug("Single core system, no need for CPU affinity.");
            return;  // No need for affinity on single-core systems
        }

        unsigned int coreId = 0;

        switch (options_.cpuAffinityMode) {
            case Options::CpuAffinityMode::Sequential:
                coreId = threadId % numCores;
                break;

            case Options::CpuAffinityMode::Spread:
                // Try to spread threads across different physical cores
                coreId = (threadId * 2) % numCores;  // Simple heuristic
                break;

            case Options::CpuAffinityMode::CorePinned:
                if (!options_.pinnedCores.empty()) {
                    coreId = options_.pinnedCores[threadId %
                                                  options_.pinnedCores.size()];
                } else {
                    spdlog::warn(
                        "CorePinned affinity mode selected but no pinnedCores "
                        "specified. Defaulting to sequential.");
                    coreId = threadId % numCores;
                }
                break;

            case Options::CpuAffinityMode::Automatic:
                // Automatic mode relies on OS scheduling, no explicit action
                // here
                spdlog::debug(
                    "CPU affinity mode set to Automatic, relying on OS "
                    "scheduling.");
                return;

            default:
                spdlog::warn("Unknown CPU affinity mode selected.");
                return;
        }

        spdlog::debug("Setting CPU affinity for thread {} to core {}.",
                      threadId, coreId);
        // Set CPU affinity
#if defined(ATOM_PLATFORM_WINDOWS)
        DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << coreId);
        if (SetThreadAffinityMask(GetCurrentThread(), mask) == 0) {
            spdlog::warn("Failed to set thread affinity mask on Windows.");
        }
#elif defined(ATOM_PLATFORM_LINUX)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(coreId, &cpuset);
        if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t),
                                   &cpuset) != 0) {
            spdlog::warn("Failed to set thread affinity on Linux.");
        }
#elif defined(ATOM_PLATFORM_MACOS)
        // macOS only supports soft affinity through thread policy
        thread_affinity_policy_data_t policy = {static_cast<integer_t>(coreId)};
        if (thread_policy_set(pthread_mach_thread_np(pthread_self()),
                              THREAD_AFFINITY_POLICY, (thread_policy_t)&policy,
                              THREAD_AFFINITY_POLICY_COUNT) != KERN_SUCCESS) {
            spdlog::warn("Failed to set thread affinity policy on macOS.");
        }
#endif
    }

private:
    Options options_;                   // Thread pool configuration
    std::atomic<bool> stop_;            // Stop flag
    std::vector<std::thread> workers_;  // Worker threads

    // Global task queue, used for initial task submission
    DefaultQueueType<std::function<void()>> globalTaskQueue_;

    // Local task queues for each worker thread, used for work stealing
    std::vector<DefaultQueueType<std::function<void()>>> localTaskQueues_;

    std::vector<bool> terminationFlags_;  // Thread termination flags

    mutable std::mutex queueMutex_;  // Mutex protecting global task queue and
                                     // worker/terminationFlags vectors
    std::condition_variable
        condition_;  // Condition variable for thread waiting for tasks
    std::condition_variable
        waitEmpty_;  // Condition variable for waiting for all tasks to complete
    std::condition_variable waitAvailable_;  // Condition variable for waiting
                                             // for an available thread

    std::atomic<size_t> activeThreads_;  // Current active thread count

#ifdef ATOM_USE_ASIO
    // ASIO context
    std::unique_ptr<AsioContextWrapper> asioContext_;
#endif
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

#ifdef ATOM_USE_ASIO
// ASIO-enabled thread pool singleton
inline ThreadPool& asioThreadPool() {
    static ThreadPool instance(ThreadPool::Options::createAsioEnabled());
    return instance;
}
#endif

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

#ifdef ATOM_USE_ASIO
/**
 * @brief Asynchronously execute a task in the ASIO thread pool
 * @tparam F Function type
 * @tparam Args Argument types
 * @param f Function to execute
 * @param args Function arguments
 * @return EnhancedFuture containing the task result
 */
template <typename F, typename... Args>
    requires std::invocable<F, Args...>
auto asyncAsio(F&& f, Args&&... args) {
    return asioThreadPool().submit(std::forward<F>(f),
                                   std::forward<Args>(args)...);
}
#endif

}  // namespace atom::async

#endif  // ATOM_ASYNC_THREADPOOL_HPP
