/*
 * coroutine_task.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-24

Description: Unified C++20 coroutine Task type for async operations

**************************************************/

#ifndef ATOM_ASYNC_EXECUTION_COROUTINE_TASK_HPP
#define ATOM_ASYNC_EXECUTION_COROUTINE_TASK_HPP

#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace atom::async {

/**
 * @brief C++20 coroutine task class for asynchronous parallel computation
 *
 * @tparam T Task result type
 */
template <typename T>
class [[nodiscard]] Task {
public:
    /**
     * @brief Promise type for the coroutine task
     */
    struct promise_type {
        std::optional<T> result;
        std::exception_ptr exception;

        Task get_return_object() noexcept {
            return Task{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        template <typename U>
            requires std::convertible_to<U, T>
        void return_value(U&& value) noexcept(
            std::is_nothrow_constructible_v<T, U&&>) {
            result = std::forward<U>(value);
        }

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    /**
     * @brief Destructor - destroys the coroutine handle if done
     */
    ~Task() {
        if (handle_ && handle_.done()) {
            handle_.destroy();
        }
    }

    // Disable copy
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // Enable move
    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_ && handle_.done()) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Get the task result
     *
     * @return Result value
     * @throws Rethrows if the coroutine threw an exception
     */
    T get() {
        if (!handle_.done()) {
            handle_.resume();
        }

        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }

        if (!handle_.promise().result.has_value()) {
            throw std::runtime_error("Coroutine did not return a value");
        }

        return std::move(handle_.promise().result.value());
    }

    /**
     * @brief Check if the task is complete
     */
    [[nodiscard]] bool is_done() const noexcept {
        return handle_ && handle_.done();
    }

    /**
     * @brief Check if the task is ready (alias for is_done)
     */
    [[nodiscard]] bool is_ready() const noexcept { return is_done(); }

    /**
     * @brief Awaiter support for co_await
     */
    struct Awaiter {
        handle_type handle;

        bool await_ready() const noexcept { return handle.done(); }

        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<> h) noexcept {
            continuation = h;
            return handle;
        }

        T await_resume() {
            if (handle.promise().exception) {
                std::rethrow_exception(handle.promise().exception);
            }
            return std::move(handle.promise().result.value());
        }

        std::coroutine_handle<> continuation = nullptr;
    };

    Awaiter operator co_await() noexcept { return Awaiter{handle_}; }

private:
    explicit Task(handle_type h) noexcept : handle_(h) {}
    handle_type handle_{};
};

/**
 * @brief Void return type specialization for coroutine task
 */
template <>
class [[nodiscard]] Task<void> {
public:
    struct promise_type {
        std::exception_ptr exception;

        Task get_return_object() noexcept {
            return Task{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void() noexcept {}

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    ~Task() {
        if (handle_ && handle_.done()) {
            handle_.destroy();
        }
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_ && handle_.done()) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    void get() {
        if (!handle_.done()) {
            handle_.resume();
        }

        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
    }

    [[nodiscard]] bool is_done() const noexcept {
        return handle_ && handle_.done();
    }

    [[nodiscard]] bool is_ready() const noexcept { return is_done(); }

    struct Awaiter {
        handle_type handle;

        bool await_ready() const noexcept { return handle.done(); }

        void await_suspend(std::coroutine_handle<> h) noexcept { h.resume(); }

        void await_resume() {
            if (handle.promise().exception) {
                std::rethrow_exception(handle.promise().exception);
            }
        }
    };

    auto operator co_await() noexcept { return Awaiter{handle_}; }

private:
    explicit Task(handle_type h) noexcept : handle_(h) {}
    handle_type handle_{};
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_EXECUTION_COROUTINE_TASK_HPP
