/*
 * async.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: A simple but useful async worker manager

**************************************************/

#ifndef ATOM_ASYNC_ASYNC_HPP
#define ATOM_ASYNC_ASYNC_HPP

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
#define ATOM_PLATFORM_WINDOWS
#include <windows.h>
#elif defined(__APPLE__)
#define ATOM_PLATFORM_MACOS
#include <mach/thread_policy.h>
#include <pthread.h>
#else
#define ATOM_PLATFORM_LINUX
#include <pthread.h>
#include <sched.h>
#endif

#include <chrono>
#include <cmath>
#include <concepts>
#include <coroutine>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/lockfree/queue.hpp>
#endif

#include "atom/async/future.hpp"
#include "atom/error/exception.hpp"

class TimeoutException : public atom::error::RuntimeError {
public:
    using atom::error::RuntimeError::RuntimeError;
};

#define THROW_TIMEOUT_EXCEPTION(...)                                       \
    throw TimeoutException(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                           __VA_ARGS__);

// Platform-specific threading utilities
namespace atom::platform {

// Priority ranges for different platforms
struct Priority {
#ifdef ATOM_PLATFORM_WINDOWS
    static constexpr int LOW = THREAD_PRIORITY_BELOW_NORMAL;
    static constexpr int NORMAL = THREAD_PRIORITY_NORMAL;
    static constexpr int HIGH = THREAD_PRIORITY_ABOVE_NORMAL;
    static constexpr int CRITICAL = THREAD_PRIORITY_HIGHEST;
#elif defined(ATOM_PLATFORM_MACOS)
    static constexpr int LOW = 15;
    static constexpr int NORMAL = 31;
    static constexpr int HIGH = 47;
    static constexpr int CRITICAL = 63;
#else  // Linux
    static constexpr int LOW = 1;
    static constexpr int NORMAL = 50;
    static constexpr int HIGH = 75;
    static constexpr int CRITICAL = 99;
#endif
};

namespace detail {

#ifdef ATOM_PLATFORM_WINDOWS
inline bool setPriorityImpl(std::thread::native_handle_type handle,
                            int priority) noexcept {
    return ::SetThreadPriority(reinterpret_cast<HANDLE>(handle), priority) != 0;
}

inline int getCurrentPriorityImpl(
    std::thread::native_handle_type handle) noexcept {
    return ::GetThreadPriority(reinterpret_cast<HANDLE>(handle));
}

inline bool setAffinityImpl(std::thread::native_handle_type handle,
                            size_t cpu) noexcept {
    const DWORD_PTR mask = static_cast<DWORD_PTR>(1ull << cpu);
    return ::SetThreadAffinityMask(reinterpret_cast<HANDLE>(handle), mask) != 0;
}

#elif defined(ATOM_PLATFORM_MACOS)
bool setPriorityImpl(std::thread::native_handle_type handle,
                     int priority) noexcept {
    sched_param param{};
    param.sched_priority = priority;
    return pthread_setschedparam(handle, SCHED_FIFO, &param) == 0;
}

int getCurrentPriorityImpl(std::thread::native_handle_type handle) noexcept {
    sched_param param{};
    int policy;
    if (pthread_getschedparam(handle, &policy, &param) == 0) {
        return param.sched_priority;
    }
    return Priority::NORMAL;
}

bool setAffinityImpl(std::thread::native_handle_type handle,
                     size_t cpu) noexcept {
    thread_affinity_policy_data_t policy{static_cast<integer_t>(cpu)};
    return thread_policy_set(pthread_mach_thread_np(handle),
                             THREAD_AFFINITY_POLICY,
                             reinterpret_cast<thread_policy_t>(&policy),
                             THREAD_AFFINITY_POLICY_COUNT) == KERN_SUCCESS;
}

#else  // Linux
bool setPriorityImpl(std::thread::native_handle_type handle,
                     int priority) noexcept {
    sched_param param{};
    param.sched_priority = priority;
    return pthread_setschedparam(handle, SCHED_FIFO, &param) == 0;
}

int getCurrentPriorityImpl(std::thread::native_handle_type handle) noexcept {
    sched_param param{};
    int policy;
    if (pthread_getschedparam(handle, &policy, &param) == 0) {
        return param.sched_priority;
    }
    return Priority::NORMAL;
}

bool setAffinityImpl(std::thread::native_handle_type handle,
                     size_t cpu) noexcept {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    return pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset) == 0;
}
#endif

}  // namespace detail

}  // namespace atom::platform

namespace atom::platform {
inline bool setPriority(std::thread::native_handle_type handle,
                        int priority) noexcept {
    return detail::setPriorityImpl(handle, priority);
}

inline int getCurrentPriority(std::thread::native_handle_type handle) noexcept {
    return detail::getCurrentPriorityImpl(handle);
}

inline bool setAffinity(std::thread::native_handle_type handle,
                        size_t cpu) noexcept {
    return detail::setAffinityImpl(handle, cpu);
}

// RAII thread priority guard
class [[nodiscard]] ThreadPriorityGuard {
public:
    explicit ThreadPriorityGuard(std::thread::native_handle_type handle,
                                 int priority)
        : handle_(handle) {
        original_priority_ = getCurrentPriority(handle_);
        setPriority(handle_, priority);
    }

    ~ThreadPriorityGuard() noexcept {
        try {
            setPriority(handle_, original_priority_);
        } catch (...) {
        }  // Best-effort restore
    }

