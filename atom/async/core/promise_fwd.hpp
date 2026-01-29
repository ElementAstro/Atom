/*
 * promise_fwd.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-01-01

Description: Forward declarations for Promise types

**************************************************/

#ifndef ATOM_ASYNC_CORE_PROMISE_FWD_HPP
#define ATOM_ASYNC_CORE_PROMISE_FWD_HPP

#include <exception>
#include <functional>

namespace atom::async {

// Forward declarations
template <typename T>
class Promise;

template <typename T>
class EnhancedFuture;

template <typename T>
class PromiseAwaiter;

// Exception forward declaration
class PromiseCancelledException;

// Common type aliases
template <typename T>
using PromiseCallback = std::function<void(T)>;

using VoidCallback = std::function<void()>;
using ErrorCallback = std::function<void(std::exception_ptr)>;

}  // namespace atom::async

#endif  // ATOM_ASYNC_CORE_PROMISE_FWD_HPP
