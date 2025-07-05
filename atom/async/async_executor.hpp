/*
 * async_executor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-24

Description: Advanced async task executor with thread pooling

**************************************************/

#ifndef ATOM_ASYNC_ASYNC_EXECUTOR_HPP
#define ATOM_ASYNC_ASYNC_EXECUTOR_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <coroutine>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <semaphore>
#include <source_location>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// Platform-specific optimizations
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define ATOM_PLATFORM_WINDOWS 1
#define WIN32_LEAN_AND_MEAN
#elif defined(__APPLE__)
#include <dispatch/dispatch.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#define ATOM_PLATFORM_MACOS 1
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>
#define ATOM_PLATFORM_LINUX 1
#endif

// Add compiler-specific optimizations
#if defined(__GNUC__) || defined(__clang__)
#define ATOM_LIKELY(x) __builtin_expect(!!(x), 1)
#define ATOM_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define ATOM_FORCE_INLINE __attribute__((always_inline)) inline
#define ATOM_NO_INLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define ATOM_LIKELY(x) (x)
#define ATOM_UNLIKELY(x) (x)
#define ATOM_FORCE_INLINE __forceinline
#define ATOM_NO_INLINE __declspec(noinline)
#else
#define ATOM_LIKELY(x) (x)
#define ATOM_UNLIKELY(x) (x)
#define ATOM_FORCE_INLINE inline
#define ATOM_NO_INLINE
#endif

// Cache line size definition - to avoid false sharing
#ifndef ATOM_CACHE_LINE_SIZE
#if defined(ATOM_PLATFORM_WINDOWS)
#define ATOM_CACHE_LINE_SIZE 64
#elif defined(ATOM_PLATFORM_MACOS)
#define ATOM_CACHE_LINE_SIZE 128
#else
#define ATOM_CACHE_LINE_SIZE 64
#endif
#endif

// Macro for aligning to cache line
#define ATOM_CACHELINE_ALIGN alignas(ATOM_CACHE_LINE_SIZE)

namespace atom::async {

// Forward declaration
class AsyncExecutor;

// Enhanced C++20 exception class with source location information
class ExecutorException : public std::runtime_error {
public:
    explicit ExecutorException(
        const std::string& msg,
        const std::source_location& loc = std::source_location::current())
        : std::runtime_error(msg + " at " + loc.file_name() + ":" +
                             std::to_string(loc.line()) + " in " +
                             loc.function_name()) {}
};

// Enhanced task exception handling mechanism
class TaskException : public ExecutorException {
public:
    explicit TaskException(
        const std::string& msg,
        const std::source_location& loc = std::source_location::current())
        : ExecutorException(msg, loc) {}
};

// C++20 coroutine task type, including continuation and error handling
template <typename R>
class Task;

// Task<void> specialization for coroutines
template <>
class Task<void> {
public:
    struct promise_type {
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void unhandled_exception() { exception_ = std::current_exception(); }
        void return_void() {}

        Task<void> get_return_object() {
            return Task<void>{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::exception_ptr exception_{};
    };

    using handle_type = std::coroutine_handle<promise_type>;

    Task(handle_type h) : handle_(h) {}
    ~Task() {
        if (handle_ && handle_.done()) {
            handle_.destroy();
        }
    }

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

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    bool is_ready() const noexcept { return handle_.done(); }

    void get() {
        handle_.resume();
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
    }

    struct Awaiter {
        handle_type handle;
        bool await_ready() const noexcept { return handle.done(); }
        void await_suspend(std::coroutine_handle<> h) noexcept { h.resume(); }
        void await_resume() {
            if (handle.promise().exception_) {
                std::rethrow_exception(handle.promise().exception_);
            }
        }
    };

    auto operator co_await() noexcept { return Awaiter{handle_}; }

private:
    handle_type handle_{};
    std::exception_ptr exception_{};
};

// Generic type implementation
template <typename R>
class Task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void unhandled_exception() { exception_ = std::current_exception(); }

        template <typename T>
            requires std::convertible_to<T, R>
        void return_value(T&& value) {
            result_ = std::forward<T>(value);
        }

        Task get_return_object() {
            return Task{handle_type::from_promise(*this)};
        }

        R result_{};
        std::exception_ptr exception_{};
    };

    Task(handle_type h) : handle_(h) {}
    ~Task() {
        if (handle_ && handle_.done()) {
            handle_.destroy();
        }
    }

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

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    bool is_ready() const noexcept { return handle_.done(); }

    R get_result() {
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        return std::move(handle_.promise().result_);
    }

    // Coroutine awaiter support
    struct Awaiter {
        handle_type handle;

        bool await_ready() const noexcept { return handle.done(); }

        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<> h) noexcept {
            // Store continuation
            continuation = h;
            return handle;
        }

        R await_resume() {
            if (handle.promise().exception_) {
                std::rethrow_exception(handle.promise().exception_);
            }
            return std::move(handle.promise().result_);
        }

