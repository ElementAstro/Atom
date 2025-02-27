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

#include <chrono>
#include <concepts>
#include <coroutine>
// 移除未使用的头文件
// #include <functional>
#include <future>
// #include <mutex>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>

#include "atom/type/noncopyable.hpp"

namespace atom::async {

// Concept for thread callable objects
template <typename Callable, typename... Args>
concept ThreadCallable = requires(Callable c, Args... args) {
    { c(args...) };  // Can be called with args
};

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
     * @throws std::runtime_error if the thread cannot be started
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

            thread_ = std::jthread([func = std::forward<Callable>(func),
                                    ... args = std::forward<Args>(args),
                                    exception_ptr,
                                    thread_started =
                                        std::move(thread_started)]() mutable {
                try {
                    // Signal that the thread has started
                    thread_started->set_value();

                    if constexpr (std::is_invocable_v<Callable, std::stop_token,
                                                      Args...>) {
                        std::stop_token stop_token{};
                        func(stop_token, std::move(args)...);
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
                throw std::runtime_error(
                    "Thread failed to start within timeout period");
            }

            // Check if an exception was thrown during thread startup
            if (*exception_ptr) {
                thread_.request_stop();
                std::rethrow_exception(*exception_ptr);
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to start thread: ") +
                                     e.what());
        }
    }

    /**
     * @brief Starts a thread with a function that returns a value.
     *
     * @tparam R Return type of the function
     * @tparam Callable Type of the callable object
     * @tparam Args Types of the arguments to the callable
     * @param func Callable object
     * @param args Arguments to pass to the callable
     * @return std::future<R> A future that will contain the result
     */
    template <typename R, typename Callable, typename... Args>
        requires ThreadCallable<Callable, Args...>
    [[nodiscard]] auto startWithResult(Callable&& func,
                                       Args&&... args) -> std::future<R> {
        auto task = std::make_shared<std::packaged_task<R()>>(
            [func = std::forward<Callable>(func),
             ... args = std::forward<Args>(args)]() mutable -> R {
                return func(std::move(args)...);
            });

        auto future = task->get_future();

        try {
            start([task]() { (*task)(); });
            return future;
        } catch (const std::exception&) {
            throw;  // Re-throw the exception
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
        } catch (...) {
            // Ignore any exceptions during stop request
        }
    }

    /**
     * @brief Waits for the thread to finish execution.
     *
     * @throws std::runtime_error if joining the thread throws an exception
     */
    void join() {
        try {
            if (thread_.joinable()) {
                thread_.join();
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to join thread: ") +
                                     e.what());
        }
    }

    /**
     * @brief Tries to join the thread with a timeout
     *
     * @tparam Rep Clock tick representation
     * @tparam Period Clock tick period
     * @param timeout_duration The maximum time to wait
     * @return true if joined successfully, false if timed out
     */
    template <typename Rep, typename Period>
    [[nodiscard]] auto tryJoinFor(const std::chrono::duration<Rep, Period>&
                                      timeout_duration) noexcept -> bool {
        // Implement a polling-based join with timeout since jthread doesn't
        // provide join_for
        auto start = std::chrono::steady_clock::now();
        while (running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (std::chrono::steady_clock::now() - start > timeout_duration) {
                return false;
            }
        }
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
    void swap(Thread& other) noexcept { thread_.swap(other.thread_); }

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
     * @brief Default destructor that automatically joins the thread if
     * joinable.
     */
    ~Thread() {
        try {
            if (thread_.joinable()) {
                thread_.request_stop();
                thread_.join();
            }
        } catch (...) {
            // Ignore exceptions in destructor
        }
    }

private:
    std::jthread thread_;  ///< The underlying jthread object.
};

/**
 * @brief A simple C++20 coroutine task wrapper
 *
 * This allows launching asynchronous tasks using coroutines
 */
template <typename T = void>
class Task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() { exception_ = std::current_exception(); }

        template <typename U>
            requires std::convertible_to<U, T>
        void return_value(U&& value) {
            value_ = std::forward<U>(value);
        }

        void return_void()
            requires std::same_as<T, void>
        {}

        Task get_return_object() {
            return Task(handle_type::from_promise(*this));
        }

        std::exception_ptr exception_;
        std::conditional_t<std::same_as<T, void>, std::monostate,
                           std::optional<T>>
            value_;
    };

    Task(handle_type h) : handle_(h) {}
    Task(Task&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}
    Task& operator=(Task&& other) noexcept {
        handle_ = std::exchange(other.handle_, nullptr);
        return *this;
    }

    ~Task() {
        if (handle_)
            handle_.destroy();
    }

private:
    handle_type handle_;
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_THREAD_WRAPPER_HPP
