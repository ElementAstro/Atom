/*
 * thread_wrapper.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-13

Description: A simple wrapper of std::jthread

**************************************************/

#ifndef ATOM_ASYNC_THREAD_WRAPPER_HPP
#define ATOM_ASYNC_THREAD_WRAPPER_HPP

#include <algorithm>  // For std::min, std::max
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
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>  // Used by ThreadPool and parallel_for_each

#include "atom/type/noncopyable.hpp"

// Platform-specific includes
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#include <sched.h>  // For sched_param, SCHED_RR etc. in ThreadPool::setThreadPriority
#endif

namespace atom::async {

/**
 * @brief Exception class for thread-related errors.
 */
class ThreadException : public std::runtime_error {
public:
    /**
     * @brief Constructor to create a thread exception with source location
     * information.
     * @param message Error message.
     * @param loc Source code location (defaults to current location).
     */
    explicit ThreadException(
        const std::string& message,
        const std::source_location& loc = std::source_location::current())
        : std::runtime_error(formatMessage(message, loc)) {}

private:
    /**
     * @brief Formats the error message to include source code location.
     * @param message Original error message.
     * @param loc Source code location.
     * @return Formatted error message string.
     */
    static std::string formatMessage(const std::string& message,
                                     const std::source_location& loc) {
        std::stringstream ss;
        ss << message << " (at " << loc.file_name() << ":" << loc.line()
           << " in " << loc.function_name() << ")";
        return ss.str();
    }
};

// Concept for thread callable objects
template <typename Callable, typename... Args>
concept ThreadCallable = requires(Callable c, Args... args) {
    { c(args...) };  // Can be called with args
};

// Concept for thread callables that accept stop tokens
template <typename Callable, typename... Args>
concept StopTokenCallable =
    requires(Callable c, std::stop_token st, Args... args) {
        { c(st, args...) };  // Can be called with a stop token and args
    };

// Concept for any thread-poolable function
template <typename F>
concept PoolableFunction = std::is_invocable_v<std::decay_t<F>>;

/**
 * @brief A wrapper class for managing a C++20 jthread with enhanced
 * functionality.
 *
 * This class provides a convenient interface for managing a C++20 jthread,
 * allowing for starting, stopping, and joining threads easily.
 */
class Thread : public NonCopyable {
public:
    /**
     * @brief Default constructor.
     */
    Thread() noexcept = default;

    /**
     * @brief Constructor that immediately starts a thread with the given
     * function.
     *
     * @tparam Callable The type of the callable object.
     * @tparam Args The types of the function arguments.
     * @param func The callable to execute in the thread.
     * @param args The arguments to pass to the callable.
     */
    template <typename Callable, typename... Args>
        requires ThreadCallable<Callable, Args...>
    explicit Thread(Callable&& func, Args&&... args) {
        start(std::forward<Callable>(func), std::forward<Args>(args)...);
    }