        std::coroutine_handle<> continuation = nullptr;
    };

    Awaiter operator co_await() noexcept { return Awaiter{handle_}; }

private:
    handle_type handle_{};
};

/**
 * @brief Asynchronous executor - high-performance thread pool implementation
 *
 * Implements efficient task scheduling and execution, supports task priorities,
 * coroutines, and future/promise.
 */
class AsyncExecutor {
public:
    // Task priority
    enum class Priority { Low = 0, Normal = 50, High = 100, Critical = 200 };

    // Thread pool configuration options
    struct Configuration {
        size_t minThreads = 4;            // Minimum number of threads
        size_t maxThreads = 16;           // Maximum number of threads
        size_t queueSizePerThread = 128;  // Queue size per thread
        std::chrono::milliseconds threadIdleTimeout =
            std::chrono::seconds(30);  // Idle thread timeout
        bool setPriority = false;      // Whether to set thread priority
        int threadPriority = 0;        // Thread priority, platform-dependent
        bool pinThreads = false;       // Whether to pin threads to CPU cores
        bool useWorkStealing =
            true;  // Whether to enable work-stealing algorithm
        std::chrono::milliseconds statInterval =
            std::chrono::seconds(10);  // Statistics collection interval
    };

    /**
     * @brief Creates an asynchronous executor with the specified configuration
     * @param config Thread pool configuration
     */
    explicit AsyncExecutor(Configuration config);

    /**
     * @brief Disable copy constructor
     */
    AsyncExecutor(const AsyncExecutor&) = delete;
    AsyncExecutor& operator=(const AsyncExecutor&) = delete;

    /**
     * @brief Support move constructor
     */
    AsyncExecutor(AsyncExecutor&& other) noexcept;
    AsyncExecutor& operator=(AsyncExecutor&& other) noexcept;

    /**
     * @brief Destructor - stops all threads
     */
    ~AsyncExecutor();

    /**
     * @brief Starts the thread pool
     */
    void start();

    /**
     * @brief Stops the thread pool
     */
    void stop();

    /**
     * @brief Checks if the thread pool is running
     */
    [[nodiscard]] bool isRunning() const noexcept {
        return m_isRunning.load(std::memory_order_acquire);
    }

    /**
     * @brief Gets the number of active threads
     */
    [[nodiscard]] size_t getActiveThreadCount() const noexcept {
        return m_activeThreads.load(std::memory_order_relaxed);
    }

    /**
     * @brief Gets the current number of pending tasks
     */
    [[nodiscard]] size_t getPendingTaskCount() const noexcept {
        return m_pendingTasks.load(std::memory_order_relaxed);
    }

    /**
     * @brief Gets the number of completed tasks
     */
    [[nodiscard]] size_t getCompletedTaskCount() const noexcept {
        return m_completedTasks.load(std::memory_order_relaxed);
    }

    /**
     * @brief Executes any callable object in the background, void return
     * version
     *
     * @param func Callable object
     * @param priority Task priority
     */
    template <typename Func>
        requires std::invocable<Func> &&
                 std::same_as<void, std::invoke_result_t<Func>>
    void execute(Func&& func, Priority priority = Priority::Normal) {
        if (!isRunning()) {
            throw ExecutorException("Executor is not running");
        }

        enqueueTask(createWrappedTask(std::forward<Func>(func)),
                    static_cast<int>(priority));
    }