    ThreadPriorityGuard(const ThreadPriorityGuard&) = delete;
    ThreadPriorityGuard& operator=(const ThreadPriorityGuard&) = delete;
    ThreadPriorityGuard(ThreadPriorityGuard&&) = delete;
    ThreadPriorityGuard& operator=(ThreadPriorityGuard&&) = delete;

private:
    std::thread::native_handle_type handle_;
    int original_priority_;
};

// Thread scheduling utilities
inline void yieldThread() noexcept { std::this_thread::yield(); }

inline void sleepFor(std::chrono::nanoseconds duration) noexcept {
    std::this_thread::sleep_for(duration);
}
}  // namespace atom::platform

namespace atom::async {

// C++20 concepts for improved type safety
template <typename T>
concept Invocable = requires { std::is_invocable_v<T>; };

template <typename T>
concept Callable = requires(T t) { t(); };

template <typename Func, typename... Args>
concept InvocableWithArgs =
    requires(Func f, Args... args) { std::invoke(f, args...); };

template <typename T>
concept NonVoidType = !std::is_void_v<T>;

/**
 * @brief Class for performing asynchronous tasks.
 *
 * This class allows you to start a task asynchronously and get the result when
 * it's done. It also provides functionality to cancel the task, check if it's
 * done or active, validate the result, set a callback function, and set a
 * timeout.
 *
 * @tparam ResultType The type of the result returned by the task.
 */
// Forward declaration
template <typename T>
class WorkerContainer;

// Forward declaration of the primary template
template <typename ResultType>
class AsyncWorker;

// Specialization for void
template <>
class AsyncWorker<void> {
    friend class WorkerContainer<void>;

private:
    // Task state
    enum class State : uint8_t {
        INITIAL,    // Task not started
        RUNNING,    // Task is executing
        CANCELLED,  // Task was cancelled
        COMPLETED,  // Task completed successfully
        FAILED      // Task encountered an error
    };

    // Task management
    std::atomic<State> state_{State::INITIAL};
    std::future<void> task_;
    std::function<void()> callback_;
    std::chrono::seconds timeout_{0};

    // Thread configuration
    int desired_priority_{static_cast<int>(platform::Priority::NORMAL)};
    size_t preferred_cpu_{std::numeric_limits<size_t>::max()};
    std::unique_ptr<platform::ThreadPriorityGuard> priority_guard_;

    // Helper to get current thread native handle
    static auto getCurrentThreadHandle() noexcept {
        return
#ifdef ATOM_PLATFORM_WINDOWS
            GetCurrentThread();
#else
            pthread_self();
#endif
    }

public:
    // Task priority levels
    enum class Priority {
        LOW = platform::Priority::LOW,
        NORMAL = platform::Priority::NORMAL,
        HIGH = platform::Priority::HIGH,
        CRITICAL = platform::Priority::CRITICAL
    };

    AsyncWorker() noexcept = default;
    ~AsyncWorker() noexcept {
        if (state_.load(std::memory_order_acquire) != State::COMPLETED) {
            cancel();
        }
    }

    // Rule of five - prevent copy, allow move
    AsyncWorker(const AsyncWorker&) = delete;
    AsyncWorker& operator=(const AsyncWorker&) = delete;

    /**
     * @brief Sets the thread priority for this worker
     * @param priority The priority level
     */
    void setPriority(Priority priority) noexcept {
        desired_priority_ = static_cast<int>(priority);
    }

    /**
     * @brief Sets the preferred CPU core for this worker
     * @param cpu_id The CPU core ID
     */
    void setPreferredCPU(size_t cpu_id) noexcept { preferred_cpu_ = cpu_id; }

    /**
     * @brief Checks if the task has been requested to cancel
     * @return True if cancellation was requested
     */
    [[nodiscard]] bool isCancellationRequested() const noexcept {
        return state_.load(std::memory_order_acquire) == State::CANCELLED;
    }

    /**
     * @brief Starts the task asynchronously.
     *
     * @tparam Func The type of the function to be executed asynchronously.
     * @tparam Args The types of the arguments to be passed to the function.
     * @param func The function to be executed asynchronously.
     * @param args The arguments to be passed to the function.
     * @throws std::invalid_argument If func is null or invalid.
     */
    template <typename Func, typename... Args>
        requires InvocableWithArgs<Func, Args...> &&
                 std::is_same_v<std::invoke_result_t<Func, Args...>, void>
    void startAsync(Func&& func, Args&&... args);

    /**
     * @brief Gets the result of the task (void version).
     *
     * @param timeout Optional timeout duration (0 means no timeout).
     * @throws std::invalid_argument if the task is not valid.
     * @throws TimeoutException if the timeout is reached.
     */
    void getResult(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    /**
     * @brief Cancels the task.
     *
     * If the task is valid, this function waits for the task to complete.
     */
    void cancel() noexcept;

    /**
     * @brief Checks if the task is done.
     *
     * @return True if the task is done, false otherwise.
     */
    [[nodiscard]] auto isDone() const noexcept -> bool;

    /**
     * @brief Checks if the task is active.
     *
     * @return True if the task is active, false otherwise.
     */
    [[nodiscard]] auto isActive() const noexcept -> bool;

    /**
     * @brief Validates the completion of the task (void version).
     *
     * @param validator The function to call to validate completion.
     * @return True if valid, false otherwise.
     */
    auto validate(std::function<bool()> validator) noexcept -> bool;

    /**
     * @brief Sets a callback function to be called when the task is done.
     *
     * @param callback The callback function to be set.
     * @throws std::invalid_argument if callback is empty.
     */
    void setCallback(std::function<void()> callback);

    /**
     * @brief Sets a timeout for the task.
     *
     * @param timeout The timeout duration.
     * @throws std::invalid_argument if timeout is negative.
     */
    void setTimeout(std::chrono::seconds timeout);

    /**
     * @brief Waits for the task to complete.
     *
     * If a timeout is set, this function waits until the task is done or the
     * timeout is reached. If a callback function is set and the task is done,
     * the callback function is called.
     *
     * @throws TimeoutException if the timeout is reached.
     */
    void waitForCompletion();
};

// Primary template for non-void types
template <typename ResultType>
class AsyncWorker {
    friend class WorkerContainer<ResultType>;

private:
    // Task state
    enum class State : uint8_t {
        INITIAL,    // Task not started
        RUNNING,    // Task is executing
        CANCELLED,  // Task was cancelled
        COMPLETED,  // Task completed successfully
        FAILED      // Task encountered an error
    };