    /**
     * @brief Starts a new thread with the specified callable object and
     * arguments.
     *
     * If the callable object is invocable with a std::stop_token and the
     * provided arguments, it will be invoked with a std::stop_token as the
     * first argument. Otherwise, it will be invoked with the provided
     * arguments.
     *
     * @tparam Callable The type of the callable object.
     * @tparam Args The types of the arguments.
     * @param func The callable object to execute in the new thread.
     * @param args The arguments to pass to the callable object.
     * @throws ThreadException if the thread cannot be started.
     */
    template <typename Callable, typename... Args>
        requires ThreadCallable<Callable, Args...>
    void start(Callable&& func, Args&&... args) {
        try {
            // Clean up any existing thread
            if (thread_.joinable()) {
                try {
                    thread_.request_stop();
                    thread_.join();
                } catch (...) {
                    // Ignore exceptions during cleanup
                }
            }

            // Create a shared state to track exceptions
            auto exception_ptr = std::make_shared<std::exception_ptr>(nullptr);
            auto thread_started = std::make_shared<std::promise<void>>();
            auto thread_started_future = thread_started->get_future();

            thread_name_ =
                generateThreadName();  // Generate name for OS debugging

            thread_ = std::jthread(
                [func = std::forward<Callable>(func),
                 ... args = std::forward<Args>(args), exception_ptr,
                 thread_started = std::move(thread_started),
                 thread_name = thread_name_](
                    std::stop_token
                        current_jthread_stop_token) mutable {  // Accept
                                                               // jthread's
                                                               // stop_token
                    try {
                        // Set thread name for debugging if supported
                        setCurrentThreadName(thread_name);

                        // Signal that the thread has started
                        thread_started->set_value();

                        if constexpr (StopTokenCallable<Callable, Args...>) {
                            // Pass the jthread's stop token
                            func(current_jthread_stop_token,
                                 std::move(args)...);
                        } else {
                            func(std::move(args)...);
                        }
                    } catch (...) {
                        *exception_ptr = std::current_exception();
                    }
                });

            // Wait for thread to start or time out
            using namespace std::chrono_literals;
            if (thread_started_future.wait_for(500ms) ==
                std::future_status::timeout) {
                thread_.request_stop();
                throw ThreadException(
                    "Thread failed to start within timeout period");
            }

            // Check if an exception was thrown during thread startup
            if (*exception_ptr) {
                thread_.request_stop();
                std::rethrow_exception(*exception_ptr);
            }
        } catch (const std::exception& e) {
            throw ThreadException(std::string("Failed to start thread: ") +
                                  e.what());
        }
    }

    /**
     * @brief Starts a thread with a function that returns a value.
     *
     * @tparam R Return type of the function.
     * @tparam Callable Type of the callable object.
     * @tparam Args Types of the arguments to the callable.
     * @param func Callable object.
     * @param args Arguments to pass to the callable.
     * @return std::future<R> A future that will contain the result.
     * @throws ThreadException if the thread cannot be started.
     */
    template <typename R, typename Callable, typename... Args>
        requires ThreadCallable<Callable, Args...>
    [[nodiscard]] auto startWithResult(Callable&& func, Args&&... args)
        -> std::future<R> {
        auto task = std::make_shared<std::packaged_task<R()>>(
            [func = std::forward<Callable>(func),
             ... args = std::forward<Args>(args)]() mutable -> R {
                return func(std::move(args)...);
            });

        auto future = task->get_future();

        try {
            start([task]() { (*task)(); });
            return future;
        } catch (const std::exception& e) {
            throw ThreadException(
                std::string("Failed to start thread with result: ") + e.what());
        }
    }

    /**
     * @brief Sets a timeout for thread execution, automatically stopping the
     * thread after the specified duration.
     * @tparam Rep Duration representation type.
     * @tparam Period Duration period type.
     * @param timeout Timeout duration.
     */
    template <typename Rep, typename Period>
    void setTimeout(const std::chrono::duration<Rep, Period>& timeout) {
        if (!running()) {
            return;
        }

        // Create a timeout monitoring thread
        std::jthread timeout_thread(
            [this, timeout](std::stop_token stop_token) {
                // Wait for the specified duration or until canceled
                // Use a condition variable to allow quicker stop response if
                // needed, but for simplicity, sleep_for is used here. A more
                // robust implementation might use cv.wait_for with stop_token.
                std::mutex m;
                std::condition_variable_any cv;
                std::unique_lock lock(m);
                if (cv.wait_for(lock, timeout, [&stop_token] {
                        return stop_token.stop_requested();
                    })) {
                    return;  // Stopped before timeout
                }

                // If the monitoring thread was not canceled and the main thread
                // is still running, request stop
                if (!stop_token.stop_requested() && this->running()) {
                    this->requestStop();
                }
            });

        // Store the timeout thread
        timeout_thread_ = std::move(timeout_thread);
    }

    /**
     * @brief Executes a task periodically.
     *
     * @tparam Callable Callable object type.
     * @tparam Rep Period duration representation type.
     * @tparam Period Period duration unit type.
     * @param func Function to execute.
     * @param interval Execution interval.
     */
    template <typename Callable, typename Rep, typename Period>
        requires std::invocable<Callable>
    void startPeriodic(Callable&& func,
                       const std::chrono::duration<Rep, Period>& interval) {
        start([func = std::forward<Callable>(func),
               interval](std::stop_token stop_token) mutable {
            while (!stop_token.stop_requested()) {
                func();

                // Use a condition variable to allow quicker stop response
                std::mutex m;
                std::condition_variable_any cv;
                auto pred = [&stop_token] {
                    return stop_token.stop_requested();
                };
                std::unique_lock lock(m);
                if (cv.wait_for(lock, interval, pred)) {
                    break;  // Stop requested
                }
            }
        });
    }

