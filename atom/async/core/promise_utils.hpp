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
#include <type_traits>
#include <vector>

#include "promise_fwd.hpp"

namespace atom::async {

/**
 * @brief Create a resolved promise with a value
 * @tparam T The value type
 * @param value The value to resolve with
 * @return A promise that is already resolved
 */
template <typename T>
auto makeResolvedPromise(T&& value) -> Promise<std::decay_t<T>>;

/**
 * @brief Create a rejected promise with an exception
 * @tparam T The value type
 * @param ex The exception to reject with
 * @return A promise that is already rejected
 */
template <typename T>
auto makeRejectedPromise(std::exception_ptr ex) -> Promise<T>;

/**
 * @brief Wait for all promises to complete
 * @tparam T The value type
 * @param promises Vector of promises to wait for
 * @return A promise that resolves when all input promises resolve
 */
template <typename T>
auto whenAll(std::vector<Promise<T>>& promises) -> Promise<std::vector<T>>;

/**
 * @brief Wait for any promise to complete
 * @tparam T The value type
 * @param promises Vector of promises to wait for
 * @return A promise that resolves when any input promise resolves
 */
template <typename T>
auto whenAny(std::vector<Promise<T>>& promises) -> Promise<T>;

/**
 * @brief Create a promise that resolves after a delay
 * @param duration The delay duration
 * @return A promise that resolves after the specified delay
 */
template <typename Rep, typename Period>
auto delay(std::chrono::duration<Rep, Period> duration) -> Promise<void>;

/**
 * @brief Retry an async operation with exponential backoff
 * @tparam T The result type
 * @tparam F The function type
 * @param func The function to retry
 * @param maxRetries Maximum number of retries
 * @param initialDelay Initial delay between retries
 * @return A promise with the result
 */
template <typename T, typename F>
auto retry(F&& func, size_t maxRetries,
           std::chrono::milliseconds initialDelay) -> Promise<T>;

}  // namespace atom::async

// Include the main promise header for implementations
#include "promise.hpp"

#endif  // ATOM_ASYNC_CORE_PROMISE_UTILS_HPP