    // Task management
    std::atomic<State> state_{State::INITIAL};
    std::future<ResultType> task_;
    std::function<void(ResultType)> callback_;
    std::chrono::seconds timeout_{0};

    // Thread configuration
    int desired_priority_{static_cast<int>(platform::Priority::NORMAL)};
    size_t preferred_cpu_{std::numeric_limits<size_t>::max()};
    std::unique_ptr<platform::ThreadPriorityGuard> priority_guard_;

    // Helper to get current thread native handle
    static auto getCurrentThreadHandle() noexcept {
        return
#ifdef ATOM_PLATFORM_WINDOWS
            GetCurrentThread();
#else
            pthread_self();
#endif
    }

public:
    // Task priority levels
    enum class Priority {
        LOW = platform::Priority::LOW,
        NORMAL = platform::Priority::NORMAL,
        HIGH = platform::Priority::HIGH,
        CRITICAL = platform::Priority::CRITICAL
    };

    AsyncWorker() noexcept = default;
    ~AsyncWorker() noexcept {
        if (state_.load(std::memory_order_acquire) != State::COMPLETED) {
            cancel();
        }
    }

    // Rule of five - prevent copy, allow move
    AsyncWorker(const AsyncWorker&) = delete;
    AsyncWorker& operator=(const AsyncWorker&) = delete;
    AsyncWorker(AsyncWorker&&) noexcept = default;
    AsyncWorker& operator=(AsyncWorker&&) noexcept = default;

    /**
     * @brief Sets the thread priority for this worker
     * @param priority The priority level
     */
    void setPriority(Priority priority) noexcept {
        desired_priority_ = static_cast<int>(priority);
    }

    /**
     * @brief Sets the preferred CPU core for this worker
     * @param cpu_id The CPU core ID
     */
    void setPreferredCPU(size_t cpu_id) noexcept { preferred_cpu_ = cpu_id; }

    /**
     * @brief Checks if the task has been requested to cancel
     * @return True if cancellation was requested
     */
    [[nodiscard]] bool isCancellationRequested() const noexcept {
        return state_.load(std::memory_order_acquire) == State::CANCELLED;
    }

    /**
     * @brief Starts the task asynchronously.
     *
     * @tparam Func The type of the function to be executed asynchronously.
     * @tparam Args The types of the arguments to be passed to the function.
     * @param func The function to be executed asynchronously.
     * @param args The arguments to be passed to the function.
     * @throws std::invalid_argument If func is null or invalid.
     */
    template <typename Func, typename... Args>
        requires InvocableWithArgs<Func, Args...> &&
                 std::is_same_v<std::invoke_result_t<Func, Args...>, ResultType>
    void startAsync(Func&& func, Args&&... args);

    /**
     * @brief Gets the result of the task with timeout option.
     *
     * @param timeout Optional timeout duration (0 means no timeout).
     * @throws std::invalid_argument if the task is not valid.
     * @throws TimeoutException if the timeout is reached.
     * @return The result of the task.
     */
    [[nodiscard]] auto getResult(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0))
        -> ResultType;

    /**
     * @brief Cancels the task.
     *
     * If the task is valid, this function waits for the task to complete.
     */
    void cancel() noexcept;

    /**
     * @brief Checks if the task is done.
     *
     * @return True if the task is done, false otherwise.
     */
    [[nodiscard]] auto isDone() const noexcept -> bool;

    /**
     * @brief Checks if the task is active.
     *
     * @return True if the task is active, false otherwise.
     */
    [[nodiscard]] auto isActive() const noexcept -> bool;

    /**
     * @brief Validates the result of the task using a validator function.
     *
     * @param validator The function used to validate the result.
     * @return True if the result is valid, false otherwise.
     */
    auto validate(std::function<bool(ResultType)> validator) noexcept -> bool;

    /**
     * @brief Sets a callback function to be called when the task is done.
     *
     * @param callback The callback function to be set.
     * @throws std::invalid_argument if callback is empty.
     */
    void setCallback(std::function<void(ResultType)> callback);

    /**
     * @brief Sets a timeout for the task.
     *
     * @param timeout The timeout duration.
     * @throws std::invalid_argument if timeout is negative.
     */
    void setTimeout(std::chrono::seconds timeout);

    /**
     * @brief Waits for the task to complete.
     *
     * If a timeout is set, this function waits until the task is done or the
     * timeout is reached. If a callback function is set and the task is done,
     * the callback function is called with the result.
     *
     * @throws TimeoutException if the timeout is reached.
     */
    void waitForCompletion();
};

#ifdef ATOM_USE_BOOST_LOCKFREE
/**
 * @brief Container class for worker pointers in lockfree queue
 *
 * This class provides a wrapper for storing AsyncWorker pointers in a
 * boost::lockfree::queue. It manages memory ownership to ensure proper
 * cleanup when the container is destroyed.
 *
 * @tparam ResultType The type of the result returned by the workers.
 */
template <typename ResultType>
class WorkerContainer {
public:
    /**
     * @brief Constructs a worker container with specified capacity
     *
     * @param capacity Initial capacity of the queue
     */
    explicit WorkerContainer(size_t capacity = 128) : worker_queue_(capacity) {}