    /**
     * @brief Executes a task after a delay.
     *
     * @tparam Callable Callable object type.
     * @tparam Rep Delay duration representation type.
     * @tparam Period Delay duration unit type.
     * @tparam Args Function argument types.
     * @param delay Delay duration.
     * @param func Function to execute.
     * @param args Function arguments.
     */
    template <typename Callable, typename Rep, typename Period,
              typename... Args>
        requires ThreadCallable<Callable, Args...>
    void startDelayed(const std::chrono::duration<Rep, Period>& delay,
                      Callable&& func, Args&&... args) {
        start([delay, func = std::forward<Callable>(func),
               ... args = std::forward<Args>(args)](
                  std::stop_token stop_token) mutable {
            // Use a condition variable to allow quicker stop response
            {
                std::mutex m;
                std::condition_variable_any cv;
                auto pred = [&stop_token] {
                    return stop_token.stop_requested();
                };
                std::unique_lock lock(m);
                if (cv.wait_for(lock, delay, pred)) {
                    return;  // If stopped, return directly
                }
            }

            // If not stopped, execute the task
            if (!stop_token.stop_requested()) {
                if constexpr (StopTokenCallable<Callable, Args...>) {
                    func(stop_token, std::move(args)...);
                } else {
                    func(std::move(args)...);
                }
            }
        });
    }

    /**
     * @brief Sets the thread name for debugging purposes.
     * @param name Thread name.
     */
    void setThreadName(std::string name) {
        thread_name_ = std::move(name);
        // If the thread is already running, try to set its name
        if (running()) {
            try {
                setThreadName(thread_.native_handle(), thread_name_);
            } catch (...) {
                // Ignore errors in setting thread name
            }
        }
    }

    /**
     * @brief Requests the thread to stop execution.
     */
    void requestStop() noexcept {
        try {
            if (thread_.joinable()) {
                thread_.request_stop();
            }
            // Also stop the timeout thread (if any)
            if (timeout_thread_.joinable()) {
                timeout_thread_.request_stop();
            }
        } catch (...) {
            // Ignore any exceptions during stop request
        }
    }

    /**
     * @brief Waits for the thread to finish execution.
     *
     * @throws ThreadException if joining the thread throws an exception.
     */
    void join() {
        try {
            if (thread_.joinable()) {
                thread_.join();
            }
            // Also wait for the timeout thread (if any)
            if (timeout_thread_.joinable()) {
                timeout_thread_.join();
            }
        } catch (const std::exception& e) {
            throw ThreadException(std::string("Failed to join thread: ") +
                                  e.what());
        }
    }

    /**
     * @brief Tries to join the thread with a timeout.
     *
     * @tparam Rep Clock tick representation.
     * @tparam Period Clock tick period.
     * @param timeout_duration The maximum time to wait.
     * @return true if joined successfully, false if timed out.
     */
    template <typename Rep, typename Period>
    [[nodiscard]] auto tryJoinFor(
        const std::chrono::duration<Rep, Period>& timeout_duration) noexcept
        -> bool {
        if (!running()) {
            return true;  // Thread is not running, so join succeeded
        }

        // Implement spin-based timeout wait, as jthread lacks join_for
        const auto start_time = std::chrono::steady_clock::now();

        // Use a more efficient adaptive sleep strategy
        const auto sleep_time_base = std::chrono::microseconds(100);
        auto sleep_time = sleep_time_base;
        const auto max_sleep_time = std::chrono::milliseconds(10);

        while (running()) {
            std::this_thread::sleep_for(sleep_time);

            // Adaptively increase sleep time, but not beyond max
            sleep_time =
                std::min(sleep_time * 2,
                         std::chrono::duration_cast<std::chrono::microseconds>(
                             max_sleep_time));

            // Check for timeout
            if (std::chrono::steady_clock::now() - start_time >
                timeout_duration) {
                return false;  // Timed out
            }
        }

        // Thread has ended, ensure resource cleanup
        join();  // Call regular join to clean up
        return true;
    }

