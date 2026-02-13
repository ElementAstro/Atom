/*
 * platform_dispatch.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-01-01

Description: Platform-specific async dispatch utilities for coroutines

**************************************************/

#ifndef ATOM_ASYNC_CORE_DETAIL_PLATFORM_DISPATCH_HPP
#define ATOM_ASYNC_CORE_DETAIL_PLATFORM_DISPATCH_HPP

#include <coroutine>
#include <future>
#include <thread>
#include <utility>

#include "atom/macro.hpp"

#if defined(ATOM_PLATFORM_WINDOWS)
#include "../../../../cmake/WindowsCompat.hpp"
#elif defined(ATOM_PLATFORM_APPLE)
#include <dispatch/dispatch.h>
#elif defined(ATOM_PLATFORM_LINUX)
#include <pthread.h>
#endif

#ifdef ATOM_USE_ASIO
#include <asio/post.hpp>
#include <asio/thread_pool.hpp>
#endif

namespace atom::async::detail {

#ifdef ATOM_USE_ASIO
/**
 * @brief Get the global ASIO thread pool instance
 * @return Reference to the ASIO thread pool
 */
inline asio::thread_pool& getAsioThreadPool() {
    static asio::thread_pool pool(
        std::max(1u, std::thread::hardware_concurrency() > 0
                         ? std::thread::hardware_concurrency()
                         : 2));
    return pool;
}
#endif

/**
 * @brief Platform-optimized async dispatch for coroutine suspension
 *
 * This function dispatches work to the most efficient thread mechanism
 * available on the current platform:
 * - ASIO thread pool (if ATOM_USE_ASIO is defined)
 * - Windows: CreateThread
 * - macOS: Grand Central Dispatch (dispatch_async_f)
 * - Linux: pthread_create
 * - Fallback: std::jthread
 *
 * @tparam Future The shared_future type to wait on
 * @param future The future to wait for
 * @param handle The coroutine handle to resume after the future is ready
 */
template <typename Future>
void dispatchAwaitSuspend(Future future, std::coroutine_handle<> handle) {
#ifdef ATOM_USE_ASIO
    asio::post(getAsioThreadPool(), [future = std::move(future),
                                     h = handle]() mutable {
        future.wait();
        h.resume();
    });

#elif defined(ATOM_PLATFORM_WINDOWS)
    // Windows thread pool optimization using CreateThread
    struct ThreadParams {
        Future future;
        std::coroutine_handle<> handle;

        ThreadParams(Future f, std::coroutine_handle<> h)
            : future(std::move(f)), handle(h) {}
    };

    auto threadProc = [](void* data) -> unsigned long {
        auto* params = static_cast<ThreadParams*>(data);
        params->future.wait();
        params->handle.resume();
        delete params;
        return 0;
    };

    auto* params = new ThreadParams(std::move(future), handle);
    HANDLE threadHandle = CreateThread(nullptr, 0, threadProc, params, 0, nullptr);
    if (threadHandle) {
        CloseHandle(threadHandle);
    } else {
        // Handle thread creation failure - resume immediately
        delete params;
        if (handle) {
            handle.resume();
        }
    }

#elif defined(ATOM_PLATFORM_APPLE)
    // macOS Grand Central Dispatch optimization
    struct DispatchParams {
        Future future;
        std::coroutine_handle<> handle;

        DispatchParams(Future f, std::coroutine_handle<> h)
            : future(std::move(f)), handle(h) {}

        static void execute(void* context) {
            auto* params = static_cast<DispatchParams*>(context);
            params->future.wait();
            params->handle.resume();
            delete params;
        }
    };

    auto* params = new DispatchParams(std::move(future), handle);
    dispatch_async_f(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        params, DispatchParams::execute);

#elif defined(ATOM_PLATFORM_LINUX)
    // Linux pthread optimization
    struct ThreadParams {
        Future future;
        std::coroutine_handle<> handle;

        ThreadParams(Future f, std::coroutine_handle<> h)
            : future(std::move(f)), handle(h) {}
    };

    auto* params = new ThreadParams(std::move(future), handle);
    pthread_t thread;
    int result = pthread_create(
        &thread, nullptr,
        [](void* data) -> void* {
            auto* p = static_cast<ThreadParams*>(data);
            p->future.wait();
            p->handle.resume();
            delete p;
            return nullptr;
        },
        params);

    if (result == 0) {
        pthread_detach(thread);
    } else {
        // Handle thread creation failure
        delete params;
        if (handle) {
            handle.resume();
        }
    }

#else
    // Standard C++20 fallback using jthread
    std::jthread([future = std::move(future), h = handle]() mutable {
        future.wait();
        h.resume();
    }).detach();
#endif
}

/**
 * @brief Platform-optimized async execution for Promise::runAsync
 *
 * Executes a callable asynchronously using platform-specific optimizations.
 *
 * @tparam Callable The callable type (function, lambda, etc.)
 * @param callable The callable to execute
 */
template <typename Callable>
void dispatchAsync(Callable&& callable) {
#ifdef ATOM_USE_ASIO
    asio::post(getAsioThreadPool(), std::forward<Callable>(callable));

#elif defined(ATOM_PLATFORM_WINDOWS)
    struct ThreadData {
        std::decay_t<Callable> func;

        explicit ThreadData(Callable&& f) : func(std::forward<Callable>(f)) {}

        static unsigned long WINAPI ThreadProc(void* param) {
            auto* data = static_cast<ThreadData*>(param);
            try {
                data->func();
            } catch (...) {
                // Exceptions should be handled by the callable itself
            }
            delete data;
            return 0;
        }
    };

    auto* threadData = new ThreadData(std::forward<Callable>(callable));
    HANDLE threadHandle =
        CreateThread(nullptr, 0, ThreadData::ThreadProc, threadData, 0, nullptr);
    if (threadHandle) {
        CloseHandle(threadHandle);
    } else {
        delete threadData;
    }

#elif defined(ATOM_PLATFORM_APPLE)
    struct DispatchData {
        std::decay_t<Callable> func;

        explicit DispatchData(Callable&& f) : func(std::forward<Callable>(f)) {}

        static void Execute(void* context) {
            auto* data = static_cast<DispatchData*>(context);
            try {
                data->func();
            } catch (...) {
                // Exceptions should be handled by the callable itself
            }
            delete data;
        }
    };

    auto* dispatchData = new DispatchData(std::forward<Callable>(callable));
    dispatch_async_f(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        dispatchData, DispatchData::Execute);

#else
    // Standard C++20 fallback
    std::jthread([func = std::forward<Callable>(callable)]() mutable {
        try {
            func();
        } catch (...) {
            // Exceptions should be handled by the callable itself
        }
    }).detach();
#endif
}

/**
 * @brief Get the number of available processors
 * @return Number of processors/cores available
 */
inline size_t getProcessorCount() noexcept {
#if defined(ATOM_PLATFORM_WINDOWS)
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return static_cast<size_t>(sysInfo.dwNumberOfProcessors);
#elif defined(ATOM_PLATFORM_LINUX)
    // Use sysconf for Linux
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    return nprocs > 0 ? static_cast<size_t>(nprocs) : 1;
#else
    size_t count = std::thread::hardware_concurrency();
    return count > 0 ? count : 1;
#endif
}

}  // namespace atom::async::detail

#endif  // ATOM_ASYNC_CORE_DETAIL_PLATFORM_DISPATCH_HPP