    /**
     * @brief Adds a worker to the container
     *
     * @param worker The worker to add
     * @return true if the worker was successfully added, false otherwise
     */
    bool push(const std::shared_ptr<AsyncWorker<ResultType>>& worker) {
        // Create a copy of the shared_ptr to ensure proper reference counting
        auto* workerPtr = new std::shared_ptr<AsyncWorker<ResultType>>(worker);
        bool pushed = worker_queue_.push(workerPtr);
        if (!pushed) {
            delete workerPtr;
        }
        return pushed;
    }

    /**
     * @brief Retrieves all workers from the container
     *
     * @return Vector of workers retrieved from the container
     */
    std::vector<std::shared_ptr<AsyncWorker<ResultType>>> retrieveAll() {
        std::vector<std::shared_ptr<AsyncWorker<ResultType>>> workers;
        std::shared_ptr<AsyncWorker<ResultType>>* workerPtr = nullptr;
        while (worker_queue_.pop(workerPtr)) {
            if (workerPtr) {
                workers.push_back(*workerPtr);
                delete workerPtr;
            }
        }
        return workers;
    }

    /**
     * @brief Processes all workers with a function
     *
     * @param func Function to apply to each worker
     */
    void forEach(const std::function<
                 void(const std::shared_ptr<AsyncWorker<ResultType>>&)>& func) {
        auto workers = retrieveAll();
        for (const auto& worker : workers) {
            func(worker);
            push(worker);
        }
    }

    /**
     * @brief Removes workers that satisfy a predicate
     *
     * @param predicate Function that returns true for workers to remove
     * @return Number of workers removed
     */
    size_t removeIf(
        const std::function<
            bool(const std::shared_ptr<AsyncWorker<ResultType>>&)>& predicate) {
        auto workers = retrieveAll();
        size_t initial_size = workers.size();

        // Filter workers
        auto it = std::remove_if(workers.begin(), workers.end(), predicate);
        size_t removed = std::distance(it, workers.end());
        workers.erase(it, workers.end());

        // Push back remaining workers
        for (const auto& worker : workers) {
            push(worker);
        }

        return removed;
    }

    /**
     * @brief Checks if all workers satisfy a condition
     *
     * @param condition Function that returns true if a worker satisfies the
     * condition
     * @return true if all workers satisfy the condition, false otherwise
     */
    bool allOf(
        const std::function<
            bool(const std::shared_ptr<AsyncWorker<ResultType>>&)>& condition) {
        auto workers = retrieveAll();
        bool result = std::all_of(workers.begin(), workers.end(), condition);

        // Push back all workers
        for (const auto& worker : workers) {
            push(worker);
        }

        return result;
    }

    /**
     * @brief Counts the number of workers in the container
     *
     * @return Approximate number of workers in the container
     */
    size_t size() const { return worker_queue_.read_available(); }

    /**
     * @brief Destructor that cleans up all worker pointers
     */
    ~WorkerContainer() {
        std::shared_ptr<AsyncWorker<ResultType>>* workerPtr = nullptr;
        while (worker_queue_.pop(workerPtr)) {
            delete workerPtr;
        }
    }

private:
    boost::lockfree::queue<std::shared_ptr<AsyncWorker<ResultType>>*>
        worker_queue_;
};
#endif

/**
 * @brief Class for managing multiple AsyncWorker instances.
 *
 * This class provides functionality to create and manage multiple AsyncWorker
 * instances using modern C++20 features.
 *
 * @tparam ResultType The type of the result returned by the tasks managed by
 * this class.
 */
template <typename ResultType>
class AsyncWorkerManager {
public:
    /**
     * @brief Default constructor.
     */
    AsyncWorkerManager() noexcept = default;

    /**
     * @brief Destructor that ensures cleanup.
     */
    ~AsyncWorkerManager() noexcept {
        try {
            cancelAll();
        } catch (...) {
            // Suppress any exceptions in destructor
        }
    }

    // Rule of five - prevent copy, allow move
    AsyncWorkerManager(const AsyncWorkerManager&) = delete;
    AsyncWorkerManager& operator=(const AsyncWorkerManager&) = delete;
    AsyncWorkerManager(AsyncWorkerManager&&) noexcept = default;
    AsyncWorkerManager& operator=(AsyncWorkerManager&&) noexcept = default;

    /**
     * @brief Creates a new AsyncWorker instance and starts the task
     * asynchronously.
     *
     * @tparam Func The type of the function to be executed asynchronously.
     * @tparam Args The types of the arguments to be passed to the function.
     * @param func The function to be executed asynchronously.
     * @param args The arguments to be passed to the function.
     * @return A shared pointer to the created AsyncWorker instance.
     */
    template <typename Func, typename... Args>
        requires InvocableWithArgs<Func, Args...> &&
                 std::is_same_v<std::invoke_result_t<Func, Args...>, ResultType>
    [[nodiscard]] auto createWorker(Func&& func, Args&&... args)
        -> std::shared_ptr<AsyncWorker<ResultType>>;

    /**
     * @brief Cancels all the managed tasks.
     */
    void cancelAll() noexcept;

    /**
     * @brief Checks if all the managed tasks are done.
     *
     * @return True if all tasks are done, false otherwise.
     */
    [[nodiscard]] auto allDone() const noexcept -> bool;