    /**
     * @brief Checks if the thread is currently running.
     * @return True if the thread is running, false otherwise.
     */
    [[nodiscard]] auto running() const noexcept -> bool {
        return thread_.joinable();
    }

    /**
     * @brief Swaps the content of this Thread object with another Thread
     * object.
     * @param other The Thread object to swap with.
     */
    void swap(Thread& other) noexcept {
        thread_.swap(other.thread_);
        timeout_thread_.swap(other.timeout_thread_);
        std::swap(thread_name_, other.thread_name_);
    }

    /**
     * @brief Gets the underlying std::jthread object.
     * @return Reference to the underlying std::jthread object.
     */
    [[nodiscard]] auto getThread() noexcept -> std::jthread& { return thread_; }

    /**
     * @brief Gets the underlying std::jthread object (const version).
     * @return Constant reference to the underlying std::jthread object.
     */
    [[nodiscard]] auto getThread() const noexcept -> const std::jthread& {
        return thread_;
    }

    /**
     * @brief Gets the ID of the thread.
     * @return The ID of the thread.
     */
    [[nodiscard]] auto getId() const noexcept -> std::thread::id {
        return thread_.get_id();
    }

    /**
     * @brief Gets the thread name.
     * @return The name of the thread.
     */
    [[nodiscard]] const std::string& getName() const noexcept {
        return thread_name_;
    }

    /**
     * @brief Gets the underlying std::stop_source object.
     * @return The underlying std::stop_source object.
     */
    [[nodiscard]] auto getStopSource() noexcept -> std::stop_source {
        return thread_.get_stop_source();
    }

    /**
     * @brief Gets the underlying std::stop_token object.
     * @return The underlying std::stop_token object.
     */
    [[nodiscard]] auto getStopToken() const noexcept -> std::stop_token {
        return thread_.get_stop_token();
    }

    /**
     * @brief Checks if the thread should stop.
     * @return True if the thread should stop, false otherwise.
     */
    [[nodiscard]] auto shouldStop() const noexcept -> bool {
        return thread_.get_stop_token().stop_requested();
    }

    /**
     * @brief Gets the number of hardware concurrency units available to the
     * system.
     * @return Number of system threads.
     */
    [[nodiscard]] static unsigned int getHardwareConcurrency() noexcept {
        return std::thread::hardware_concurrency();
    }

    /**
     * @brief Default destructor that automatically joins the thread if
     * joinable.
     */
    ~Thread() {
        try {
            // Request stop and wait for thread to finish
            if (thread_.joinable()) {
                thread_.request_stop();
                thread_.join();
            }

            // Also handle timeout thread
            if (timeout_thread_.joinable()) {
                timeout_thread_.request_stop();
                timeout_thread_.join();
            }
        } catch (...) {
            // Ignore exceptions in destructor
        }
    }

private:
    std::jthread thread_;          ///< Main thread object
    std::jthread timeout_thread_;  ///< Thread for timeout control
    std::string thread_name_;      ///< Thread name, for debugging

    /**
     * @brief Generates a unique thread name.
     * @return Generated thread name.
     */
    static std::string generateThreadName() {
        static std::atomic<unsigned int> counter{0};
        std::stringstream ss;
        ss << "Thread-" << counter++;
        return ss.str();
    }

    /**
     * @brief Sets the current thread name (platform-specific).
     * @param name Thread name.
     */
    static void setCurrentThreadName(const std::string& name) {
#if defined(_WIN32)
        // Set thread name on Windows (for debugging only)
        using SetThreadDescriptionFunc = HRESULT(WINAPI*)(HANDLE, PCWSTR);

        // Get function pointer
        static const auto setThreadDescriptionFunc =
            []() -> SetThreadDescriptionFunc {
            HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
            if (kernel32) {
                return reinterpret_cast<SetThreadDescriptionFunc>(
                    GetProcAddress(kernel32, "SetThreadDescription"));
            }
            return nullptr;
        }();

        if (setThreadDescriptionFunc) {
            // Convert to wide characters
            std::wstring wname(name.begin(), name.end());
            setThreadDescriptionFunc(GetCurrentThread(), wname.c_str());
        }
#elif defined(__linux__)
        // Set thread name on Linux
        pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
#elif defined(__APPLE__)
        // Set thread name on MacOS
        pthread_setname_np(name.substr(0, 63).c_str());
#endif
    }