    /**
     * @brief Executes any callable object in the background, version with
     * return value, using std::future
     *
     * @param func Callable object
     * @param priority Task priority
     * @return std::future<ResultT> Asynchronous result
     */
    template <typename Func>
        requires std::invocable<Func> &&
                 (!std::same_as<void, std::invoke_result_t<Func>>)
    auto execute(Func&& func, Priority priority = Priority::Normal)
        -> std::future<std::invoke_result_t<Func>> {
        if (!isRunning()) {
            throw ExecutorException("Executor is not running");
        }

        using ResultT = std::invoke_result_t<Func>;
        auto promise = std::make_shared<std::promise<ResultT>>();
        auto future = promise->get_future();

        auto wrappedTask = [func = std::forward<Func>(func),
                            promise = std::move(promise)]() mutable {
            try {
                if constexpr (std::is_same_v<ResultT, void>) {
                    func();
                    promise->set_value();
                } else {
                    promise->set_value(func());
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        };

        enqueueTask(std::move(wrappedTask), static_cast<int>(priority));

        return future;
    }

    /**
     * @brief Executes an asynchronous task using C++20 coroutines
     *
     * @param func Callable object
     * @param priority Task priority
     * @return Task<ResultT> Coroutine task object
     */
    template <typename Func>
        requires std::invocable<Func>
    auto executeAsTask(Func&& func, Priority priority = Priority::Normal) {
        using ResultT = std::invoke_result_t<Func>;
        using TaskType = Task<ResultT>;  // Fixed: Added semicolon

        return [this, func = std::forward<Func>(func), priority]() -> TaskType {
            struct Awaitable {
                std::future<ResultT> future;
                bool await_ready() const noexcept { return false; }
                void await_suspend(std::coroutine_handle<> h) noexcept {}
                ResultT await_resume() { return future.get(); }
            };

            if constexpr (std::is_same_v<ResultT, void>) {
                co_await Awaitable{this->execute(func, priority)};
                co_return;
            } else {
                co_return co_await Awaitable{this->execute(func, priority)};
            }
        }();
    }

    /**
     * @brief Submits a task to the global thread pool instance
     *
     * @param func Callable object
     * @param priority Task priority
     * @return future of the task result
     */
    template <typename Func>
    static auto submit(Func&& func, Priority priority = Priority::Normal) {
        return getInstance().execute(std::forward<Func>(func), priority);
    }

    /**
     * @brief Gets a reference to the global thread pool instance
     * @return AsyncExecutor& Reference to the global thread pool
     */
    static AsyncExecutor& getInstance() {
        static AsyncExecutor instance{Configuration{}};
        return instance;
    }

private:
    // Thread pool configuration
    Configuration m_config;

    // Atomic state variables
    ATOM_CACHELINE_ALIGN std::atomic<bool> m_isRunning{false};
    ATOM_CACHELINE_ALIGN std::atomic<size_t> m_activeThreads{0};
    ATOM_CACHELINE_ALIGN std::atomic<size_t> m_pendingTasks{0};
    ATOM_CACHELINE_ALIGN std::atomic<size_t> m_completedTasks{0};

    // Task counting semaphore - C++20 feature
    std::counting_semaphore<> m_taskSemaphore{0};

    // Task type
    struct TaskItem {  // Renamed from Task to avoid conflict with class Task
        std::function<void()> func;
        int priority;

        bool operator<(const TaskItem& other) const {
            // Higher priority tasks are sorted earlier in the queue
            return priority < other.priority;
        }
    };

    // Task queue - priority queue
    std::mutex m_queueMutex;
    std::priority_queue<TaskItem> m_taskQueue;
    std::condition_variable m_condition;

    // Worker threads
    std::vector<std::jthread> m_threads;
    // 保存每个线程的 native_handle
    std::vector<std::thread::native_handle_type> m_threadHandles;

    // Statistics thread
    std::jthread m_statsThread;

    // Using work-stealing queue optimization
    struct WorkStealingQueue {
        std::mutex mutex;
        std::deque<TaskItem> tasks;
    };
    std::vector<std::unique_ptr<WorkStealingQueue>> m_perThreadQueues;

    /**
     * @brief Thread worker loop
     * @param threadId Thread ID
     * @param stoken Stop token
     */
    void workerLoop(size_t threadId, std::stop_token stoken);

    /**
     * @brief Sets thread affinity
     * @param threadId Thread ID
     */
    void setThreadAffinity(size_t threadId);

    /**
     * @brief Sets thread priority
     * @param handle Native handle of the thread
     */
    void setThreadPriority(std::thread::native_handle_type handle);

    /**
     * @brief Gets a task from the queue
     * @param threadId Current thread ID
     * @return std::optional<TaskItem> Optional task
     */
    std::optional<TaskItem> dequeueTask(size_t threadId);

    /**
     * @brief Tries to steal a task from other threads
     * @param currentId Current thread ID
     * @return std::optional<TaskItem> Optional task
     */
    std::optional<TaskItem> stealTask(size_t currentId);

    /**
     * @brief Adds a task to the queue
     * @param task Task function
     * @param priority Priority
     */
    void enqueueTask(std::function<void()> task, int priority);

    /**
     * @brief Wraps a task to add exception handling and performance statistics
     * @param func Original function
     * @return std::function<void()> Wrapped task
     */
    template <typename Func>
    auto createWrappedTask(Func&& func) {
        return [this, func = std::forward<Func>(func)]() {
            // Increment active thread count
            m_activeThreads.fetch_add(1, std::memory_order_relaxed);

            // Capture task start time - for performance monitoring
            auto startTime = std::chrono::high_resolution_clock::now();

            try {
                // Execute the actual task
                func();

                // Update completed task count
                m_completedTasks.fetch_add(1, std::memory_order_relaxed);
            } catch (...) {
                // Handle task exception - may need logging in a real
                // application
                m_completedTasks.fetch_add(1, std::memory_order_relaxed);

                // Rethrow exception or log
                // throw TaskException("Task execution failed with exception");
            }

            // Calculate task execution time
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    endTime - startTime);

            // In a real application, task execution time can be logged here for
            // performance analysis

            // Decrement active thread count
            m_activeThreads.fetch_sub(1, std::memory_order_relaxed);
        };
    }

    /**
     * @brief Statistics collection thread
     * @param stoken Stop token
     */
    void statsLoop(std::stop_token stoken);
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_ASYNC_EXECUTOR_HPP