    /**
     * @brief Waits for all the managed tasks to complete.
     *
     * @param timeout Optional timeout for each task (0 means no timeout)
     * @throws TimeoutException if any task exceeds the timeout.
     */
    void waitForAll(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    /**
     * @brief Checks if a specific task is done.
     *
     * @param worker The AsyncWorker instance to check.
     * @return True if the task is done, false otherwise.
     * @throws std::invalid_argument if worker is null.
     */
    [[nodiscard]] auto isDone(
        std::shared_ptr<AsyncWorker<ResultType>> worker) const -> bool;

    /**
     * @brief Cancels a specific task.
     *
     * @param worker The AsyncWorker instance to cancel.
     * @throws std::invalid_argument if worker is null.
     */
    void cancel(std::shared_ptr<AsyncWorker<ResultType>> worker);

    /**
     * @brief Gets the number of managed workers.
     *
     * @return The number of workers.
     */
    [[nodiscard]] auto size() const noexcept -> size_t;

    /**
     * @brief Removes completed workers from the manager.
     *
     * @return The number of workers removed.
     */
    size_t pruneCompletedWorkers() noexcept;

private:
#ifdef ATOM_USE_BOOST_LOCKFREE
    WorkerContainer<ResultType>
        workers_;  ///< The lockfree container of workers.
#else
    std::vector<std::shared_ptr<AsyncWorker<ResultType>>>
        workers_;               ///< The list of workers.
    mutable std::mutex mutex_;  ///< Thread-safety for concurrent access
#endif
};

// Coroutine support for C++20
template <typename T>
struct TaskPromise;

template <typename T>
class [[nodiscard]] Task {
public:
    using promise_type = TaskPromise<T>;

    Task() noexcept = default;
    explicit Task(std::coroutine_handle<promise_type> handle)
        : handle_(handle) {}
    ~Task() {
        if (handle_ && handle_.done()) {
            handle_.destroy();
        }
    }

    // Rule of five - prevent copy, allow move
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_)
                handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] T await_result() {
        if (!handle_) {
            throw std::runtime_error("Task has no valid coroutine handle");
        }

        if (!handle_.done()) {
            handle_.resume();
        }

        return handle_.promise().result();
    }

    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }

    [[nodiscard]] bool done() const noexcept {
        return !handle_ || handle_.done();
    }

private:
    std::coroutine_handle<promise_type> handle_ = nullptr;
};

template <typename T>
struct TaskPromise {
    T value_;
    std::exception_ptr exception_;

    TaskPromise() noexcept = default;

    Task<T> get_return_object() {
        return Task<T>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
    }

    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }

    void unhandled_exception() { exception_ = std::current_exception(); }

    template <std::convertible_to<T> U>
    void return_value(U&& value) {
        value_ = std::forward<U>(value);
    }

    T result() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
        return std::move(value_);
    }
};

// Template specialization for void
template <>
struct TaskPromise<void> {
    std::exception_ptr exception_;

    TaskPromise() noexcept = default;

    Task<void> get_return_object() {
        return Task<void>{
            std::coroutine_handle<TaskPromise>::from_promise(*this)};
    }

    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }

    void unhandled_exception() { exception_ = std::current_exception(); }

    void return_void() {}

    void result() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }
};

// Retry strategy enum for different backoff strategies
enum class BackoffStrategy { FIXED, LINEAR, EXPONENTIAL };

/**
 * @brief Async execution with retry.
 *
 * This implementation uses enhanced exception handling and validations.
 *
 * @tparam Func The type of the function to be executed asynchronously.
 * @tparam Callback The type of the callback function.
 * @tparam ExceptionHandler The type of the exception handler function.
 * @tparam CompleteHandler The type of the completion handler function.
 * @tparam Args The types of the arguments to be passed to the function.
 * @param func The function to be executed asynchronously.
 * @param attemptsLeft Number of attempts left (must be > 0).
 * @param initialDelay Initial delay between retries.
 * @param strategy The backoff strategy to use.
 * @param maxTotalDelay Maximum total delay allowed.
 * @param callback Callback function called on success.
 * @param exceptionHandler Handler called when exceptions occur.
 * @param completeHandler Handler called when all attempts complete.
 * @param args Arguments to pass to func.
 * @return A future with the result of the async operation.
 * @throws std::invalid_argument If invalid parameters are provided.
 */
template <typename Func, typename Callback, typename ExceptionHandler,
          typename CompleteHandler, typename... Args>
auto asyncRetryImpl(Func&& func, int attemptsLeft,
                    std::chrono::milliseconds initialDelay,
                    BackoffStrategy strategy,
                    std::chrono::milliseconds maxTotalDelay,
                    Callback&& callback, ExceptionHandler&& exceptionHandler,
                    CompleteHandler&& completeHandler, Args&&... args) ->
    typename std::invoke_result_t<Func, Args...> {
    if (attemptsLeft <= 0) {
        throw std::invalid_argument("Attempts must be positive");
    }

    if (initialDelay.count() < 0) {
        throw std::invalid_argument("Initial delay cannot be negative");
    }

    using ReturnType = typename std::invoke_result_t<Func, Args...>;

    auto attempt = std::async(std::launch::async, std::forward<Func>(func),
                              std::forward<Args>(args)...);

    try {
        if constexpr (std::is_same_v<ReturnType, void>) {
            attempt.get();
            callback();
            completeHandler();
            return;
        } else {
            auto result = attempt.get();
            if constexpr (std::is_same_v<ReturnType, void>) {
                callback();
            } else {
                callback(result);
            }
            completeHandler();
            return result;
        }
    } catch (const std::exception& e) {
        exceptionHandler(e);  // Call custom exception handler

        if (attemptsLeft <= 1 || maxTotalDelay.count() <= 0) {
            completeHandler();  // Invoke complete handler on final failure
            throw;
        }

        // Calculate next retry delay based on strategy
        std::chrono::milliseconds nextDelay = initialDelay;
        switch (strategy) {
            case BackoffStrategy::LINEAR:
                nextDelay *= 2;
                break;
            case BackoffStrategy::EXPONENTIAL:
                nextDelay = std::chrono::milliseconds(static_cast<int>(
                    initialDelay.count() * std::pow(2, (5 - attemptsLeft))));
                break;
            default:  // FIXED strategy - keep the same delay
                break;
        }

        // Cap the delay if it exceeds max delay
        nextDelay = std::min(nextDelay, maxTotalDelay);

        std::this_thread::sleep_for(nextDelay);

        // Decrease the maximum total delay by the time spent in the last
        // attempt
        maxTotalDelay -= nextDelay;

        return asyncRetryImpl(std::forward<Func>(func), attemptsLeft - 1,
                              nextDelay, strategy, maxTotalDelay,
                              std::forward<Callback>(callback),
                              std::forward<ExceptionHandler>(exceptionHandler),
                              std::forward<CompleteHandler>(completeHandler),
                              std::forward<Args>(args)...);
    }
}