    /**
     * @brief Sets the name of a specified thread handle (platform-specific).
     * @param handle Thread handle.
     * @param name Thread name.
     */
    static void setThreadName(std::thread::native_handle_type handle,
                              const std::string& name) {
#if defined(_WIN32)
        // Set thread name on Windows (for debugging only)
        using SetThreadDescriptionFunc = HRESULT(WINAPI*)(HANDLE, PCWSTR);

        // Get function pointer
        static const auto setThreadDescriptionFunc =
            []() -> SetThreadDescriptionFunc {
            HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
            if (kernel32) {
                return reinterpret_cast<SetThreadDescriptionFunc>(
                    GetProcAddress(kernel32, "SetThreadDescription"));
            }
            return nullptr;
        }();

        if (setThreadDescriptionFunc) {
            // Convert to wide characters
            std::wstring wname(name.begin(), name.end());
            // Assuming 'handle' (native_handle_type as unsigned long long) is a
            // Thread ID
            HANDLE hThread = OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE,
                                        static_cast<DWORD>(handle));
            if (hThread) {
                setThreadDescriptionFunc(hThread, wname.c_str());
                CloseHandle(hThread);
            }
        }
#elif defined(__linux__)
        // Set thread name on Linux
        // Note: handle is pthread_t here
        pthread_setname_np(handle, name.substr(0, 15).c_str());
#elif defined(__APPLE__)
        // Cannot set name for other threads on MacOS, ignore
        (void)handle;  // Suppress unused parameter warning
        (void)name;    // Suppress unused parameter warning
#endif
    }
};

/**
 * @brief Thread pool exception class.
 */
class ThreadPoolException : public ThreadException {
public:
    /**
     * @brief Constructor.
     * @param message Exception message.
     * @param loc Source code location.
     */
    explicit ThreadPoolException(
        const std::string& message,
        const std::source_location& loc = std::source_location::current())
        : ThreadException(std::string("ThreadPool error: ") + message, loc) {}
};

/**
 * @brief A simple C++20 coroutine task wrapper.
 *
 * Uses coroutines to implement an asynchronous programming model,
 * allowing non-blocking asynchronous execution.
 * @tparam T Coroutine return value type.
 */
