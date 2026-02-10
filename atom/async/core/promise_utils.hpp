/*
 * promise_utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-01-01

Description: Utility functions for Promise operations

**************************************************/

#ifndef ATOM_ASYNC_CORE_PROMISE_UTILS_HPP
#define ATOM_ASYNC_CORE_PROMISE_UTILS_HPP

#include <chrono>
#include <functional>
#include <future>
#include <thread>
#include <type_traits>
#include <vector>

#include "promise.hpp"

namespace atom::async {

/**
 * @brief Create a resolved promise with a value
 * @tparam T The value type
 * @param value The value to resolve with
 * @return A promise that is already resolved
 */
template <typename T>
auto makeResolvedPromise(T&& value) -> Promise<std::decay_t<T>> {
    Promise<std::decay_t<T>> promise;
    promise.setValue(std::forward<T>(value));
    return promise;
}

/**
 * @brief Create a rejected promise with an exception
 * @tparam T The value type
 * @param ex The exception to reject with
 * @return A promise that is already rejected
 */
template <typename T>
auto makeRejectedPromise(std::exception_ptr ex) -> Promise<T> {
    Promise<T> promise;
    promise.setException(ex);
    return promise;
}

/**
 * @brief Wait for any promise to complete
 * @tparam T The value type
 * @param promises Vector of promises to wait for
 * @return A promise that resolves when any input promise resolves
 */
template <typename T>
auto whenAny(std::vector<Promise<T>>& promises) -> std::shared_ptr<Promise<T>> {
    auto resultPromise = std::make_shared<Promise<T>>();

    if (promises.empty()) {
        resultPromise->setException(std::make_exception_ptr(
            std::invalid_argument("Empty promises vector")));
        return resultPromise;
    }

    struct SharedState {
        std::mutex mutex;
        bool resolved = false;
        std::shared_ptr<Promise<T>> resultPromise;

        explicit SharedState(std::shared_ptr<Promise<T>> promise)
            : resultPromise(std::move(promise)) {}
    };

    auto state = std::make_shared<SharedState>(resultPromise);

    for (auto& promise : promises) {
        promise.onComplete([state](T value) {
            std::unique_lock lock(state->mutex);
            if (!state->resolved) {
                state->resolved = true;
                state->resultPromise->setValue(std::move(value));
            }
        });
    }

    return resultPromise;
}

/**
 * @brief void specialization for whenAny
 */
inline auto whenAny(std::vector<Promise<void>>& promises)
    -> std::shared_ptr<Promise<void>> {
    auto resultPromise = std::make_shared<Promise<void>>();

    if (promises.empty()) {
        resultPromise->setException(std::make_exception_ptr(
            std::invalid_argument("Empty promises vector")));
        return resultPromise;
    }

    struct SharedState {
        std::mutex mutex;
        bool resolved = false;
        std::shared_ptr<Promise<void>> resultPromise;

        explicit SharedState(std::shared_ptr<Promise<void>> promise)
            : resultPromise(std::move(promise)) {}
    };

    auto state = std::make_shared<SharedState>(resultPromise);

    for (auto& promise : promises) {
        promise.onComplete([state]() {
            std::unique_lock lock(state->mutex);
            if (!state->resolved) {
                state->resolved = true;
                state->resultPromise->setValue();
            }
        });
    }

    return resultPromise;
}

/**
 * @brief Create a promise that resolves after a delay
 * @param duration The delay duration
 * @return A promise that resolves after the specified delay
 */
template <typename Rep, typename Period>
auto delay(std::chrono::duration<Rep, Period> duration) -> Promise<void> {
    Promise<void> promise;
    promise.runAsync([duration]() { std::this_thread::sleep_for(duration); });
    return promise;
}

/**
 * @brief Retry an async operation with exponential backoff
 * @tparam T The result type
 * @tparam F The function type
 * @param func The function to retry (should return T)
 * @param maxRetries Maximum number of retries
 * @param initialDelay Initial delay between retries
 * @return A promise with the result
 */
template <typename T, typename F>
    requires std::invocable<F> &&
             std::convertible_to<std::invoke_result_t<F>, T>
auto retry(F&& func, size_t maxRetries, std::chrono::milliseconds initialDelay)
    -> Promise<T> {
    Promise<T> resultPromise;

    resultPromise.runAsync(
        [func = std::forward<F>(func), maxRetries, initialDelay]() -> T {
            std::chrono::milliseconds currentDelay = initialDelay;

            for (size_t attempt = 0; attempt <= maxRetries; ++attempt) {
                try {
                    return func();
                } catch (...) {
                    if (attempt == maxRetries) {
                        throw;  // Rethrow on final attempt
                    }
                    std::this_thread::sleep_for(currentDelay);
                    currentDelay = std::chrono::milliseconds(
                        static_cast<long long>(currentDelay.count() * 2));
                }
            }
            throw std::runtime_error("Retry exhausted");  // Should not reach
        });

    return resultPromise;
}

/**
 * @brief Retry for void-returning functions
 */
template <typename F>
    requires std::invocable<F> && std::is_void_v<std::invoke_result_t<F>>
auto retryVoid(F&& func, size_t maxRetries,
               std::chrono::milliseconds initialDelay) -> Promise<void> {
    Promise<void> resultPromise;

    resultPromise.runAsync(
        [func = std::forward<F>(func), maxRetries, initialDelay]() {
            std::chrono::milliseconds currentDelay = initialDelay;

            for (size_t attempt = 0; attempt <= maxRetries; ++attempt) {
                try {
                    func();
                    return;  // Success
                } catch (...) {
                    if (attempt == maxRetries) {
                        throw;  // Rethrow on final attempt
                    }
                    std::this_thread::sleep_for(currentDelay);
                    currentDelay = std::chrono::milliseconds(
                        static_cast<long long>(currentDelay.count() * 2));
                }
            }
        });

    return resultPromise;
}

/**
 * @brief Create a promise that races against a timeout
 * @tparam T The value type
 * @param promise The promise to race
 * @param timeout The timeout duration
 * @return A promise that resolves with the value or rejects on timeout
 */
template <typename T, typename Rep, typename Period>
auto withTimeout(Promise<T>& promise,
                 std::chrono::duration<Rep, Period> timeout)
    -> std::shared_ptr<Promise<T>> {
    auto resultPromise = std::make_shared<Promise<T>>();

    struct SharedState {
        std::mutex mutex;
        bool resolved = false;
        std::shared_ptr<Promise<T>> resultPromise;

        explicit SharedState(std::shared_ptr<Promise<T>> promise)
            : resultPromise(std::move(promise)) {}
    };

    auto state = std::make_shared<SharedState>(resultPromise);

    // Set up timeout
    detail::dispatchAsync([state, timeout]() {
        std::this_thread::sleep_for(timeout);
        std::unique_lock lock(state->mutex);
        if (!state->resolved) {
            state->resolved = true;
            state->resultPromise->setException(std::make_exception_ptr(
                std::runtime_error("Promise timed out")));
        }
    });

    // Set up value callback
    promise.onComplete([state](T value) {
        std::unique_lock lock(state->mutex);
        if (!state->resolved) {
            state->resolved = true;
            state->resultPromise->setValue(std::move(value));
        }
    });

    return resultPromise;
}

}  // namespace atom::async

#endif  // ATOM_ASYNC_CORE_PROMISE_UTILS_HPP