/**
 * @brief Async execution with retry (C++20 coroutine version).
 *
 * @tparam Func Function type
 * @tparam Args Argument types
 * @param func Function to execute
 * @param attemptsLeft Number of retry attempts
 * @param initialDelay Initial delay between retries
 * @param strategy Backoff strategy
 * @param args Function arguments
 * @return Task with the function result
 */
template <typename Func, typename... Args>
    requires InvocableWithArgs<Func, Args...>
Task<std::invoke_result_t<Func, Args...>> asyncRetryTask(
    Func&& func, int attemptsLeft, std::chrono::milliseconds initialDelay,
    BackoffStrategy strategy, Args&&... args) {
    using ReturnType = std::invoke_result_t<Func, Args...>;

    if (attemptsLeft <= 0) {
        throw std::invalid_argument("Attempts must be positive");
    }

    int attempts = 0;
    while (true) {
        try {
            if constexpr (std::is_same_v<ReturnType, void>) {
                std::invoke(std::forward<Func>(func),
                            std::forward<Args>(args)...);
                co_return;
            } else {
                co_return std::invoke(std::forward<Func>(func),
                                      std::forward<Args>(args)...);
            }
        } catch (const std::exception& e) {
            attempts++;
            if (attempts >= attemptsLeft) {
                throw;  // Re-throw after all attempts
            }

            // Calculate delay based on strategy
            std::chrono::milliseconds delay = initialDelay;
            switch (strategy) {
                case BackoffStrategy::LINEAR:
                    delay = initialDelay * attempts;
                    break;
                case BackoffStrategy::EXPONENTIAL:
                    delay = std::chrono::milliseconds(static_cast<int>(
                        initialDelay.count() * std::pow(2, attempts - 1)));
                    break;
                default:  // FIXED - keep same delay
                    break;
            }

            std::this_thread::sleep_for(delay);
        }
    }
}

/**
 * @brief Creates a future for async retry execution.
 *
 * @tparam Func The type of the function to be executed asynchronously.
 * @tparam Callback The type of the callback function.
 * @tparam ExceptionHandler The type of the exception handler function.
 * @tparam CompleteHandler The type of the completion handler function.
 * @tparam Args The types of the arguments to be passed to the function.
 */
template <typename Func, typename Callback, typename ExceptionHandler,
          typename CompleteHandler, typename... Args>
auto asyncRetry(Func&& func, int attemptsLeft,
                std::chrono::milliseconds initialDelay,
                BackoffStrategy strategy,
                std::chrono::milliseconds maxTotalDelay, Callback&& callback,
                ExceptionHandler&& exceptionHandler,
                CompleteHandler&& completeHandler, Args&&... args)
    -> std::future<typename std::invoke_result_t<Func, Args...>> {
    if (attemptsLeft <= 0) {
        throw std::invalid_argument("Attempts must be positive");
    }

    return std::async(
        std::launch::async, [=, func = std::forward<Func>(func)]() mutable {
            return asyncRetryImpl(
                std::forward<Func>(func), attemptsLeft, initialDelay, strategy,
                maxTotalDelay, std::forward<Callback>(callback),
                std::forward<ExceptionHandler>(exceptionHandler),
                std::forward<CompleteHandler>(completeHandler),
                std::forward<Args>(args)...);
        });
}

/**
 * @brief Creates an enhanced future for async retry execution.
 *
 * @tparam Func The type of the function to be executed asynchronously.
 * @tparam Callback The type of the callback function.
 * @tparam ExceptionHandler The type of the exception handler function.
 * @tparam CompleteHandler The type of the completion handler function.
 * @tparam Args The types of the arguments to be passed to the function.
 */
template <typename Func, typename Callback, typename ExceptionHandler,
          typename CompleteHandler, typename... Args>
auto asyncRetryE(Func&& func, int attemptsLeft,
                 std::chrono::milliseconds initialDelay,
                 BackoffStrategy strategy,
                 std::chrono::milliseconds maxTotalDelay, Callback&& callback,
                 ExceptionHandler&& exceptionHandler,
                 CompleteHandler&& completeHandler, Args&&... args)
    -> EnhancedFuture<typename std::invoke_result_t<Func, Args...>> {
    if (attemptsLeft <= 0) {
        throw std::invalid_argument("Attempts must be positive");
    }

    using ReturnType = typename std::invoke_result_t<Func, Args...>;

    auto future =
        std::async(std::launch::async, [=, func = std::forward<Func>(
                                               func)]() mutable {
            return asyncRetryImpl(
                std::forward<Func>(func), attemptsLeft, initialDelay, strategy,
                maxTotalDelay, std::forward<Callback>(callback),
                std::forward<ExceptionHandler>(exceptionHandler),
                std::forward<CompleteHandler>(completeHandler),
                std::forward<Args>(args)...);
        }).share();

    if constexpr (std::is_same_v<ReturnType, void>) {
        return EnhancedFuture<void>(std::shared_future<void>(future));
    } else {
        return EnhancedFuture<ReturnType>(
            std::shared_future<ReturnType>(future));
    }
}