template <typename T = void>
class Task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    /**
     * @brief Coroutine Promise type.
     */
    struct promise_type {
        /**
         * @brief Whether to suspend immediately when the coroutine starts.
         * @return Suspend object.
         */
        std::suspend_never initial_suspend() noexcept { return {}; }

        /**
         * @brief Whether to suspend when the coroutine ends.
         * @return Suspend object.
         */
        std::suspend_never final_suspend() noexcept { return {}; }

        /**
         * @brief Handles unhandled exceptions within the coroutine.
         */
        void unhandled_exception() noexcept {
            exception_ = std::current_exception();
            has_exception_ = true;
            if (completion_callback_) {
                completion_callback_();
            }
        }

        /**
         * @brief Sets the coroutine return value.
         * @tparam U Return value type.
         * @param value Return value.
         */
        template <typename U = T>
            requires(!std::is_void_v<T> && std::convertible_to<U, T>)
        void return_value(U&& value) {
            value_ = std::forward<U>(value);
            has_value_ = true;
            if (completion_callback_) {
                completion_callback_();
            }
        }

        /**
         * @brief Handles return for void-type coroutines.
         */
        void return_void()
            requires std::same_as<T, void>
        {
            has_value_ = true;  // For void, has_value_ indicates completion
                                // without exception
            if (completion_callback_) {
                completion_callback_();
            }
        }

        /**
         * @brief Gets the coroutine return object.
         * @return Task object.
         */
        Task get_return_object() {
            return Task(handle_type::from_promise(*this));
        }

        /**
         * @brief Sets the callback function for task completion.
         * @param callback Callback function.
         */
        void setCompletionCallback(std::function<void()> callback) {
            completion_callback_ = std::move(callback);
            // If task already completed, invoke callback immediately
            if (has_value_ || has_exception_) {
                completion_callback_();
            }
        }

        /**
         * @brief Gets the task status.
         * @return True if the task is completed.
         */
        [[nodiscard]] bool isCompleted() const noexcept {
            return has_value_ || has_exception_;
        }

        /**
         * @brief Gets the task result.
         * @return Task result.
         * @throws Rethrows the exception caught in the task if it failed.
         */
        decltype(auto) getResult() {
            if (has_exception_) {
                std::rethrow_exception(exception_);
            }

            if constexpr (std::is_void_v<T>) {
                return;  // No value to return for void
            } else {
                if (value_)
                    return std::move(
                        *value_);  // Check if optional contains value
                else
                    throw std::runtime_error(
                        "Task completed without a value (or value already "
                        "moved).");
            }
        }

        // Internal data
        std::function<void()> completion_callback_;
        std::exception_ptr exception_;
        std::atomic<bool> has_exception_{false};
        std::atomic<bool> has_value_{
            false};  // Indicates successful completion (with or without value)
        std::conditional_t<std::is_void_v<T>, std::monostate, std::optional<T>>
            value_;
    };

    /**
     * @brief Constructor.
     * @param h Coroutine handle.
     */
    explicit Task(handle_type h) : handle_(h) {}

    /**
     * @brief Move constructor.
     * @param other Other Task object.
     */
    Task(Task&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

    /**
     * @brief Move assignment operator.
     * @param other Other Task object.
     * @return Reference to this object.
     */
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {  // Protect against self-assignment
            if (handle_)
                handle_.destroy();  // Destroy existing handle if any
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    /**
     * @brief Destructor, destroys the coroutine handle.
     */
    ~Task() {
        if (handle_)
            handle_.destroy();
    }

    /**
     * @brief Checks if the task is completed.
     * @return True if the task is completed.
     */
    [[nodiscard]] bool isCompleted() const noexcept {
        return handle_ && handle_.promise().isCompleted();
    }

    /**
     * @brief Gets the task result.
     * @return Task result.
     * @throws Throws an exception if the task is not completed or failed.
     */
    decltype(auto) getResult() {
        if (!handle_) {
            throw std::runtime_error("Task has no valid coroutine handle");
        }

        if (!handle_.promise().isCompleted()) {
            // This is a design choice. Some might prefer to co_await or block.
            // For now, throwing if not completed.
            throw std::runtime_error("Task is not yet completed");
        }

        return handle_.promise().getResult();
    }

    /**
     * @brief Sets the callback function for task completion.
     * @param callback Callback function.
     */
    void setCompletionCallback(std::function<void()> callback) {
        if (handle_) {
            handle_.promise().setCompletionCallback(std::move(callback));
        }
    }

    /**
     * @brief Gets the coroutine handle.
     * @return Coroutine handle.
     */
    [[nodiscard]] handle_type getHandle() const noexcept { return handle_; }

private:
    handle_type handle_{nullptr};  ///< Coroutine handle, initialized to nullptr
};

/**
 * @brief Sleeps the current thread for a specified duration.
 *
 * @tparam Rep Duration representation type.
 * @tparam Period Duration period type.
 * @param duration Sleep duration.
 */
template <typename Rep, typename Period>
void sleep_for(const std::chrono::duration<Rep, Period>& duration) {
    std::this_thread::sleep_for(duration);
}

/**
 * @brief Sleeps the current thread until a specified time point.
 *
 * @tparam Clock Clock type.
 * @tparam Duration Duration type.
 * @param time_point Sleep deadline time point.
 */
template <typename Clock, typename Duration>
void sleep_until(const std::chrono::time_point<Clock, Duration>& time_point) {
    std::this_thread::sleep_until(time_point);
}

/**
 * @brief Gets the current thread ID.
 *
 * @return std::thread::id Thread ID.
 */
inline std::thread::id getCurrentThreadId() noexcept {
    return std::this_thread::get_id();
}

/**
 * @brief Yields CPU to allow other threads to run.
 */
inline void yield() noexcept { std::this_thread::yield(); }

/**
 * @brief Creates a task with a stop token (C++20 coroutine).
 *
 * @tparam F Function type.
 * @param f Function object.
 * @return Coroutine task.
 */
template <typename F>
auto makeTask(F&& f) -> Task<std::invoke_result_t<F>> {
    // This is a simplified makeTask. A real one might interact with an executor
    // or provide more suspension options.
    if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
        co_await std::suspend_never{};  // Execute immediately for this simple
                                        // version
        std::forward<F>(f)();
        co_return;
    } else {
        co_await std::suspend_never{};  // Execute immediately
        co_return std::forward<F>(f)();
    }
}

/**
 * @brief Creates a group of threads to execute a batch operation.
 *
 * @tparam InputIt Input iterator type.
 * @tparam Function Function type.
 * @param first Start iterator.
 * @param last End iterator.
 * @param function Function to execute.
 * @param num_threads Number of threads (default: hardware concurrency).
 */
template <typename InputIt, typename Function>
void parallel_for_each(
    InputIt first, InputIt last, Function function,
    unsigned int num_threads = std::thread::hardware_concurrency()) {
    if (first == last)
        return;
    if (num_threads == 0)
        num_threads = 1;  // Ensure at least one thread

    const auto length = std::distance(first, last);
    if (length == 0)
        return;

    // Calculate batch size per thread, ensuring all elements are covered
    const auto batch_size = (length + num_threads - 1) / num_threads;

    std::vector<std::jthread> threads;
    if (num_threads > 0) {  // Reserve only if num_threads is positive
        threads.reserve(num_threads);
    }

    auto current_it = first;
    for (unsigned int i = 0; i < num_threads && current_it != last; ++i) {
        auto batch_start = current_it;
        auto batch_end = batch_start;
        // Ensure std::distance result is compatible with std::min argument
        // types
        auto current_distance = std::distance(batch_start, last);
        std::advance(
            batch_end,
            std::min(static_cast<decltype(current_distance)>(batch_size),
                     current_distance));

        if (batch_start == batch_end)
            continue;

        threads.emplace_back([function, batch_start, batch_end]() {
            std::for_each(batch_start, batch_end, function);
        });
        current_it = batch_end;
    }

    // jthreads automatically join on destruction
}

/**
 * @brief Processes elements in a range in parallel using a specified execution
 * policy.
 *
 * @tparam ExecutionPolicy Execution policy type (can be number of threads or
 * standard execution policy).
 * @tparam InputIt Input iterator type.
 * @tparam Function Function type.
 * @param policy Execution policy.
 * @param first Start iterator.
 * @param last End iterator.
 * @param function Function to execute.
 */
template <typename ExecutionPolicy, typename InputIt, typename Function,
          typename = std::enable_if_t<
              !std::is_convertible_v<ExecutionPolicy, InputIt>>>
void parallel_for_each(ExecutionPolicy&& policy, InputIt first, InputIt last,
                       Function function) {
    unsigned int num_threads = std::thread::hardware_concurrency();

    if constexpr (std::is_integral_v<std::remove_cvref_t<ExecutionPolicy>>) {
        // If policy is a number, interpret as number of threads
        num_threads = static_cast<unsigned int>(policy);
        if (num_threads == 0)
            num_threads = std::thread::hardware_concurrency();  // Default if 0
    }
    // else if constexpr
    // (std::is_execution_policy_v<std::remove_cvref_t<ExecutionPolicy>>) {
    //     // Handle standard execution policies if needed, e.g.
    //     std::execution::par
    //     // For std::execution::par, typically num_threads would be
    //     hardware_concurrency()
    //     // This example focuses on the integer-as-num_threads case.
    // }

    parallel_for_each(first, last, std::forward<Function>(function),
                      num_threads);
}

}  // namespace atom::async

#endif  // ATOM_ASYNC_THREAD_WRAPPER_HPP