/**
 * @brief Gets the result of a future with a timeout.
 *
 * @tparam T Result type
 * @tparam Duration Duration type
 * @param future The future to get the result from
 * @param timeout The timeout duration
 * @return The result of the future
 * @throws TimeoutException if the timeout is reached
 * @throws Any exception thrown by the future
 */
template <typename T, typename Duration>
    requires NonVoidType<T>
auto getWithTimeout(std::future<T>& future, Duration timeout) -> T {
    if (timeout.count() < 0) {
        throw std::invalid_argument("Timeout cannot be negative");
    }

    if (!future.valid()) {
        throw std::invalid_argument("Invalid future");
    }

    if (future.wait_for(timeout) == std::future_status::ready) {
        return future.get();
    }
    THROW_TIMEOUT_EXCEPTION("Timeout occurred while waiting for future result");
}

// Implementation of AsyncWorker methods
template <typename ResultType>
template <typename Func, typename... Args>
    requires InvocableWithArgs<Func, Args...> &&
             std::is_same_v<std::invoke_result_t<Func, Args...>, ResultType>
void AsyncWorker<ResultType>::startAsync(Func&& func, Args&&... args) {
    if constexpr (std::is_pointer_v<std::decay_t<Func>>) {
        if (!func) {
            throw std::invalid_argument("Function cannot be null");
        }
    }

    State expected = State::INITIAL;
    if (!state_.compare_exchange_strong(expected, State::RUNNING,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
        throw std::runtime_error("Task already started");
    }

    try {
        auto wrapped_func =
            [this, f = std::forward<Func>(func),
             ... args = std::forward<Args>(args)]() mutable -> ResultType {
            // Set thread priority and CPU affinity at the start of the thread
            auto thread_handle = getCurrentThreadHandle();
            priority_guard_ = std::make_unique<platform::ThreadPriorityGuard>(
                thread_handle, desired_priority_);

            if (preferred_cpu_ != std::numeric_limits<size_t>::max()) {
                platform::setAffinity(thread_handle, preferred_cpu_);
            }

            try {
                if constexpr (std::is_same_v<ResultType, void>) {
                    std::invoke(std::forward<Func>(f),
                                std::forward<Args>(args)...);
                    state_.store(State::COMPLETED, std::memory_order_release);
                } else {
                    auto result = std::invoke(std::forward<Func>(f),
                                              std::forward<Args>(args)...);
                    state_.store(State::COMPLETED, std::memory_order_release);
                    return result;
                }
            } catch (...) {
                state_.store(State::FAILED, std::memory_order_release);
                throw;
            }
        };

        task_ = std::async(std::launch::async, std::move(wrapped_func));
    } catch (const std::exception& e) {
        state_.store(State::FAILED, std::memory_order_release);
        throw std::runtime_error(std::string("Failed to start async task: ") +
                                 e.what());
    }
}

template <typename ResultType>
[[nodiscard]] auto AsyncWorker<ResultType>::getResult(
    std::chrono::milliseconds timeout) -> ResultType {
    if (!task_.valid()) {
        throw std::invalid_argument("Task is not valid");
    }

    if (timeout.count() > 0) {
        if (task_.wait_for(timeout) != std::future_status::ready) {
            THROW_TIMEOUT_EXCEPTION("Task result retrieval timed out");
        }
    }

    return task_.get();
}

template <typename ResultType>
void AsyncWorker<ResultType>::cancel() noexcept {
    try {
        if (task_.valid()) {
            task_.wait();  // Wait for task to complete
        }
    } catch (...) {
        // Suppress exceptions in cancel operation
    }
}

template <typename ResultType>
[[nodiscard]] auto AsyncWorker<ResultType>::isDone() const noexcept -> bool {
    try {
        return task_.valid() && (task_.wait_for(std::chrono::seconds(0)) ==
                                 std::future_status::ready);
    } catch (...) {
        return false;  // In case of any exception, consider not done
    }
}

template <typename ResultType>
[[nodiscard]] auto AsyncWorker<ResultType>::isActive() const noexcept -> bool {
    try {
        return task_.valid() && (task_.wait_for(std::chrono::seconds(0)) ==
                                 std::future_status::timeout);
    } catch (...) {
        return false;  // In case of any exception, consider not active
    }
}

template <typename ResultType>
auto AsyncWorker<ResultType>::validate(
    std::function<bool(ResultType)> validator) noexcept -> bool {
    try {
        if (!validator)
            return false;
        if (!isDone())
            return false;

        ResultType result = task_.get();
        return validator(result);
    } catch (...) {
        return false;
    }
}

template <typename ResultType>
void AsyncWorker<ResultType>::setCallback(
    std::function<void(ResultType)> callback) {
    if (!callback) {
        throw std::invalid_argument("Callback function cannot be null");
    }
    callback_ = std::move(callback);
}

template <typename ResultType>
void AsyncWorker<ResultType>::setTimeout(std::chrono::seconds timeout) {
    if (timeout < std::chrono::seconds(0)) {
        throw std::invalid_argument("Timeout cannot be negative");
    }
    timeout_ = timeout;
}

template <typename ResultType>
void AsyncWorker<ResultType>::waitForCompletion() {
    constexpr auto kSleepDuration =
        std::chrono::milliseconds(10);  // Reduced sleep time

    if (timeout_ != std::chrono::seconds(0)) {
        auto startTime = std::chrono::steady_clock::now();
        while (!isDone()) {
            std::this_thread::sleep_for(kSleepDuration);
            if (std::chrono::steady_clock::now() - startTime > timeout_) {
                cancel();
                THROW_TIMEOUT_EXCEPTION("Task execution timed out");
            }
        }
    } else {
        while (!isDone()) {
            std::this_thread::sleep_for(kSleepDuration);
        }
    }

    if (callback_ && isDone()) {
        try {
            callback_(getResult());
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Callback execution failed: ") + e.what());
        }
    }
}

template <typename ResultType>
template <typename Func, typename... Args>
    requires InvocableWithArgs<Func, Args...> &&
             std::is_same_v<std::invoke_result_t<Func, Args...>, ResultType>
[[nodiscard]] auto AsyncWorkerManager<ResultType>::createWorker(Func&& func,
                                                                Args&&... args)
    -> std::shared_ptr<AsyncWorker<ResultType>> {
    auto worker = std::make_shared<AsyncWorker<ResultType>>();

    try {
        worker->startAsync(std::forward<Func>(func),
                           std::forward<Args>(args)...);

#ifdef ATOM_USE_BOOST_LOCKFREE
        // For lockfree implementation, there's no need to acquire a mutex lock
        if (!workers_.push(worker)) {
            // If push fails (queue full), we need to handle it properly
            for (int retry = 0; retry < 5; ++retry) {
                std::this_thread::yield();
                if (workers_.push(worker)) {
                    return worker;
                }
                // Backoff on contention
                if (retry > 0) {
                    std::this_thread::sleep_for(
                        std::chrono::microseconds(1 << retry));
                }
            }
            throw std::runtime_error("Failed to add worker: queue is full");
        }
#else
        std::lock_guard<std::mutex> lock(mutex_);
        workers_.push_back(worker);
#endif
        return worker;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to create worker: ") +
                                 e.what());
    }
}

template <typename ResultType>
void AsyncWorkerManager<ResultType>::cancelAll() noexcept {
    try {
#ifdef ATOM_USE_BOOST_LOCKFREE
        workers_.forEach([](const auto& worker) {
            if (worker)
                worker->cancel();
        });
#else
        std::lock_guard<std::mutex> lock(mutex_);

        // Use parallel algorithm if there are many workers
        if (workers_.size() > 10) {
            // C++17 parallel execution policy
            std::for_each(workers_.begin(), workers_.end(), [](auto& worker) {
                if (worker)
                    worker->cancel();
            });
        } else {
            for (auto& worker : workers_) {
                if (worker)
                    worker->cancel();
            }
        }
#endif
    } catch (...) {
        // Ensure noexcept guarantee
    }
}

template <typename ResultType>
[[nodiscard]] auto AsyncWorkerManager<ResultType>::allDone() const noexcept
    -> bool {
#ifdef ATOM_USE_BOOST_LOCKFREE
    return const_cast<WorkerContainer<ResultType>&>(workers_).allOf(
        [](const auto& worker) { return worker && worker->isDone(); });
#else
    std::lock_guard<std::mutex> lock(mutex_);

    return std::all_of(
        workers_.begin(), workers_.end(),
        [](const auto& worker) { return worker && worker->isDone(); });
#endif
}

template <typename ResultType>
void AsyncWorkerManager<ResultType>::waitForAll(
    std::chrono::milliseconds timeout) {
    std::vector<std::jthread> waitThreads;

#ifdef ATOM_USE_BOOST_LOCKFREE
    // Create a copy to avoid race conditions
    auto workersCopy = workers_.retrieveAll();

    for (auto& worker : workersCopy) {
        if (!worker)
            continue;
        waitThreads.emplace_back(
            [worker, timeout]() { worker->waitForCompletion(); });

        // Add the worker back to the container
        workers_.push(worker);
    }
#else
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Create a copy to avoid race conditions
        auto workersCopy = workers_;

        for (auto& worker : workersCopy) {
            if (!worker)
                continue;
            waitThreads.emplace_back(
                [worker, timeout]() { worker->waitForCompletion(); });
        }
    }
#endif

    for (auto& thread : waitThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

template <typename ResultType>
[[nodiscard]] auto AsyncWorkerManager<ResultType>::isDone(
    std::shared_ptr<AsyncWorker<ResultType>> worker) const -> bool {
    if (!worker) {
        throw std::invalid_argument("Worker cannot be null");
    }
    return worker->isDone();
}

template <typename ResultType>
void AsyncWorkerManager<ResultType>::cancel(
    std::shared_ptr<AsyncWorker<ResultType>> worker) {
    if (!worker) {
        throw std::invalid_argument("Worker cannot be null");
    }
    worker->cancel();
}

template <typename ResultType>
[[nodiscard]] auto AsyncWorkerManager<ResultType>::size() const noexcept
    -> size_t {
#ifdef ATOM_USE_BOOST_LOCKFREE
    return workers_.size();
#else
    std::lock_guard<std::mutex> lock(mutex_);
    return workers_.size();
#endif
}

template <typename ResultType>
size_t AsyncWorkerManager<ResultType>::pruneCompletedWorkers() noexcept {
    try {
#ifdef ATOM_USE_BOOST_LOCKFREE
        return workers_.removeIf(
            [](const auto& worker) { return worker && worker->isDone(); });
#else
        std::lock_guard<std::mutex> lock(mutex_);
        auto initialSize = workers_.size();

        workers_.erase(std::remove_if(workers_.begin(), workers_.end(),
                                      [](const auto& worker) {
                                          return worker && worker->isDone();
                                      }),
                       workers_.end());

        return initialSize - workers_.size();
#endif
    } catch (...) {
        // Ensure noexcept guarantee
        return 0;
    }
}
}  // namespace atom::async
#endif